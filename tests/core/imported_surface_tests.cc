// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
thread_local size_t allocationCountdown = SIZE_MAX;
thread_local size_t allocationsObserved = 0;
thread_local bool isCountingAllocations = false;
thread_local bool hasInjectedAllocationFailure = false;
}
void* operator new(size_t size)
{
    if (isCountingAllocations)
        ++allocationsObserved;
    if (allocationCountdown != SIZE_MAX && allocationCountdown-- == 0) {
        allocationCountdown = SIZE_MAX;
        hasInjectedAllocationFailure = true;
        throw std::bad_alloc();
    }
    if (void* value = std::malloc(std::max(size, size_t(1))))
        return value;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, size_t) noexcept { std::free(value); }
void operator delete[](void* value, size_t) noexcept { std::free(value); }
void* operator new(size_t size, const std::nothrow_t&) noexcept
{
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](size_t size, const std::nothrow_t& tag) noexcept { return ::operator new(size, tag); }
void operator delete(void* value, const std::nothrow_t&) noexcept { std::free(value); }
void operator delete[](void* value, const std::nothrow_t&) noexcept { std::free(value); }

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
using Owner = std::unique_ptr<Dust3DImportedSurfaceMesh, decltype(&dust3d_imported_surface_mesh_destroy)>;
Owner load(const std::vector<uint8_t>& bytes, size_t budget = 0)
{
    Dust3DImportedSurfaceOptions options {};
    options.struct_size = sizeof(options);
    options.maximum_payload_bytes = budget;
    Dust3DError error {};
    Owner owner(dust3d_imported_surface_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_imported_surface_mesh_destroy);
    require(owner && error.status == DUST3D_STATUS_OK, error.message);
    return owner;
}
// Deliberately narrow writer for this synthetic one-model fixture, independent of the native reader.
std::vector<uint8_t> withXml(const std::vector<uint8_t>& bytes, const std::string& xml)
{
    const std::string text(bytes.begin(), bytes.end());
    const auto start = text.find("glTF");
    require(start != std::string::npos && bytes.size() - start >= 12, "Synthetic GLB asset is missing");
    uint32_t length = 0;
    for (size_t index = 0; index < 4; ++index)
        length |= uint32_t(bytes[start + 8 + index]) << (index * 8);
    require(length == bytes.size() - start, "Synthetic fixture must contain one final GLB asset");
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size())
        + "'/><asset name='models/{00000300-0000-4000-8000-000000000300}.glb' offset='" + std::to_string(xml.size())
        + "' size='" + std::to_string(length) + "'/></ds3>";
    std::ostringstream envelope;
    envelope << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
             << header << xml;
    const auto serialized = envelope.str();
    std::vector<uint8_t> result(serialized.begin(), serialized.end());
    result.insert(result.end(), bytes.begin() + start, bytes.end());
    return result;
}
std::string fixtureXml(const std::vector<uint8_t>& bytes)
{
    const std::string text(bytes.begin(), bytes.end());
    const auto start = text.find("<canvas"), end = text.find("</canvas>");
    require(start != std::string::npos && end != std::string::npos && end >= start, "Synthetic model XML is missing");
    return text.substr(start, end + 9 - start);
}
void insertBefore(std::string& xml, const std::string& marker, const std::string& value)
{
    const auto offset = xml.find(marker);
    require(offset != std::string::npos, "Synthetic fixture mutation target is missing");
    xml.insert(offset, value);
}
void verifyMirrorAndFilteredTriangleAlignment(const std::vector<uint8_t>& bytes)
{
    auto reference = load(bytes);
    const auto* referenceGeometry = dust3d_imported_surface_mesh_geometry(reference.get());
    const auto* referenceRecords = dust3d_imported_surface_mesh_triangles(reference.get());
    auto mirroredXml = fixtureXml(bytes);
    const auto partOffset = mirroredXml.find("<part ");
    require(partOffset != std::string::npos, "Synthetic part missing");
    mirroredXml.insert(partOffset + 6, "xMirrored='true' ");
    const auto componentOffset = mirroredXml.find("<component ");
    require(componentOffset != std::string::npos, "Synthetic component missing");
    mirroredXml.insert(componentOffset + 11, "combineMode='Uncombined' ");
    auto mirrored = load(withXml(bytes, mirroredXml));
    require(dust3d_imported_surface_mesh_triangle_count(mirrored.get()) == 8
            && dust3d_imported_surface_mesh_component_count(mirrored.get()) == 2,
        "Mirror must retain two classified surface instances");
    const auto* mirrorRecords = dust3d_imported_surface_mesh_triangles(mirrored.get());
    const auto* mirrorVertices = dust3d_mesh_vertices(dust3d_imported_surface_mesh_geometry(mirrored.get()));
    size_t originalCount = 0, generatedCount = 0;
    for (size_t face = 0; face < 8; ++face) {
        const auto& record = mirrorRecords[face];
        const auto kind = dust3d_imported_surface_mesh_component_kind(mirrored.get(), record.component_index);
        require(record.uv_known == 1 && record.model_index == 0
                && (kind == DUST3D_SURFACE_COMPONENT_ORIGINAL || kind == DUST3D_SURFACE_COMPONENT_GENERATED),
            "Mirror lost source UV/model or catalog classification");
        const bool generated = kind == DUST3D_SURFACE_COMPONENT_GENERATED;
        generated ? ++generatedCount : ++originalCount;
        for (size_t corner = 0; corner < 3; ++corner) {
            size_t sourceCorner = 12;
            for (size_t index = 0; index < 12; ++index)
                if (record.uv[corner][0] == referenceRecords[index / 3].uv[index % 3][0]
                    && record.uv[corner][1] == referenceRecords[index / 3].uv[index % 3][1])
                    sourceCorner = index;
            require(sourceCorner < 12, "Mirror invented source UV coordinates");
            for (size_t channel = 0; channel < 4; ++channel)
                require(record.color[corner][channel] == referenceRecords[sourceCorner / 3].color[sourceCorner % 3][channel],
                    "Mirror mismatched source color/alpha and UV corners");
            for (size_t axis = 0; axis < 3; ++axis) {
                const float original = dust3d_mesh_vertices(referenceGeometry)[sourceCorner].position[axis];
                const float expected = generated && axis == 0 ? -original : original;
                require(std::abs(mirrorVertices[face * 3 + corner].position[axis] - expected) < 1e-7,
                    "Mirror source attributes refer to a different reflected vertex");
            }
        }
    }
    require(originalCount == 4 && generatedCount == 4, "Mirror classification did not exercise both instances");
    auto filteredXml = fixtureXml(bytes);
    const auto sourceComponent = filteredXml.find("<component ");
    require(sourceComponent != std::string::npos, "Synthetic source component missing");
    filteredXml.insert(sourceComponent + 11, "combineMode='Uncombined' ");
    const std::string part = "{00000101-0000-4000-8000-000000000101}";
    const std::string first = "{00001002-0000-4000-8000-000000001002}";
    const std::string second = "{00001003-0000-4000-8000-000000001003}";
    insertBefore(filteredXml, "</nodes>", "<node id='" + first + "' partId='" + part + "' x='1000' y='1000' z='1000' radius='.000001'/>" + "<node id='" + second + "' partId='" + part + "' x='1000' y='1000.000001' z='1000' radius='.000001'/>");
    insertBefore(filteredXml, "</edges>", "<edge id='{00002001-0000-4000-8000-000000002001}' partId='" + part + "' from='" + first + "' to='" + second + "'/>");
    insertBefore(filteredXml, "</parts>", "<part id='" + part + "' target='ImportedModel' importedModelId='{00000300-0000-4000-8000-000000000300}' interpolated='false'/>");
    // The collapsed component precedes the valid one in actual uncombined traversal.
    insertBefore(filteredXml, "<component ", "<component id='{00000201-0000-4000-8000-000000000201}' linkDataType='partId' linkData='" + part + "' combineMode='Uncombined'/>");
    auto filtered = load(withXml(bytes, filteredXml));
    const auto* geometry = dust3d_imported_surface_mesh_geometry(filtered.get());
    require(dust3d_mesh_vertex_count(geometry) == 12 && dust3d_imported_surface_mesh_triangle_count(filtered.get()) == 4,
        "Float-collapsed imported triangles must not leave stale source records");
    const auto* records = dust3d_imported_surface_mesh_triangles(filtered.get());
    for (size_t face = 0; face < 4; ++face) {
        require(records[face].uv_known == 1 && records[face].model_index == 0,
            "Filtering shifted source UV/model records");
        require(std::strcmp(dust3d_imported_surface_mesh_component_id(filtered.get(), records[face].component_index),
                    "{00000200-0000-4000-8000-000000000200}")
                == 0,
            "Filtering left a source record for a collapsed component");
        for (size_t corner = 0; corner < 3; ++corner) {
            for (size_t axis = 0; axis < 2; ++axis)
                require(records[face].uv[corner][axis] == referenceRecords[face].uv[corner][axis],
                    "Skipping a collapsed prefix shifted surviving UV corners");
            for (size_t channel = 0; channel < 4; ++channel)
                require(records[face].color[corner][channel] == referenceRecords[face].color[corner][channel],
                    "Skipping a collapsed prefix shifted surviving RGBA corners");
        }
    }
    const size_t extraReserved = 4 * (3 * sizeof(Dust3DVertex) + sizeof(Dust3DImportedSurfaceTriangle));
    require(dust3d_imported_surface_mesh_payload_bytes(filtered.get()) >= dust3d_imported_surface_mesh_payload_bytes(reference.get()) + extraReserved,
        "Filtered records must still charge retained pre-export buffer capacity");
}
void verifyLateAllocationFailures(const std::vector<uint8_t>& bytes)
{
    Dust3DImportedSurfaceOptions options { sizeof(Dust3DImportedSurfaceOptions), 0 };
    Dust3DError error {};
    allocationsObserved = 0;
    isCountingAllocations = true;
    auto* pointer = dust3d_imported_surface_mesh_load(bytes.data(), bytes.size(), &options, &error);
    isCountingAllocations = false;
    const size_t successfulCount = allocationsObserved;
    Owner baseline(pointer, dust3d_imported_surface_mesh_destroy);
    require(baseline && error.status == DUST3D_STATUS_OK && successfulCount >= 32, "Surface allocation baseline was not exercised");
    for (size_t tail = 0; tail < 32; ++tail) {
        hasInjectedAllocationFailure = false;
        allocationCountdown = successfulCount - 1 - tail;
        auto* failed = dust3d_imported_surface_mesh_load(bytes.data(), bytes.size(), &options, &error);
        allocationCountdown = SIZE_MAX;
        Owner rejected(failed, dust3d_imported_surface_mesh_destroy);
        require(hasInjectedAllocationFailure && !rejected && error.status == DUST3D_STATUS_LIMIT_EXCEEDED
                && std::strcmp(error.message, std::bad_alloc().what()) == 0,
            "Late surface allocation failure crossed C or lost its original cause");
    }
    require(dust3d_imported_surface_mesh_triangles(baseline.get())[0].uv_known == 1,
        "Failed later loads invalidated a published surface owner");
}
void verifyProceduralSurface(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "Procedural fixture must be readable");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    auto owner = load(bytes);
    const auto count = dust3d_imported_surface_mesh_triangle_count(owner.get());
    require(count > 0 && dust3d_imported_surface_mesh_model_count(owner.get()) == 0
            && dust3d_mesh_vertex_count(dust3d_imported_surface_mesh_geometry(owner.get())) == count * 3,
        "Procedural-only geometry must retain aligned source records without imported models");
    const auto* records = dust3d_imported_surface_mesh_triangles(owner.get());
    for (size_t face = 0; face < count; ++face) {
        require(records[face].color_encoding == DUST3D_SURFACE_COLOR_SRGB, "Document fallback RGB must retain its sRGB encoding");
        require(records[face].model_index == SIZE_MAX && records[face].uv_known == 0,
            "Procedural geometry invented an imported model or authored UV mapping");
        for (size_t corner = 0; corner < 3; ++corner)
            require(records[face].uv[corner][0] == 0 && records[face].uv[corner][1] == 0,
                "Unknown imported UVs must be zero");
    }
}
void verifyImportedFallbackColorEncoding(const std::vector<uint8_t>& bytes)
{
    auto noVertexColors = bytes;
    const std::string text(bytes.begin(), bytes.end());
    const std::string attribute = ",\"COLOR_0\":3";
    const auto offset = text.find(attribute);
    require(offset != std::string::npos, "Synthetic color attribute missing");
    std::fill(noVertexColors.begin() + offset, noVertexColors.begin() + offset + attribute.size(), ' ');
    auto owner = load(noVertexColors);
    const auto* records = dust3d_imported_surface_mesh_triangles(owner.get());
    for (size_t face = 0; face < 4; ++face) {
        require(records[face].model_index == 0 && records[face].color_encoding == DUST3D_SURFACE_COLOR_SRGB,
            "An imported model without COLOR_0 must retain document fallback color encoding");
        for (size_t corner = 0; corner < 3; ++corner)
            require(records[face].color[corner][0] == 143.0 / 255 && records[face].color[corner][1] == 197.0 / 255
                    && records[face].color[corner][2] == 220.0 / 255 && records[face].color[corner][3] == 1,
                "Fallback color values were converted, modulated or premultiplied during transfer");
    }
}
void verifyRepeatedAndNonfiniteSourceUvs(const std::vector<uint8_t>& bytes)
{
    auto modified = bytes;
    const std::string text(bytes.begin(), bytes.end());
    const auto glb = text.find("glTF");
    require(glb != std::string::npos && bytes.size() - glb > 20, "Synthetic GLB header missing");
    uint32_t jsonLength = 0;
    for (size_t index = 0; index < 4; ++index)
        jsonLength |= uint32_t(bytes[glb + 12 + index]) << (8 * index);
    const std::string json(text.substr(glb + 20, jsonLength));
    require(json.find("\"byteOffset\":72,\"byteLength\":32") != std::string::npos
            && json.find("\"TEXCOORD_0\":2") != std::string::npos,
        "Synthetic source UV layout changed");
    const size_t firstUv = glb + 20 + jsonLength + 8 + 72;
    require(firstUv <= modified.size() && modified.size() - firstUv >= 8, "Synthetic UV bytes missing");
    const auto writeBits = [&](size_t offset, uint32_t bits) {
        for (size_t index = 0; index < 4; ++index)
            modified[offset + index] = static_cast<uint8_t>(bits >> (8 * index));
    };
    writeBits(firstUv, 0xc0000000); // Authored repeat coordinate -2, independent of host endianness.
    writeBits(firstUv + 4, 0x40400000); // Authored repeat coordinate 3.
    auto owner = load(modified);
    const auto* triangles = dust3d_imported_surface_mesh_triangles(owner.get());
    size_t repeatedCorners = 0;
    for (size_t face = 0; face < dust3d_imported_surface_mesh_triangle_count(owner.get()); ++face)
        for (size_t corner = 0; corner < 3; ++corner)
            if (triangles[face].uv[corner][0] == -2 && triangles[face].uv[corner][1] == 3) {
                ++repeatedCorners;
                require(triangles[face].uv_known && triangles[face].model_index == 0
                        && triangles[face].color[corner][0] == 1
                        && triangles[face].color[corner][3] == 128.0 / 255,
                    "Repeated UV coordinates lost their source color/model association");
            }
    require(repeatedCorners == 3, "Authored repeat UVs must remain outside the packed atlas range");
    writeBits(firstUv, 0x7fc00000);
    Dust3DImportedSurfaceOptions options { sizeof(Dust3DImportedSurfaceOptions), 0 };
    Dust3DError error {};
    Owner rejected(dust3d_imported_surface_mesh_load(modified.data(), modified.size(), &options, &error), dust3d_imported_surface_mesh_destroy);
    require(!rejected && error.status == DUST3D_STATUS_INVALID_DOCUMENT
            && std::strstr(error.message, "nonfinite or out-of-range attribute"),
        "Nonfinite source UV must preserve the decoder's typed original cause");
    require(dust3d_imported_surface_mesh_triangles(owner.get()) == triangles && triangles[0].uv_known,
        "Rejected later loads must not invalidate a published source owner");
}
void verifyOwnedSourceAttributes(std::vector<uint8_t> bytes)
{
    const auto original = bytes;
    auto owner = load(bytes);
    const auto* geometry = dust3d_imported_surface_mesh_geometry(owner.get());
    const auto count = dust3d_imported_surface_mesh_triangle_count(owner.get());
    const auto* triangles = dust3d_imported_surface_mesh_triangles(owner.get());
    require(geometry && count == 4 && dust3d_mesh_vertex_count(geometry) == count * 3 && triangles,
        "Imported source records must align with actual exported geometry");
    require(dust3d_imported_surface_mesh_model_count(owner.get()) == 1
            && std::strcmp(dust3d_imported_surface_mesh_model_id(owner.get(), 0), "{00000300-0000-4000-8000-000000000300}") == 0,
        "Resolved GLB model identity was lost");
    std::array<size_t, 4> alphaCounts {};
    const std::array<std::array<double, 3>, 4> colors { { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 0 } } };
    const std::array<double, 4> alpha { 128.0 / 255, 1, 64.0 / 255, 192.0 / 255 };
    const std::array<std::array<double, 2>, 4> uv { { { 0, 0 }, { 1, 0 }, { .5, 1 }, { .5, .5 } } };
    for (size_t face = 0; face < count; ++face) {
        const auto& triangle = triangles[face];
        require(triangle.uv_known == 1 && triangle.model_index == 0, "Imported UV/model mapping was lost");
        require(triangle.color_encoding == DUST3D_SURFACE_COLOR_LINEAR, "glTF vertex colors must retain their linear encoding");
        require(dust3d_imported_surface_mesh_component_kind(owner.get(), triangle.component_index) == DUST3D_SURFACE_COMPONENT_ORIGINAL,
            "Original imported component classification was lost");
        require(std::strcmp(dust3d_imported_surface_mesh_component_id(owner.get(), triangle.component_index),
                    "{00000200-0000-4000-8000-000000000200}")
                == 0,
            "Opaque component identity changed");
        for (size_t corner = 0; corner < 3; ++corner) {
            size_t source = 4;
            for (size_t index = 0; index < 4; ++index)
                if (triangle.uv[corner][0] == uv[index][0] && triangle.uv[corner][1] == uv[index][1])
                    source = index;
            require(source < 4 && std::abs(triangle.color[corner][3] - alpha[source]) < 1e-12,
                "Straight authored alpha was lost or multiplied by the material factor");
            for (size_t channel = 0; channel < 3; ++channel)
                require(triangle.color[corner][channel] == colors[source][channel],
                    "Source RGB was premultiplied or modulated by the material factor");
            ++alphaCounts[source];
            require(dust3d_mesh_vertices(geometry)[face * 3 + corner].color[3] == 1,
                "Ordinary preview opacity policy changed");
        }
    }
    require(alphaCounts == std::array<size_t, 4> { 3, 3, 3, 3 }, "Source corner identity/alpha alignment changed");
    std::fill(bytes.begin(), bytes.end(), 0);
    {
        auto subsequent = load(original);
    }
    require(dust3d_imported_surface_mesh_triangles(owner.get()) == triangles && triangles[0].uv_known == 1,
        "Input overwrite or another owner invalidated borrowed views");
    const auto exactBytes = dust3d_imported_surface_mesh_payload_bytes(owner.get());
    auto exact = load(original, exactBytes);
    require(dust3d_imported_surface_mesh_payload_bytes(exact.get()) == exactBytes, "Exact owned payload budget must fit");
    Dust3DImportedSurfaceOptions shortOptions { sizeof(Dust3DImportedSurfaceOptions), exactBytes - 1 };
    Dust3DError error {};
    Owner rejected(dust3d_imported_surface_mesh_load(original.data(), original.size(), &shortOptions, &error), dust3d_imported_surface_mesh_destroy);
    require(!rejected && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && error.message[0], "One byte below retained ownership must reject");
    require(!dust3d_imported_surface_mesh_model_id(owner.get(), SIZE_MAX)
            && !dust3d_imported_surface_mesh_component_id(owner.get(), SIZE_MAX)
            && dust3d_imported_surface_mesh_component_kind(owner.get(), SIZE_MAX) == DUST3D_SURFACE_COMPONENT_UNKNOWN,
        "Out-of-range catalog queries must be safe");
    for (size_t size : { size_t(0), sizeof(Dust3DImportedSurfaceOptions) - 1 }) {
        Dust3DImportedSurfaceOptions invalid { size, 0 };
        Owner failed(dust3d_imported_surface_mesh_load(original.data(), original.size(), &invalid, &error), dust3d_imported_surface_mesh_destroy);
        require(!failed && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0], "Invalid options size must retain a typed cause");
    }
    Dust3DImportedSurfaceOptions oversized { sizeof(Dust3DImportedSurfaceOptions), size_t(DUST3D_MAX_IMPORTED_SURFACE_PAYLOAD_BYTES) + 1 };
    Owner tooLarge(dust3d_imported_surface_mesh_load(original.data(), original.size(), &oversized, &error), dust3d_imported_surface_mesh_destroy);
    require(!tooLarge && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Oversized ownership option must reject");
    Owner missing(dust3d_imported_surface_mesh_load(original.data(), original.size(), nullptr, &error), dust3d_imported_surface_mesh_destroy);
    require(!missing && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Missing options must reject");
    Dust3DImportedSurfaceOptions defaults { sizeof(Dust3DImportedSurfaceOptions), 0 };
    Owner invalidInput(dust3d_imported_surface_mesh_load(nullptr, 0, &defaults, &error), dust3d_imported_surface_mesh_destroy);
    require(!invalidInput && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0], "Invalid archive must retain its original cause");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture directory required");
        std::ifstream file(std::string(argv[1]) + "/ImportedSurface.ds3", std::ios::binary);
        require(file.good(), "Imported surface fixture must be readable");
        const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        verifyOwnedSourceAttributes(bytes);
        verifyRepeatedAndNonfiniteSourceUvs(bytes);
        verifyMirrorAndFilteredTriangleAlignment(bytes);
        verifyImportedFallbackColorEncoding(bytes);
        verifyProceduralSurface(std::string(argv[1]) + "/Capsule.ds3");
        verifyLateAllocationFailures(bytes);
        require(!dust3d_imported_surface_mesh_geometry(nullptr) && !dust3d_imported_surface_mesh_triangles(nullptr)
                && !dust3d_imported_surface_mesh_triangle_count(nullptr) && !dust3d_imported_surface_mesh_model_count(nullptr)
                && !dust3d_imported_surface_mesh_component_count(nullptr) && !dust3d_imported_surface_mesh_payload_bytes(nullptr)
                && !dust3d_imported_surface_mesh_model_id(nullptr, 0) && !dust3d_imported_surface_mesh_component_id(nullptr, 0)
                && dust3d_imported_surface_mesh_component_kind(nullptr, 0) == DUST3D_SURFACE_COMPONENT_UNKNOWN,
            "Null imported-surface queries must be safe");
        dust3d_imported_surface_mesh_destroy(nullptr);
        std::cout << "Owned imported surface contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

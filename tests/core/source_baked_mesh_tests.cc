// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
using Owner = std::unique_ptr<Dust3DSourceBakedMesh, decltype(&dust3d_source_baked_mesh_destroy)>;
std::vector<uint8_t> read(const char* directory, const char* name)
{
    std::ifstream file(std::string(directory) + '/' + name, std::ios::binary);
    require(bool(file), "Missing native source bake fixture");
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
Dust3DSourceBakeOptions options()
{
    Dust3DSourceBakeOptions result {};
    result.struct_size = sizeof(result);
    result.legacy.struct_size = sizeof(result.legacy);
    result.legacy.atlas_texture_size = 256;
    result.legacy.color_size = result.legacy.material_size = 256;
    result.legacy.normal_size = 128;
    return result;
}
void nullAndMalformedOptions(const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    Owner absent(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), nullptr, &error), dust3d_source_baked_mesh_destroy);
    require(!absent && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Explicit source bake options required");
    auto settings = options();
    settings.struct_size = sizeof(settings) - 1;
    absent.reset(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error));
    require(!absent && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Sized source bake options checked");
    require(dust3d_source_baked_mesh_geometry(nullptr) == nullptr && dust3d_source_baked_mesh_triangles(nullptr) == nullptr
            && dust3d_source_baked_mesh_triangle_count(nullptr) == 0 && dust3d_source_baked_mesh_payload_bytes(nullptr) == 0
            && dust3d_source_baked_mesh_texture(nullptr, DUST3D_TEXTURE_COLOR).pixels == nullptr
            && dust3d_source_baked_mesh_model_count(nullptr) == 0 && dust3d_source_baked_mesh_component_count(nullptr) == 0
            && std::strlen(dust3d_source_baked_mesh_model_id(nullptr, 0)) == 0
            && std::strlen(dust3d_source_baked_mesh_component_id(nullptr, 0)) == 0,
        "Source bake accessors are null safe");
}
void importedOwnership(const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    auto settings = options();
    Owner missing(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(!missing && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && std::strlen(error.message), "Required decoded imported image must resolve explicitly");
    std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> assets(
        dust3d_imported_assets_load(bytes.data(), bytes.size(), nullptr, &error), dust3d_imported_assets_destroy);
    require(assets && dust3d_imported_assets_model_count(assets.get()) == 1, "Real imported material owner required");
    const auto material = dust3d_imported_assets_material(assets.get(), 0);
    std::string identity(material.model_id);
    std::vector<uint8_t> decoded { 128, 128, 128, 128 }; // Synthetic decoded pixels isolate the native image boundary.
    Dust3DModelBakeImage image { identity.data(), identity.size(), { 1, 1, 4, decoded.size(), decoded.data() } };
    settings.model_images = &image;
    settings.model_image_count = 1;
    Owner owner(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    if (!owner)
        throw std::runtime_error(error.message);
    require(error.status == DUST3D_STATUS_OK && dust3d_source_baked_mesh_triangle_count(owner.get()) == 4, "Real final source triangles aligned");
    require(dust3d_source_baked_mesh_model_count(owner.get()) == 1
            && std::strcmp(dust3d_source_baked_mesh_model_id(owner.get(), 0), identity.c_str()) == 0
            && dust3d_source_baked_mesh_component_count(owner.get()) == 1
            && std::strcmp(dust3d_source_baked_mesh_component_id(owner.get(), 0), "{00000200-0000-4000-8000-000000000200}") == 0
            && dust3d_source_baked_mesh_component_kind(owner.get(), 0) == DUST3D_SURFACE_COMPONENT_ORIGINAL,
        "Native bake catalogs preserve exact original model/component identities");
    const auto* geometry = dust3d_source_baked_mesh_geometry(owner.get());
    require(geometry && dust3d_mesh_vertex_count(geometry) == 12, "Source bake geometry ownership");
    const auto* triangles = dust3d_source_baked_mesh_triangles(owner.get());
    for (size_t index = 0; index < 4; ++index) {
        const auto& triangle = triangles[index];
        require(triangle.uv_known && triangle.policy_known && triangle.model_index == 0 && triangle.component_index != SIZE_MAX
                && triangle.alpha_mode == material.alpha_mode && triangle.alpha_cutoff == material.alpha_cutoff
                && triangle.is_double_sided == material.is_double_sided && triangle.tangent.known,
            "Source bake policies, tangents and source identity align with geometry");
        for (size_t corner = 0; corner < 3; ++corner) {
            for (size_t axis = 0; axis < 2; ++axis)
                require(std::isfinite(triangle.uv[corner][axis]) && triangle.uv[corner][axis] >= 0 && triangle.uv[corner][axis] <= 1, "Native packed source UVs normalized");
            const auto& vertex = dust3d_mesh_vertices(geometry)[index * 3 + corner];
            for (size_t channel = 0; channel < 4; ++channel)
                require(vertex.color[channel] == 1, "Fully factored baked colors require neutral vertex modulation");
        }
    }
    const auto metallic = dust3d_source_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_METALLIC);
    const auto roughness = dust3d_source_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_ROUGHNESS);
    const auto emissive = dust3d_source_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_EMISSIVE);
    require(metallic.pixels && metallic.pixels[0] == 32 && roughness.pixels && roughness.pixels[0] == 159,
        "Authored fixture metallic .125 and roughness .625 survive native resolution");
    require(emissive.pixels && emissive.pixels[0] == 89 && emissive.pixels[1] == 124 && emissive.pixels[2] == 149,
        "Authored fixture linear emissive (.1,.2,.3) encodes once");
    require(dust3d_source_baked_mesh_texture(owner.get(), 999).pixels == nullptr
            && std::strlen(dust3d_source_baked_mesh_model_id(owner.get(), 999)) == 0
            && std::strlen(dust3d_source_baked_mesh_component_id(owner.get(), 999)) == 0
            && dust3d_source_baked_mesh_component_kind(owner.get(), 999) == DUST3D_SURFACE_COMPONENT_UNKNOWN,
        "Native source bake rejects out-of-range view selectors");
    require(std::strstr(dust3d_mesh_warning(geometry), "materials, textures and transparency are not yet applied") == nullptr, "Successful source bake clears only obsolete preview notice");
    const auto texture = dust3d_source_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_COLOR);
    require(texture.width == 256 && texture.height == 256 && texture.byte_count == 256 * 256 * 4 && texture.pixels, "Owned native source map dimensions");
    const std::vector<uint8_t> copied(texture.pixels, texture.pixels + texture.byte_count);
    decoded.clear();
    decoded.shrink_to_fit();
    identity.clear();
    assets.reset();
    settings.model_images = nullptr;
    settings.model_image_count = 0;
    Owner subsequent(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(!subsequent && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET, "Subsequent failure cannot alter prior bake owner");
    require(std::equal(copied.begin(), copied.end(), texture.pixels), "Native source maps outlive caller inputs and subsequent calls");
    require(dust3d_source_baked_mesh_maximum_sampling_lod(owner.get()) == 0, "Native metadata preserves source atlas LOD restriction");
}

std::string fixtureXml(const std::vector<uint8_t>& bytes)
{
    const std::string text(bytes.begin(), bytes.end());
    const auto start = text.find("<canvas"), end = text.find("</canvas>");
    require(start != std::string::npos && end != std::string::npos && end >= start, "Missing synthetic XML");
    return text.substr(start, end + 9 - start);
}
// Independent envelope writer, limited to this synthetic fixture's final GLB asset.
std::vector<uint8_t> withXml(const std::vector<uint8_t>& bytes, const std::string& xml)
{
    const std::string text(bytes.begin(), bytes.end());
    const auto start = text.find("glTF");
    require(start != std::string::npos && bytes.size() - start >= 12, "Missing fixture GLB");
    uint32_t length = 0;
    for (size_t index = 0; index < 4; ++index)
        length |= uint32_t(bytes[start + 8 + index]) << (index * 8);
    require(length == bytes.size() - start, "Fixture GLB must be the last asset");
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size())
        + "'/><asset name='models/{00000300-0000-4000-8000-000000000300}.glb' offset='" + std::to_string(xml.size())
        + "' size='" + std::to_string(length) + "'/></ds3>";
    std::ostringstream output;
    output << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    const auto serialized = output.str();
    std::vector<uint8_t> result(serialized.begin(), serialized.end());
    result.insert(result.end(), bytes.begin() + start, bytes.end());
    return result;
}
void insertBefore(std::string& xml, const std::string& marker, const std::string& value)
{
    const auto offset = xml.find(marker);
    require(offset != std::string::npos, "Missing synthetic XML mutation target");
    xml.insert(offset, value);
}
struct DecodedFixture {
    std::string identity = "{00000300-0000-4000-8000-000000000300}";
    std::vector<uint8_t> pixels { 128, 128, 128, 128 };
    Dust3DModelBakeImage image() const { return { identity.data(), identity.size(), { 1, 1, 4, pixels.size(), pixels.data() } }; }
};
Owner load(const std::vector<uint8_t>& bytes, Dust3DSourceBakeOptions settings = options())
{
    DecodedFixture decoded;
    auto image = decoded.image();
    settings.model_images = &image;
    settings.model_image_count = 1;
    Dust3DError error {};
    Owner owner(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(owner && error.status == DUST3D_STATUS_OK, error.message);
    return owner;
}
void malformedImagesAndLimits(const std::vector<uint8_t>& bytes)
{
    DecodedFixture decoded;
    auto descriptor = decoded.image();
    auto settings = options();
    settings.model_images = &descriptor;
    settings.model_image_count = 1;
    auto rejects = [&](Dust3DStatus expected) {
        Dust3DError error {};
        Owner owner(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
        require(!owner && error.status == expected && error.message[0], "Native malformed source-bake input did not preserve its typed cause");
    };
    for (size_t width : { size_t(0), size_t(8193) }) {
        descriptor = decoded.image();
        descriptor.image.width = width;
        rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    }
    descriptor = decoded.image();
    descriptor.image.pixels = nullptr;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    settings.model_image_count = 10001;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    settings.model_image_count = 1;
    settings.maximum_chart_count = 100001;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    settings.maximum_chart_count = 0;
    settings.maximum_raster_samples = 1000000001;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    settings.maximum_raster_samples = 0;
    settings.maximum_sampler_payload_bytes = 256 * 1024 * 1024 + 1;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_sampler_payload_bytes = 0;
    descriptor.image.row_stride = 3;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    descriptor.image.byte_count = 3;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    descriptor.image.height = 2;
    descriptor.image.row_stride = std::numeric_limits<size_t>::max();
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    descriptor.model_id_length = 129;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    descriptor.model_id_length += 1; // Includes the terminator: embedded NUL must reject.
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    descriptor.model_id = nullptr;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    descriptor = decoded.image();
    Dust3DModelBakeImage duplicates[] { descriptor, descriptor };
    settings.model_images = duplicates;
    settings.model_image_count = 2;
    rejects(DUST3D_STATUS_INVALID_DOCUMENT);
    settings.model_images = &descriptor;
    settings.model_image_count = 1;
    descriptor.model_id = "{00000301-0000-4000-8000-000000000301}";
    rejects(DUST3D_STATUS_UNSUPPORTED_ASSET);
    descriptor = decoded.image();
    settings.maximum_sampler_payload_bytes = 1;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_sampler_payload_bytes = 0;
    settings.maximum_atlas_payload_bytes = 1;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_atlas_payload_bytes = 0;
    settings.maximum_output_payload_bytes = 1;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_output_payload_bytes = 0;
    settings.maximum_raster_samples = 1;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_raster_samples = 0;
    auto reference = load(bytes);
    settings.maximum_output_payload_bytes = dust3d_source_baked_mesh_payload_bytes(reference.get());
    Dust3DError error {};
    Owner exact(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(exact && error.status == DUST3D_STATUS_OK, "Exact aggregate source-bake quota must succeed");
    --settings.maximum_output_payload_bytes;
    rejects(DUST3D_STATUS_LIMIT_EXCEEDED);
    settings.maximum_output_payload_bytes = 0;
    std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> cancellation(dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
    require(bool(cancellation), "Cancellation token allocation");
    dust3d_cancellation_token_cancel(cancellation.get());
    settings.legacy.cancellation = cancellation.get();
    rejects(DUST3D_STATUS_CANCELLED);
}

std::vector<uint8_t> blankJsonField(const std::vector<uint8_t>& bytes, const std::string& field)
{
    auto result = bytes;
    const std::string text(bytes.begin(), bytes.end());
    const auto offset = text.find(field);
    require(offset != std::string::npos, "Missing synthetic GLB optional field");
    std::fill_n(result.begin() + offset, field.size(), ' ');
    return result;
}
void texturelessAndEncodedFallback(const std::vector<uint8_t>& bytes)
{
    const auto textureless = blankJsonField(bytes, "\"baseColorTexture\":{\"index\":0},");
    Dust3DError error {};
    auto settings = options();
    Owner owner(dust3d_source_baked_mesh_load(textureless.data(), textureless.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(owner && error.status == DUST3D_STATUS_OK && dust3d_source_baked_mesh_triangle_count(owner.get()) == 4,
        "Textureless imported models must not require decoded images");
    const auto uncolored = blankJsonField(bytes, ",\"COLOR_0\":3");
    auto fallback = load(uncolored);
    const auto map = dust3d_source_baked_mesh_texture(fallback.get(), DUST3D_TEXTURE_COLOR);
    // Independent sRGB reference: component #8fc5dc, linear base factor (.25,.5,.75),
    // one gray-128 source texel. RGB factors apply once; .5 material alpha gives 64.
    require(map.pixels[0] == 32 && map.pixels[1] == 69 && map.pixels[2] == 96 && map.pixels[3] == 64,
        "Encoded component fallback color must decode independently of linear glTF vertex colors");
}

void preservesLargeAlphaCutoffs(const std::vector<uint8_t>& bytes)
{
    auto baseline = load(bytes);
    const auto original = dust3d_source_baked_mesh_texture(baseline.get(), DUST3D_TEXTURE_COLOR);
    const std::string text(bytes.begin(), bytes.end());
    const std::string field = "\"alphaCutoff\":0.375";
    const auto offset = text.find(field);
    require(offset != std::string::npos, "Missing real GLB alpha cutoff fixture field");
    for (const auto& replacement : { std::string("1.500"), std::string("1e300") }) {
        auto changed = bytes;
        std::copy(replacement.begin(), replacement.end(), changed.begin() + offset + field.size() - replacement.size());
        auto owner = load(changed);
        const double expected = replacement == "1.500" ? 1.5 : 1e300;
        const auto count = dust3d_source_baked_mesh_triangle_count(owner.get());
        require(count == 4, "Valid large cutoffs must not change retained geometry");
        const auto* triangles = dust3d_source_baked_mesh_triangles(owner.get());
        for (size_t index = 0; index < count; ++index)
            require(triangles[index].policy_known && triangles[index].alpha_mode == DUST3D_IMPORTED_ALPHA_MASK
                    && triangles[index].alpha_cutoff == expected,
                "The owned C API must preserve the original nonnegative double cutoff");
        const auto color = dust3d_source_baked_mesh_texture(owner.get(), DUST3D_TEXTURE_COLOR);
        require(color.byte_count == original.byte_count && std::equal(original.pixels, original.pixels + original.byte_count, color.pixels),
            "A mask cutoff must not erase source RGB/alpha during baking");
    }
}

void proceduralCompatibility(const std::vector<uint8_t>& bytes)
{
    Dust3DError error {};
    auto settings = options();
    Owner current(dust3d_source_baked_mesh_load(bytes.data(), bytes.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(current && error.status == DUST3D_STATUS_OK, error.message);
    std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)> legacy(dust3d_baked_mesh_load(bytes.data(), bytes.size(), &settings.legacy, &error), dust3d_baked_mesh_destroy);
    require(legacy && error.status == DUST3D_STATUS_OK, error.message);
    const auto oldColor = dust3d_baked_mesh_texture(legacy.get(), DUST3D_TEXTURE_COLOR);
    const auto newColor = dust3d_source_baked_mesh_texture(current.get(), DUST3D_TEXTURE_COLOR);
    require(oldColor.byte_count == newColor.byte_count && std::equal(oldColor.pixels, oldColor.pixels + oldColor.byte_count, newColor.pixels), "Procedural-only source composition preserves legacy encoded color output");
    require(dust3d_source_baked_mesh_model_count(current.get()) == 0, "Procedural output cannot fabricate imported model ownership");
    const auto* records = dust3d_source_baked_mesh_triangles(current.get());
    for (size_t index = 0; index < dust3d_source_baked_mesh_triangle_count(current.get()); ++index)
        require(records[index].model_index == SIZE_MAX && records[index].uv_known, "Procedural UV metadata retained");
}

void acceptedSamplerValues(const std::vector<uint8_t>& bytes)
{
    auto replaceField = [&](const char* key, const char* value) {
        auto changed = bytes;
        const std::string text(bytes.begin(), bytes.end());
        const std::string prefix = std::string("\"") + key + "\":";
        const auto offset = text.find(prefix);
        require(offset != std::string::npos, "Missing synthetic sampler numeric field");
        std::copy_n(value, std::strlen(value), changed.begin() + offset + prefix.size());
        auto owner = load(changed);
        require(dust3d_source_baked_mesh_triangle_count(owner.get()) == 4, "Native sampler variant must retain geometry and policies");
    };
    for (const char* value : { "9728", "9729", "9984", "9985", "9986", "9987" })
        replaceField("minFilter", value);
    for (const char* value : { "9728", "9729" })
        replaceField("magFilter", value);
    for (const char* value : { "10497", "33648", "33071" }) {
        replaceField("wrapS", value);
        replaceField("wrapT", value);
    }
    auto defaults = bytes;
    const std::string text(bytes.begin(), bytes.end());
    for (const std::string field : { "\"magFilter\":9728,", "\"minFilter\":9984," }) {
        const auto offset = text.find(field);
        require(offset != std::string::npos, "Missing optional sampler member");
        std::fill_n(defaults.begin() + offset, field.size(), ' '); // Same-length valid JSON, no archive rewrite.
    }
    auto owner = load(defaults);
    require(dust3d_source_baked_mesh_triangle_count(owner.get()) == 4, "Missing glTF filters use documented defaults");
}

void rejectUnsupportedImportedImageOverride(const std::vector<uint8_t>& bytes)
{
    auto xml = fixtureXml(bytes);
    const auto offset = xml.find("<component ");
    require(offset != std::string::npos, "Missing imported component");
    const std::string identity = "{44444444-4444-4444-8444-444444444444}";
    xml.insert(offset + 11, "colorImageId='" + identity + "' ");
    auto changed = withXml(bytes, xml);
    DecodedFixture decoded;
    auto descriptor = decoded.image();
    Dust3DBakeImage legacyImage {};
    std::memcpy(legacyImage.image_id, identity.c_str(), identity.size() + 1);
    legacyImage.width = legacyImage.height = 1;
    legacyImage.row_stride = legacyImage.byte_count = 4;
    legacyImage.pixels = decoded.pixels.data();
    auto settings = options();
    settings.model_images = &descriptor;
    settings.model_image_count = 1;
    settings.legacy.images = &legacyImage;
    settings.legacy.image_count = 1;
    Dust3DError error {};
    Owner owner(dust3d_source_baked_mesh_load(changed.data(), changed.size(), &settings, &error), dust3d_source_baked_mesh_destroy);
    require(!owner && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported imported component image override cannot silently disappear in a successful bake");
}

void mirrorsFilteredAndMixed(const std::vector<uint8_t>& bytes)
{
    auto xml = fixtureXml(bytes);
    const auto partOffset = xml.find("<part ");
    const auto componentOffset = xml.find("<component ");
    require(partOffset != std::string::npos && componentOffset != std::string::npos, "Missing original fixture identities");
    xml.insert(componentOffset + 11, "combineMode='Uncombined' ");
    xml.insert(partOffset + 6, "xMirrored='true' ");
    auto mirrored = load(withXml(bytes, xml));
    require(dust3d_source_baked_mesh_triangle_count(mirrored.get()) == 8 && dust3d_source_baked_mesh_component_count(mirrored.get()) == 2, "Native mirrored source bake retains both classified components");
    size_t generated = 0;
    for (size_t index = 0; index < 8; ++index) {
        const auto& triangle = dust3d_source_baked_mesh_triangles(mirrored.get())[index];
        generated += dust3d_source_baked_mesh_component_kind(mirrored.get(), triangle.component_index) == DUST3D_SURFACE_COMPONENT_GENERATED;
        require(triangle.policy_known && triangle.uv_known && triangle.tangent.known, "Mirrored source material records align");
        double lengthSquared = 0;
        for (size_t axis = 0; axis < 3; ++axis)
            lengthSquared += double(triangle.tangent.direction[axis]) * triangle.tangent.direction[axis];
        require(std::abs(lengthSquared - 1) < 1e-5 && std::abs(triangle.tangent.handedness) == 1, "Mirrored ABI tangent frames are finite unit directions");
    }
    require(generated == 4, "Mirrored generated catalog membership");
    xml = fixtureXml(bytes);
    const auto sourceOffset = xml.find("<component ");
    xml.insert(sourceOffset + 11, "combineMode='Uncombined' ");
    const std::string part = "{00000101-0000-4000-8000-000000000101}";
    const std::string first = "{00001002-0000-4000-8000-000000001002}";
    const std::string second = "{00001003-0000-4000-8000-000000001003}";
    insertBefore(xml, "</nodes>", "<node id='" + first + "' partId='" + part + "' x='1000' y='1000' z='1000' radius='.000001'/><node id='" + second + "' partId='" + part + "' x='1000' y='1000.000001' z='1000' radius='.000001'/>");
    insertBefore(xml, "</edges>", "<edge id='{00002001-0000-4000-8000-000000002001}' partId='" + part + "' from='" + first + "' to='" + second + "'/>");
    insertBefore(xml, "</parts>", "<part id='" + part + "' target='ImportedModel' importedModelId='{00000300-0000-4000-8000-000000000300}' interpolated='false'/>");
    insertBefore(xml, "<component ", "<component id='{00000201-0000-4000-8000-000000000201}' linkDataType='partId' linkData='" + part + "' combineMode='Uncombined'/>");
    auto filtered = load(withXml(bytes, xml));
    require(dust3d_source_baked_mesh_triangle_count(filtered.get()) == 4 && dust3d_mesh_vertex_count(dust3d_source_baked_mesh_geometry(filtered.get())) == 12, "Float-collapsed triangles must not gain stale atlas or policy records");
    xml = fixtureXml(bytes);
    const auto baseOffset = xml.find("<component ");
    xml.insert(baseOffset + 11, "combineMode='Uncombined' ");
    insertBefore(xml, "</nodes>", "<node id='" + first + "' partId='" + part + "' x='2' y='.2' z='.5' radius='.1'/><node id='" + second + "' partId='" + part + "' x='2' y='.8' z='.5' radius='.1'/>");
    insertBefore(xml, "</edges>", "<edge id='{00002001-0000-4000-8000-000000002001}' partId='" + part + "' from='" + first + "' to='" + second + "'/>");
    insertBefore(xml, "</parts>", "<part id='" + part + "' interpolated='false'/>");
    insertBefore(xml, "<component ", "<component id='{00000201-0000-4000-8000-000000000201}' linkDataType='partId' linkData='" + part + "' color='#ff0000' combineMode='Uncombined'/>");
    auto mixed = load(withXml(bytes, xml));
    size_t imports = 0, procedural = 0;
    for (size_t index = 0; index < dust3d_source_baked_mesh_triangle_count(mixed.get()); ++index) {
        const auto& triangle = dust3d_source_baked_mesh_triangles(mixed.get())[index];
        require(triangle.uv_known && triangle.policy_known, "Mixed final source mappings must stay complete");
        triangle.model_index == SIZE_MAX ? ++procedural : ++imports;
    }
    require(imports == 4 && procedural > 0, "Mixed procedural/imported bake must exercise both paths");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture directory required");
        const auto bytes = read(argv[1], "ImportedSurface.ds3");
        nullAndMalformedOptions(bytes);
        importedOwnership(bytes);
        malformedImagesAndLimits(bytes);
        proceduralCompatibility(read(argv[1], "Capsule.ds3"));
        mirrorsFilteredAndMixed(bytes);
        rejectUnsupportedImportedImageOverride(bytes);
        acceptedSamplerValues(bytes);
        texturelessAndEncodedFallback(bytes);
        preservesLargeAlphaCutoffs(bytes);
        std::cout << "Native source bake contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

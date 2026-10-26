// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerImportedSurface.h>
#include <native/Engine/WorkerImportedSurfaceValidation.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(ImportedSurfaceErrorCode expected, Action action)
{
    try {
        action();
    } catch (const ImportedSurfaceError& cause) {
        require(cause.code() == expected && cause.what()[0], "Typed imported surface cause survives");
        return;
    }
    throw std::runtime_error("Invalid imported surface accepted");
}
ImportedSurfacePayload sample()
{
    ImportedSurfacePayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.geometry.warning = "original κ";
    value.models = { "é", "é" };
    value.components = { { "Original", 1 }, { "Generated", 2 } };
    ImportedSurfaceTriangle triangle;
    triangle.uv = { { { -0.0, -2 }, { 4, 0 }, { 0, 7 } } };
    triangle.colors = { { { -0.0, 0.25, 1, 0 }, { 1, 0.5, 0, 0.75 }, { 0, 1, 0, 1 } } };
    triangle.uvKnown = 1;
    triangle.colorEncoding = 1;
    triangle.modelIndex = 1;
    triangle.componentIndex = 0;
    value.triangles = { triangle };
    return value;
}
void roundTrip()
{
    auto value = sample();
    auto bytes = encodeImportedSurfacePayload(value, {});
    const auto saved = bytes;
    const auto shape = inspectImportedSurfacePayload(bytes.data(), bytes.size(), {});
    require(shape.vertexCount == 3 && shape.triangleCount == 1, "Geometry and authored records correspond");
    ImportedSurfaceLimits exact;
    exact.maximumWireBytes = bytes.size();
    exact.maximumDecodedBytes = shape.logicalBytes;
    auto copy = decodeImportedSurfacePayload(bytes.data(), bytes.size(), exact);
    require(encodeImportedSurfacePayload(copy, exact) == saved, "All source bits retain exact wire representation");
    require(copy.models[0] != copy.models[1] && copy.triangles[0].uv[0][1] == -2
            && copy.triangles[0].uv[2][1] == 7 && std::signbit(copy.triangles[0].colors[0][0]),
        "Authored repeated UVs, Unicode aliases and negative zero survive");
    bytes.assign(bytes.size(), 0);
    value = {};
    require(encodeImportedSurfacePayload(copy, {}) == saved, "Copied values outlive source storage");
    for (int field = 0; field < 2; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        else
            --limits.maximumDecodedBytes;
        reject(ImportedSurfaceErrorCode::limitExceeded, [&] { encodeImportedSurfacePayload(copy, limits); });
        reject(ImportedSurfaceErrorCode::limitExceeded, [&] { decodeImportedSurfacePayload(saved.data(), saved.size(), limits); });
    }
}
void metadata()
{
    auto original = sample();
    auto invalid = [&](ImportedSurfacePayload value, ImportedSurfaceErrorCode code) {
        reject(code, [&] { encodeImportedSurfacePayload(value, {}); });
    };
    auto value = original;
    value.triangles.clear();
    invalid(value, ImportedSurfaceErrorCode::countMismatch);
    for (double scalar : { -1.0, 1.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.triangles[0].colors[1][2] = scalar;
        invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    }
    value = original;
    value.triangles[0].colorEncoding = 2;
    invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    value = original;
    value.triangles[0].uvKnown = 2;
    invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    value = original;
    value.triangles[0].modelIndex = noImportedIdentity;
    invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    value = original;
    value.triangles[0].uvKnown = 0;
    invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    value.triangles[0].uv = {};
    value.triangles[0].modelIndex = noImportedIdentity;
    value.triangles[0].componentIndex = noImportedIdentity;
    value.models.clear();
    value.components.clear();
    auto wire = encodeImportedSurfacePayload(value, {});
    auto copy = decodeImportedSurfacePayload(wire.data(), wire.size(), {});
    require(copy.triangles[0].modelIndex == noImportedIdentity && !copy.triangles[0].uvKnown,
        "Unknown ownership and UVs remain explicit");
    value = original;
    value.models.push_back(value.models.front());
    invalid(value, ImportedSurfaceErrorCode::invalidIdentity);
}
void boundaries()
{
    const auto original = sample();
    const auto bytes = encodeImportedSurfacePayload(original, {});
    auto invalid = [&](ImportedSurfacePayload value, ImportedSurfaceErrorCode code) {
        reject(code, [&] { encodeImportedSurfacePayload(value, {}); });
    };
    for (const auto& key : { std::string(), std::string(39, 'a'), std::string("a\0b", 3), std::string("\xff", 1) }) {
        auto value = original;
        value.models[0] = key;
        invalid(value, ImportedSurfaceErrorCode::invalidIdentity);
        value = original;
        value.components[0].id = key;
        invalid(value, ImportedSurfaceErrorCode::invalidIdentity);
    }
    auto value = original;
    value.components.push_back(value.components[0]);
    invalid(value, ImportedSurfaceErrorCode::invalidIdentity);
    for (uint32_t kind : { uint32_t(0), uint32_t(3) }) {
        value = original;
        value.components[0].kind = kind;
        invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    }
    for (int field = 0; field < 3; ++field) {
        value = original;
        if (field == 0)
            value.triangles[0].modelIndex = value.models.size();
        if (field == 1)
            value.triangles[0].componentIndex = value.components.size();
        if (field == 2)
            value.triangles[0].uv[0][0] = std::numeric_limits<double>::infinity();
        invalid(value, ImportedSurfaceErrorCode::invalidMetadata);
    }
    value = original;
    value.triangles[0].colorEncoding = 0;
    auto srgb = encodeImportedSurfacePayload(value, {});
    require(decodeImportedSurfacePayload(srgb.data(), srgb.size(), {}).triangles[0].colorEncoding == 0,
        "sRGB numerical channel does not become linear");
    for (int field = 0; field < 8; ++field) {
        ImportedSurfaceLimits limits;
        if (field == 0)
            limits.maximumWireBytes = 1;
        if (field == 1)
            limits.maximumDecodedBytes = 1;
        if (field == 2)
            limits.surface.maximumWireBytes = 1;
        if (field == 3)
            limits.surface.maximumDecodedBytes = 1;
        if (field == 4)
            limits.surface.maximumModels = 1;
        if (field == 5)
            limits.surface.maximumComponents = 1;
        if (field == 6)
            limits.surface.maximumTextBytes = 1;
        if (field == 7)
            limits.geometry.maximumWireBytes = 1;
        if (field == 7) {
            bool rejected = false;
            try {
                encodeImportedSurfacePayload(original, limits);
            } catch (const MeshPayloadError& cause) {
                rejected = cause.code() == MeshPayloadErrorCode::limitExceeded;
            }
            require(rejected, "Original inner geometry quota cause survives");
        } else {
            reject(ImportedSurfaceErrorCode::limitExceeded, [&] { encodeImportedSurfacePayload(original, limits); });
            reject(ImportedSurfaceErrorCode::limitExceeded, [&] { inspectImportedSurfacePayload(bytes.data(), bytes.size(), limits); });
        }
    }
    for (int field = 0; field < 8; ++field)
        for (bool oversized : { false, true }) {
            ImportedSurfaceLimits limits;
            const auto amount = oversized ? maximumPayloadBytes + 1 : 0;
            if (field == 0)
                limits.maximumWireBytes = amount;
            if (field == 1)
                limits.maximumDecodedBytes = amount;
            if (field == 2)
                limits.surface.maximumWireBytes = amount;
            if (field == 3)
                limits.surface.maximumDecodedBytes = amount;
            if (field == 4)
                limits.surface.maximumTriangles = oversized ? 1000001 : 0;
            if (field == 5)
                limits.surface.maximumModels = oversized ? 100001 : 0;
            if (field == 6)
                limits.surface.maximumComponents = oversized ? 100001 : 0;
            if (field == 7)
                limits.surface.maximumTextBytes = amount;
            reject(ImportedSurfaceErrorCode::invalidLimits, [&] { validateImportedSurfaceLimits(limits); });
        }
    const auto measured = detail::measureImportedSurface(1, 2, 2, 22, 1, {});
    require(measured.wire > importedMetadataTriangleBytes && measured.logical > sizeof(ImportedSurfaceTriangle), "Shared native preflight charges catalogs");
    reject(ImportedSurfaceErrorCode::countMismatch, [&] { detail::measureImportedSurface(0, 0, 0, 0, 0, {}); });
    reject(ImportedSurfaceErrorCode::limitExceeded, [&] { detail::measureImportedSurface(1000001, 0, 0, 0, 1000001, {}); });
    reject(ImportedSurfaceErrorCode::limitExceeded, [&] { detail::measureImportedSurface(1, 0, 0, 9 * 1024 * 1024, 1, {}); });
    std::unordered_set<std::string_view> keys;
    detail::validateImportedKey("Model", keys);
    detail::validateImportedKind(1);
    detail::validateImportedTriangle(original.triangles[0], 2, 2);
}

void framing()
{
    auto bytes = encodeImportedSurfacePayload(sample(), {});
    reject(ImportedSurfaceErrorCode::invalidInput, [&] { inspectImportedSurfacePayload(nullptr, bytes.size(), {}); });
    for (size_t count : { size_t(0), importedSurfacePrefixBytes - 1, bytes.size() - 1 })
        reject(ImportedSurfaceErrorCode::truncated, [&] { inspectImportedSurfacePayload(bytes.data(), count, {}); });
    auto modified = bytes;
    modified.push_back(0);
    reject(ImportedSurfaceErrorCode::trailingData, [&] { inspectImportedSurfacePayload(modified.data(), modified.size(), {}); });
    auto edit = [&](size_t offset, uint64_t value, size_t width, ImportedSurfaceErrorCode code) {
        auto wire = bytes;
        require(offset <= wire.size() && width <= wire.size() - offset, "Pure bounded framing edit");
        detail::writeUnsigned(wire.data() + offset, value, width);
        reject(code, [&] { inspectImportedSurfacePayload(wire.data(), wire.size(), {}); });
    };
    edit(0, 2, 2, ImportedSurfaceErrorCode::unsupportedVersion);
    edit(2, 0, 2, ImportedSurfaceErrorCode::invalidLayout);
    edit(4, 1, 4, ImportedSurfaceErrorCode::invalidLayout);
    edit(8, 0, 8, ImportedSurfaceErrorCode::invalidLayout);
    edit(16, 0, 8, ImportedSurfaceErrorCode::invalidLayout);
    const auto surface = importedSurfacePrefixBytes + detail::readUnsigned(bytes.data() + 8, 8);
    edit(surface, 2, 2, ImportedSurfaceErrorCode::unsupportedVersion);
    edit(surface + 4, 1, 4, ImportedSurfaceErrorCode::invalidLayout);
    edit(surface + 8, 2, 8, ImportedSurfaceErrorCode::countMismatch);
    edit(surface + importedMetadataPrefixBytes + 161, 1, 1, ImportedSurfaceErrorCode::invalidLayout);
    edit(surface + 32, 100, 8, ImportedSurfaceErrorCode::truncated);
    edit(surface + 32, 0, 8, ImportedSurfaceErrorCode::trailingData);
    edit(surface + importedMetadataPrefixBytes + importedMetadataTriangleBytes, UINT32_MAX, 4, ImportedSurfaceErrorCode::truncated);
    auto catalog = [&](uint64_t models, uint64_t components, uint64_t text, uint64_t firstLength, ImportedSurfaceErrorCode code) {
        auto wire = bytes;
        const size_t prefix = surface + importedMetadataPrefixBytes + importedMetadataTriangleBytes;
        wire.resize(prefix + models * 4 + components * 8 + text, 'a');
        detail::writeUnsigned(wire.data() + 16, wire.size() - surface, 8);
        detail::writeUnsigned(wire.data() + surface + 16, models, 8);
        detail::writeUnsigned(wire.data() + surface + 24, components, 8);
        detail::writeUnsigned(wire.data() + surface + 32, text, 8);
        // Keep triangle references valid while exercising only bounded key framing.
        detail::writeUnsigned(wire.data() + surface + importedMetadataPrefixBytes + 144, noImportedIdentity, 8);
        detail::writeUnsigned(wire.data() + surface + importedMetadataPrefixBytes + 152, noImportedIdentity, 8);
        wire[surface + importedMetadataPrefixBytes + 160] = 0;
        for (size_t index = 0; index < 48; ++index)
            wire[surface + importedMetadataPrefixBytes + index] = 0;
        for (size_t index = prefix; index < wire.size(); ++index)
            wire[index] = 'a';
        detail::writeUnsigned(wire.data() + prefix, firstLength, 4);
        reject(code, [&] { inspectImportedSurfacePayload(wire.data(), wire.size(), {}); });
    };
    catalog(2, 0, 0, 2, ImportedSurfaceErrorCode::truncated);
    catalog(1, 1, 0, 7, ImportedSurfaceErrorCode::truncated);
    catalog(1, 0, 2, 1, ImportedSurfaceErrorCode::invalidLayout);
    ImportedSurfaceLimits invalid;
    invalid.maximumDecodedBytes = 0;
    reject(ImportedSurfaceErrorCode::invalidLimits, [&] { inspectImportedSurfacePayload(bytes.data(), bytes.size(), invalid); });
}

}
int main()
{
    try {
        roundTrip();
        metadata();
        framing();
        boundaries();
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

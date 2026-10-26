// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerImportedRender.h>
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
void reject(ImportedRenderErrorCode expected, Action action)
{
    try {
        action();
    } catch (const ImportedRenderError& cause) {
        require(cause.code() == expected && cause.what()[0], "Original render cause survives");
        return;
    }
    throw std::runtime_error("Invalid imported render value accepted");
}
ImportedRenderPayload sample()
{
    ImportedRenderPayload value;
    value.surface.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.surface.geometry.nodeCount = 3;
    value.surface.geometry.partCount = 1;
    value.surface.models = { "Model" };
    ImportedSurfaceTriangle triangle;
    triangle.modelIndex = 0;
    value.surface.triangles = { triangle };
    ImportedMaterial material;
    material.modelId = "Model";
    material.sampler = ImportedMaterialSampler {};
    material.image = ImportedEncodedImage { ImportedImageEncoding::png, { 1, 2, 3 } };
    value.materials.materials = { material };
    value.images = { ImportedRenderImage { 1, 1, { 17, 23, 41, 0 } } };
    return value;
}
void roundTrip()
{
    auto value = sample();
    auto wire = encodeImportedRenderPayload(value, {});
    const auto saved = wire;
    const auto shape = inspectImportedRenderPayload(wire.data(), wire.size(), {});
    require(shape.pixelBytes == 4 && shape.materialCount == 1, "Decoded pixel charge and aligned material count are explicit");
    auto copy = decodeImportedRenderPayload(wire.data(), wire.size(), {});
    value = {};
    wire.clear();
    require(copy.images[0]->pixels[0] == 17 && copy.images[0]->pixels[3] == 0
            && copy.materials.materials[0].image->bytes == std::vector<uint8_t>({ 1, 2, 3 })
            && encodeImportedRenderPayload(copy, {}) == saved,
        "Straight transparent RGB and encoded bytes survive independently without claiming image decoding");
    auto limits = ImportedRenderLimits {};
    limits.maximumDecodedPixelBytes = 3;
    reject(ImportedRenderErrorCode::limitExceeded, [&] { encodeImportedRenderPayload(copy, limits); });
    reject(ImportedRenderErrorCode::limitExceeded, [&] { decodeImportedRenderPayload(saved.data(), saved.size(), limits); });
}
void correspondence()
{
    auto value = sample();
    value.materials.materials[0].modelId = "Other";
    reject(ImportedRenderErrorCode::countMismatch, [&] { encodeImportedRenderPayload(value, {}); });
    value = sample();
    value.images.clear();
    reject(ImportedRenderErrorCode::countMismatch, [&] { encodeImportedRenderPayload(value, {}); });
    value = sample();
    value.images[0].reset();
    reject(ImportedRenderErrorCode::invalidImage, [&] { encodeImportedRenderPayload(value, {}); });
    value = sample();
    value.images[0]->pixels.pop_back();
    reject(ImportedRenderErrorCode::invalidImage, [&] { encodeImportedRenderPayload(value, {}); });
    value = sample();
    value.materials.materials[0].image.reset();
    value.materials.materials[0].sampler.reset();
    value.images[0].reset();
    auto wire = encodeImportedRenderPayload(value, {});
    require(!decodeImportedRenderPayload(wire.data(), wire.size(), {}).images[0], "Untextured materials stay untextured");
}
std::vector<uint8_t> assembled(const ImportedSurfacePayload& surface, const ImportedMaterialsPayload& materials, uint64_t count, uint64_t pixels, const std::vector<uint8_t>& images)
{
    const auto source = encodeImportedSurfacePayload(surface, {});
    const auto facts = encodeImportedMaterialsPayload(materials, {});
    std::vector<uint8_t> bytes(importedRenderPrefixBytes, 0);
    detail::writeUnsigned(bytes.data(), 1, 2);
    detail::writeUnsigned(bytes.data() + 2, importedRenderPrefixBytes, 2);
    detail::writeUnsigned(bytes.data() + 8, source.size(), 8);
    detail::writeUnsigned(bytes.data() + 16, facts.size(), 8);
    detail::writeUnsigned(bytes.data() + 24, count, 8);
    detail::writeUnsigned(bytes.data() + 32, pixels, 8);
    bytes.insert(bytes.end(), source.begin(), source.end());
    bytes.insert(bytes.end(), facts.begin(), facts.end());
    bytes.insert(bytes.end(), images.begin(), images.end());
    return bytes;
}
void boundaries()
{
    const auto value = sample();
    const auto bytes = encodeImportedRenderPayload(value, {});
    reject(ImportedRenderErrorCode::invalidInput, [&] { inspectImportedRenderPayload(nullptr, bytes.size(), {}); });
    for (size_t size : { size_t(0), importedRenderPrefixBytes - 1, bytes.size() - 1 })
        reject(ImportedRenderErrorCode::truncated, [&] { inspectImportedRenderPayload(bytes.data(), size, {}); });
    auto extra = bytes;
    extra.push_back(0);
    reject(ImportedRenderErrorCode::trailingData, [&] { inspectImportedRenderPayload(extra.data(), extra.size(), {}); });
    auto edit = [&](size_t offset, uint64_t scalar, size_t width, ImportedRenderErrorCode code) {
        auto wire = bytes;
        require(offset <= wire.size() && width <= wire.size() - offset, "Pure wire edit remains bounded");
        detail::writeUnsigned(wire.data() + offset, scalar, width);
        reject(code, [&] { inspectImportedRenderPayload(wire.data(), wire.size(), {}); });
    };
    edit(0, 2, 2, ImportedRenderErrorCode::unsupportedVersion);
    edit(2, 0, 2, ImportedRenderErrorCode::invalidLayout);
    edit(4, 1, 4, ImportedRenderErrorCode::invalidLayout);
    edit(8, 0, 8, ImportedRenderErrorCode::invalidLayout);
    edit(16, 0, 8, ImportedRenderErrorCode::invalidLayout);
    const auto imageOffset = bytes.size() - importedRenderImageBytes - 4;
    edit(imageOffset, 0, 4, ImportedRenderErrorCode::invalidImage);
    edit(imageOffset + 4, 0, 4, ImportedRenderErrorCode::invalidImage);
    edit(imageOffset, UINT32_MAX, 4, ImportedRenderErrorCode::invalidImage);
    edit(imageOffset + 4, UINT32_MAX, 4, ImportedRenderErrorCode::invalidImage);
    edit(imageOffset + 8, 3, 8, ImportedRenderErrorCode::invalidImage);
    for (int field = 0; field < 3; ++field) {
        ImportedRenderLimits limits;
        if (field == 0)
            limits.maximumWireBytes = 1;
        if (field == 1)
            limits.maximumDecodedBytes = 1;
        if (field == 2)
            limits.maximumDecodedPixelBytes = 3;
        reject(ImportedRenderErrorCode::limitExceeded, [&] { encodeImportedRenderPayload(value, limits); });
        reject(ImportedRenderErrorCode::limitExceeded, [&] { inspectImportedRenderPayload(bytes.data(), bytes.size(), limits); });
        for (uint64_t bad : { uint64_t(0), maximumPayloadBytes + 1 }) {
            if (field == 0)
                limits.maximumWireBytes = bad;
            if (field == 1)
                limits.maximumDecodedBytes = bad;
            if (field == 2)
                limits.maximumDecodedPixelBytes = bad;
            reject(ImportedRenderErrorCode::invalidLimits, [&] { validateImportedRenderLimits(limits); });
        }
    }
    auto exact = ImportedRenderLimits {};
    exact.maximumWireBytes = bytes.size();
    exact.maximumDecodedBytes = inspectImportedRenderPayload(bytes.data(), bytes.size(), {}).logicalBytes;
    exact.maximumDecodedPixelBytes = 4;
    require(encodeImportedRenderPayload(decodeImportedRenderPayload(bytes.data(), bytes.size(), exact), exact) == bytes,
        "Exact aggregate quotas accept without rounding");
    --exact.maximumDecodedBytes;
    reject(ImportedRenderErrorCode::limitExceeded, [&] { inspectImportedRenderPayload(bytes.data(), bytes.size(), exact); });
    auto empty = value;
    empty.surface.models.clear();
    empty.surface.triangles[0].modelIndex = noImportedIdentity;
    empty.materials.materials.clear();
    empty.images.clear();
    auto noModels = encodeImportedRenderPayload(empty, {});
    require(decodeImportedRenderPayload(noModels.data(), noModels.size(), {}).images.empty(), "Empty rendered catalog remains explicit");
    auto missing = value;
    missing.materials.materials.clear();
    reject(ImportedRenderErrorCode::countMismatch, [&] { encodeImportedRenderPayload(missing, {}); });
    auto bound = value;
    bound.materials.bindings.push_back({ 0, "Part", std::nullopt });
    reject(ImportedRenderErrorCode::countMismatch, [&] { encodeImportedRenderPayload(bound, {}); });
    auto imageBytes = std::vector<uint8_t>(bytes.begin() + imageOffset, bytes.end());
    auto checkJoin = [&](const ImportedSurfacePayload& source, const ImportedMaterialsPayload& facts, uint64_t count, uint64_t pixels, const std::vector<uint8_t>& images, ImportedRenderErrorCode code) {
        const auto wire = assembled(source, facts, count, pixels, images);
        reject(code, [&] { inspectImportedRenderPayload(wire.data(), wire.size(), {}); });
    };
    checkJoin(value.surface, value.materials, 0, 0, {}, ImportedRenderErrorCode::countMismatch);
    checkJoin(value.surface, bound.materials, 1, 4, imageBytes, ImportedRenderErrorCode::countMismatch);
    checkJoin(empty.surface, value.materials, 1, 4, imageBytes, ImportedRenderErrorCode::countMismatch);
    checkJoin(value.surface, empty.materials, 0, 0, {}, ImportedRenderErrorCode::countMismatch);
    auto wrong = value.materials;
    wrong.materials[0].modelId = "Other";
    checkJoin(value.surface, wrong, 1, 4, imageBytes, ImportedRenderErrorCode::countMismatch);
    auto absent = value.materials;
    absent.materials[0].image.reset();
    absent.materials[0].sampler.reset();
    checkJoin(value.surface, absent, 1, 4, imageBytes, ImportedRenderErrorCode::invalidImage);
    for (size_t field : { size_t(4), size_t(8) }) {
        std::vector<uint8_t> absentStorage(importedRenderImageBytes, 0);
        detail::writeUnsigned(absentStorage.data() + field, 1, field == 4 ? 4 : 8);
        checkJoin(value.surface, absent, 1, 0, absentStorage, ImportedRenderErrorCode::invalidImage);
    }
    auto tooLong = imageBytes;
    detail::writeUnsigned(tooLong.data(), 2, 4);
    detail::writeUnsigned(tooLong.data() + 8, 8, 8);
    checkJoin(value.surface, value.materials, 1, 4, tooLong, ImportedRenderErrorCode::truncated);
    auto trailingPixels = imageBytes;
    trailingPixels.insert(trailingPixels.end(), 4, 0);
    checkJoin(value.surface, value.materials, 1, 8, trailingPixels, ImportedRenderErrorCode::invalidLayout);
    auto pair = value;
    pair.surface.models.push_back("Second");
    auto second = pair.materials.materials[0];
    second.modelId = "Second";
    pair.materials.materials.push_back(second);
    pair.images.push_back(pair.images.front());
    auto pairWire = encodeImportedRenderPayload(pair, {});
    require(decodeImportedRenderPayload(pairWire.data(), pairWire.size(), {}).images.size() == 2, "Multiple images retain exact catalog order");
    const auto pairOffset = pairWire.size() - 2 * (importedRenderImageBytes + 4);
    detail::writeUnsigned(pairWire.data() + pairOffset, 2, 4);
    detail::writeUnsigned(pairWire.data() + pairOffset + 4, 2, 4);
    detail::writeUnsigned(pairWire.data() + pairOffset + 8, 16, 8);
    reject(ImportedRenderErrorCode::truncated, [&] { inspectImportedRenderPayload(pairWire.data(), pairWire.size(), {}); });
}

}
int main()
{
    try {
        roundTrip();
        correspondence();
        boundaries();
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

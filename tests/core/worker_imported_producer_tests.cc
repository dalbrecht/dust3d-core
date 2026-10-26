// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerImportedProducer.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
class Decoder final : public ImportedRenderImageDecoder {
public:
    size_t calls = 0;
    ImportedRenderImage decode(const ImportedEncodedImage& encoded, std::string_view model, uint64_t encodedLimit, uint64_t decodedLimit) override
    {
        require(model == "Used" && encoded.bytes == std::vector<uint8_t>({ 1, 2, 3 }) && encodedLimit > 0 && decodedLimit >= 4,
            "Decoder receives only rendered definition and remaining independent quotas");
        ++calls;
        return { 1, 1, { 11, 23, 37, 0 } };
    }
};
ImportedSurfacePayload surface()
{
    ImportedSurfacePayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.nodeCount = 3;
    value.geometry.partCount = 1;
    value.models = { "Used" };
    ImportedSurfaceTriangle triangle;
    triangle.modelIndex = 0;
    value.triangles = { triangle };
    return value;
}
ImportedMaterialsPayload facts()
{
    ImportedMaterialsPayload value;
    ImportedMaterial unused;
    unused.modelId = "Unused";
    ImportedMaterial used;
    used.modelId = "Used";
    used.sampler = ImportedMaterialSampler {};
    used.image = ImportedEncodedImage { ImportedImageEncoding::png, { 1, 2, 3 } };
    value.materials = { unused, used };
    value.bindings = { { 1, "Part", std::nullopt } };
    return value;
}
class ReceiptDecoder final : public ImportedRenderImageDecoder {
public:
    int mode = 0;
    size_t calls = 0;
    ImportedRenderImage decode(const ImportedEncodedImage&, std::string_view, uint64_t, uint64_t remaining) override
    {
        ++calls;
        if (mode == 1)
            throw std::logic_error("original decoder cause");
        if (mode == 2)
            return { 1, 1, std::vector<uint8_t>(static_cast<size_t>(remaining + 1), 0) };
        if (mode == 3)
            return { 2, 1, { 1, 2, 3, 4 } };
        return { 1, 1, { 1, 2, 3, 4 } };
    }
};
void boundaries()
{
    ReceiptDecoder decoder;
    auto reject = [&](ImportedProducerErrorCode expected, auto action) {
        bool rejected = false;
        try {
            action();
        } catch (const ImportedProducerError& cause) {
            rejected = cause.code() == expected && cause.what()[0];
        }
        require(rejected, "Producer preserves typed original rejection");
    };
    auto missing = facts();
    missing.materials[1].modelId = "Other";
    reject(ImportedProducerErrorCode::missingMaterial, [&] { prepareImportedRender(surface(), missing, decoder, {}); });
    require(!decoder.calls, "Missing material rejects before decoding");
    ImportedProducerLimits limits;
    limits.maximumEncodedImageBytes = 2;
    reject(ImportedProducerErrorCode::decodeLimitExceeded, [&] { prepareImportedRender(surface(), facts(), decoder, limits); });
    require(!decoder.calls, "Encoded quota rejects before decoding");
    for (uint64_t bad : { uint64_t(0), maximumPayloadBytes + 1 }) {
        limits.maximumEncodedImageBytes = bad;
        reject(ImportedProducerErrorCode::invalidLimits, [&] { validateImportedProducerLimits(limits); });
    }
    require(!prepareImportedRender(surface(), {}, decoder, {}), "No facts returns successful absence");
    auto untextured = facts();
    untextured.materials[1].image.reset();
    untextured.materials[1].sampler.reset();
    const auto untexturedCalls = decoder.calls;
    auto plain = prepareImportedRender(surface(), untextured, decoder, {});
    require(plain && !plain->images[0] && decoder.calls == untexturedCalls, "Untextured selected models need no decoder");
    auto duplicate = facts();
    duplicate.materials.push_back(duplicate.materials[0]);
    bool duplicateRejected = false;
    try {
        prepareImportedRender(surface(), duplicate, decoder, {});
    } catch (const ImportedMaterialsError& cause) {
        duplicateRejected = cause.code() == ImportedMaterialsErrorCode::invalidIdentity;
    }
    require(duplicateRejected, "Full input facts reject duplicate identities before selection");
    auto probe = prepareImportedRender(surface(), facts(), decoder, {});
    const auto encoded = encodeImportedRenderPayload(*probe, {});
    const auto logical = inspectImportedRenderPayload(encoded.data(), encoded.size(), {}).logicalBytes;
    for (int field = 0; field < 4; ++field) {
        ImportedProducerLimits quota;
        if (field == 0)
            quota.render.maximumWireBytes = 1;
        if (field == 1)
            quota.render.maximumDecodedBytes = 1;
        if (field == 2)
            quota.render.maximumWireBytes = encoded.size() - 4;
        if (field == 3)
            quota.render.maximumDecodedBytes = logical - 4;
        const auto beforeQuota = decoder.calls;
        reject(ImportedProducerErrorCode::decodeLimitExceeded, [&] { prepareImportedRender(surface(), facts(), decoder, quota); });
        require(decoder.calls == beforeQuota, "Aggregate quota rejects before decoding");
    }
    decoder.mode = 1;
    bool original = false;
    try {
        prepareImportedRender(surface(), facts(), decoder, {});
    } catch (const std::logic_error& cause) {
        original = std::string(cause.what()) == "original decoder cause";
    }
    require(original, "Original decoder exception is not replaced by a generic error");
    decoder.mode = 2;
    limits = {};
    limits.render.maximumDecodedPixelBytes = 4;
    reject(ImportedProducerErrorCode::decodeLimitExceeded, [&] { prepareImportedRender(surface(), facts(), decoder, limits); });
    decoder.mode = 3;
    auto two = surface();
    two.models.push_back("Unused");
    auto definitions = facts();
    definitions.materials[0].sampler = ImportedMaterialSampler {};
    definitions.materials[0].image = ImportedEncodedImage { ImportedImageEncoding::png, { 7 } };
    const auto before = decoder.calls;
    bool invalid = false;
    try {
        prepareImportedRender(two, definitions, decoder, {});
    } catch (const ImportedRenderError& cause) {
        invalid = cause.code() == ImportedRenderErrorCode::invalidImage;
    }
    require(invalid && decoder.calls == before + 1, "Invalid first image receipt rejects before decoding another image");
}

}
int main()
{
    try {
        boundaries();
        Decoder decoder;
        auto value = prepareImportedRender(surface(), facts(), decoder, {});
        require(value && decoder.calls == 1 && value->materials.materials.size() == 1
                && value->materials.materials[0].modelId == "Used" && value->materials.bindings.empty()
                && value->materials.materials[0].image->bytes == std::vector<uint8_t>({ 1, 2, 3 }) && value->images[0]->pixels[0] == 11,
            "Surface-order join decodes only used models and preserves encoded and decoded ownership");
        const auto wire = encodeImportedRenderPayload(*value, {});
        require(encodeImportedRenderPayload(decodeImportedRenderPayload(wire.data(), wire.size(), {}), {}) == wire, "Prepared output satisfies complete render contract");
        auto procedural = surface();
        procedural.models.clear();
        procedural.triangles[0].modelIndex = noImportedIdentity;
        require(!prepareImportedRender(procedural, facts(), decoder, {}) && decoder.calls == 1,
            "No rendered imported model returns absence without decoder work");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

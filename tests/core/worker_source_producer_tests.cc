// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerSourceProducerNative.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
class Procedural final : public ProceduralImageDecoder {
public:
    size_t calls = 0;
    ProceduralDecodedImage decode(const ProceduralImageInput&, uint64_t, uint64_t) override
    {
        ++calls;
        return { 1, 1, { 17, 23, 41, 0 } };
    }
};
class Imported final : public ImportedRenderImageDecoder {
public:
    size_t calls = 0;
    ImportedRenderImage decode(const ImportedEncodedImage&, std::string_view, uint64_t, uint64_t remaining) override
    {
        ++calls;
        if (remaining != 4)
            throw std::runtime_error("Imported callback must receive aggregate pixels remaining after procedural output");
        return { 1, 1, { 53, 67, 79, 255 } };
    }
};
class ReceiptDecoder final : public ProceduralImageDecoder, public ImportedRenderImageDecoder {
public:
    ImportedRenderImage receipt { 1, 1, { 1, 2, 3, 4 } };
    std::vector<uint64_t> proceduralBudgets;
    ProceduralDecodedImage decode(const ProceduralImageInput&, uint64_t, uint64_t maximum) override
    {
        proceduralBudgets.push_back(maximum);
        return receipt;
    }
    ImportedRenderImage decode(const ImportedEncodedImage&, std::string_view, uint64_t, uint64_t) override { return receipt; }
};
template <typename Action>
void expectPolicyError(Action action)
{
    try {
        action();
    } catch (const LegacyProducerError&) {
        return;
    }
    throw std::runtime_error("Invalid scalar source policy was accepted");
}
void policyCases()
{
    NativeSourceProducerOptions options;
    validateNativeSourceProducerOptions(options);
    for (auto member : { &NativeSourceProducerOptions::maximumMaterialNativeBytes, &NativeSourceProducerOptions::maximumSamplerNativeBytes,
             &NativeSourceProducerOptions::maximumAtlasNativeBytes, &NativeSourceProducerOptions::maximumOutputNativeBytes }) {
        for (uint64_t value : { uint64_t(0), maximumPayloadBytes + 1 }) {
            auto invalid = options;
            invalid.*member = value;
            expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
        }
    }
    for (auto member : { &NativeSourceProducerOptions::maximumPermittedChartCount, &NativeSourceProducerOptions::maximumPermittedRasterSamples,
             &NativeSourceProducerOptions::maximumPermittedLevels }) {
        auto invalid = options;
        invalid.*member = 0;
        expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    }
    for (auto pair : { std::pair<uint64_t NativeSourceProducerOptions::*, uint64_t> { &NativeSourceProducerOptions::maximumPermittedChartCount, 100001 },
             { &NativeSourceProducerOptions::maximumPermittedRasterSamples, 1000000001 }, { &NativeSourceProducerOptions::maximumPermittedLevels, 15 } }) {
        auto invalid = options;
        invalid.*pair.first = pair.second;
        expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    }
    auto invalid = options;
    invalid.maximumChartCount = 100001;
    expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    invalid = options;
    invalid.maximumRasterSamples = 1000000001;
    expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    invalid = options;
    invalid.maximumLevels = 15;
    expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    invalid = options;
    invalid.preparation.maximumModelImages = 0;
    expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
    invalid.preparation.maximumModelImages = 10001;
    expectPolicyError([&] { validateNativeSourceProducerOptions(invalid); });
}

}
int main()
{
    try {
        policyCases();
        std::vector<ProceduralImageInput> procedural { { "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}", "images/{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}.png", { 1, 2, 3 } } };
        ImportedMaterialsPayload materials;
        ImportedMaterial model;
        model.modelId = "Model";
        model.sampler = ImportedMaterialSampler {};
        model.image = ImportedEncodedImage { ImportedImageEncoding::png, { 4, 5, 6 } };
        materials.materials = { model };
        if (inspectImportedMaterialsPayload(materials, {}).materialCount != 1)
            throw std::runtime_error("Owned material preflight must inspect without allocating an encoded copy");
        Procedural proceduralDecoder;
        Imported importedDecoder;
        SourceProducerOptions options;
        options.maximumDecodedImageBytes = 8;
        const auto images = prepareSourceBakeImages(procedural, materials, &proceduralDecoder, &importedDecoder, options);
        if (images.procedural.size() != 1 || images.models.size() != 1 || images.models[0].modelId != "Model"
            || images.procedural[0].pixels[0] != 17 || images.models[0].image.pixels[0] != 53
            || proceduralDecoder.calls != 1 || importedDecoder.calls != 1)
            throw std::runtime_error("Distinct procedural/imported roles preserve complete owned images and aggregate quota");
        const auto expectPreflightFailure = [&](SourceProducerOptions policy, ImportedRenderImageDecoder* provider) {
            const auto beforeProcedural = proceduralDecoder.calls;
            const auto beforeImported = importedDecoder.calls;
            bool rejected = false;
            try {
                (void)prepareSourceBakeImages(procedural, materials, &proceduralDecoder, provider, policy);
            } catch (const LegacyProducerError&) {
                rejected = true;
            }
            if (!rejected || proceduralDecoder.calls != beforeProcedural || importedDecoder.calls != beforeImported)
                throw std::runtime_error("Both roles must be admitted before either callback");
        };
        auto encodedPolicy = options;
        encodedPolicy.maximumEncodedTotalBytes = 5;
        expectPreflightFailure(encodedPolicy, &importedDecoder);
        expectPreflightFailure(options, nullptr);
        auto modelPolicy = options;
        modelPolicy.maximumModelImages = 1;
        auto secondModel = model;
        secondModel.modelId = "Second";
        materials.materials.push_back(secondModel);
        expectPreflightFailure(modelPolicy, &importedDecoder);
        materials.materials.pop_back();
        auto pixelPolicy = options;
        pixelPolicy.maximumDecodedImageBytes = 4;
        bool preserved = false;
        try {
            (void)prepareSourceBakeImages(procedural, materials, &proceduralDecoder, &importedDecoder, pixelPolicy);
        } catch (const SourceImageError& failure) {
            if (failure.role() != SourceImageRole::imported || failure.identity() != "Model")
                throw std::runtime_error("Image failure lost exact resource context");
            try {
                std::rethrow_exception(failure.cause());
            } catch (const LegacyProducerError& original) {
                preserved = original.code() == LegacyProducerErrorCode::decodedLimitExceeded;
            }
        }
        if (!preserved || importedDecoder.calls != 1)
            throw std::runtime_error("Exhausted aggregate pixels must retain typed cause before imported callback");

        for (auto member : { &SourceProducerOptions::maximumEncodedImageBytes, &SourceProducerOptions::maximumEncodedTotalBytes, &SourceProducerOptions::maximumDecodedImageBytes }) {
            auto invalid = options;
            invalid.*member = 0;
            expectPreflightFailure(invalid, &importedDecoder);
            invalid.*member = maximumPayloadBytes + 1;
            expectPreflightFailure(invalid, &importedDecoder);
        }
        for (uint64_t count : { uint64_t(0), uint64_t(10001) }) {
            auto invalid = options;
            invalid.maximumModelImages = count;
            expectPreflightFailure(invalid, &importedDecoder);
        }
        auto referencePolicy = options;
        referencePolicy.procedural.images.maximumReferences = 1;
        procedural.push_back(procedural.front());
        expectPreflightFailure(referencePolicy, &importedDecoder);
        procedural.pop_back();
        auto perImagePolicy = options;
        perImagePolicy.maximumEncodedImageBytes = 2;
        expectPreflightFailure(perImagePolicy, &importedDecoder);
        auto proceduralTotalPolicy = options;
        proceduralTotalPolicy.procedural.images.maximumEncodedTotalBytes = 2;
        expectPreflightFailure(proceduralTotalPolicy, &importedDecoder);
        expectPolicyError([&] { (void)prepareSourceBakeImages(procedural, {}, nullptr, nullptr, options); });
        const auto empty = prepareSourceBakeImages({}, {}, nullptr, nullptr, options);
        if (!empty.procedural.empty() || !empty.models.empty())
            throw std::runtime_error("Absent image roles require no decoder and return owned empty collections");
        auto nativePerImagePolicy = options;
        nativePerImagePolicy.procedural.images.maximumEncodedImageBytes = 2;
        expectPreflightFailure(nativePerImagePolicy, &importedDecoder);
        ImportedMaterialsPayload untextured;
        ImportedMaterial untexturedModel;
        untexturedModel.modelId = "Untextured";
        untextured.materials.push_back(untexturedModel);
        if (!prepareSourceBakeImages({}, untextured, nullptr, nullptr, options).models.empty())
            throw std::runtime_error("Untextured declarations retain no fabricated image");
        procedural.push_back(procedural.front());
        auto proceduralPixelPolicy = options;
        proceduralPixelPolicy.maximumDecodedImageBytes = 4;
        bool proceduralExhausted = false;
        try {
            (void)prepareSourceBakeImages(procedural, {}, &proceduralDecoder, nullptr, proceduralPixelPolicy);
        } catch (const SourceImageError& cause) {
            proceduralExhausted = cause.role() == SourceImageRole::procedural;
        }
        if (!proceduralExhausted)
            throw std::runtime_error("Procedural receipts share the same remaining pixel quota");
        procedural.pop_back();
        ReceiptDecoder receipts;
        auto independentPolicy = options;
        independentPolicy.procedural.maximumDecodedImageBytes = 4;
        auto twoProcedural = procedural;
        twoProcedural.push_back(procedural.front());
        bool independentRejected = false;
        try {
            (void)prepareSourceBakeImages(twoProcedural, {}, &receipts, nullptr, independentPolicy);
        } catch (const SourceImageError& cause) {
            independentRejected = cause.role() == SourceImageRole::procedural;
        }
        if (!independentRejected || receipts.proceduralBudgets != std::vector<uint64_t>({ 4 }))
            throw std::runtime_error("Procedural aggregate host quota must tighten callback budget and prevent a second decode");
        receipts.proceduralBudgets.clear();
        receipts.receipt = { 2, 1, { 1, 2, 3, 4, 5, 6, 7, 8 } };
        independentRejected = false;
        try {
            (void)prepareSourceBakeImages(procedural, {}, &receipts, nullptr, independentPolicy);
        } catch (const SourceImageError& cause) {
            independentRejected = cause.role() == SourceImageRole::procedural;
        }
        if (!independentRejected || receipts.proceduralBudgets != std::vector<uint64_t>({ 4 }))
            throw std::runtime_error("Fake procedural receipt exceeding its tighter supplied quota must reject even within shared quota");
        for (const auto& receipt : { ImportedRenderImage { 8193, 1, { 1, 2, 3, 4 } }, ImportedRenderImage { 1, 8193, { 1, 2, 3, 4 } },
                 ImportedRenderImage { 1, 1, { 1, 2, 3 } }, ImportedRenderImage { 1, 1, std::vector<uint8_t>(12) } }) {
            receipts.receipt = receipt;
            for (bool importedRole : { false, true }) {
                bool rejected = false;
                try {
                    (void)prepareSourceBakeImages(importedRole ? std::vector<ProceduralImageInput> {} : procedural,
                        importedRole ? materials : ImportedMaterialsPayload {}, &receipts, &receipts, options);
                } catch (const SourceImageError& cause) {
                    rejected = cause.role() == (importedRole ? SourceImageRole::imported : SourceImageRole::procedural) && bool(cause.cause());
                }
                if (!rejected)
                    throw std::runtime_error("Invalid fake receipt must retain exact role and original typed cause");
            }
        }

    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

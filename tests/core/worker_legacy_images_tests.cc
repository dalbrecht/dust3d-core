// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerLegacyProducer.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
class Decoder final : public ProceduralImageDecoder {
public:
    int mode = 0;
    size_t calls = 0;
    ProceduralDecodedImage decode(const ProceduralImageInput&, uint64_t, uint64_t) override
    {
        ++calls;
        if (mode == 1)
            return { 1, 1, { 1, 2, 3, 4, 5 } };
        if (mode == 2)
            return { 8193, 1, { 1, 2, 3, 4 } };
        if (mode == 3)
            return { 1, 8193, { 1, 2, 3, 4 } };
        if (mode == 4)
            throw std::logic_error("original procedural decoder cause");
        if (mode == 5)
            return { 2, 1, { 1, 2, 3, 4 } };
        return { 1, 1, { 17, 23, 41, 0 } };
    }
};
}
int main()
{
    try {
        const std::vector<ProceduralImageInput> inputs { { "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}", "images/{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}.png", { 1, 2, 3 } } };
        Decoder decoder;
        LegacyProducerOptions options;
        options.maximumDecodedImageBytes = 4;
        const auto rejected = [&](LegacyProducerErrorCode expected, auto action) {
            bool failed = false;
            try {
                action();
            } catch (const LegacyProducerError& cause) {
                failed = cause.code() == expected && cause.what()[0];
            }
            require(failed, "Original pure procedural preparation error survives");
        };
        require(prepareProceduralImagePixels({}, nullptr, options).empty(), "Empty facts need no provider or pixels");
        rejected(LegacyProducerErrorCode::unavailableDecoder, [&] { prepareProceduralImagePixels(inputs, nullptr, options); });
        for (int mode : { 1, 2, 3 }) {
            decoder.mode = mode;
            rejected(mode == 1 ? LegacyProducerErrorCode::decodedLimitExceeded : LegacyProducerErrorCode::invalidImage,
                [&] { prepareProceduralImagePixels(inputs, &decoder, options); });
        }
        decoder.mode = 4;
        bool original = false;
        try {
            prepareProceduralImagePixels(inputs, &decoder, options);
        } catch (const std::logic_error& cause) {
            original = std::string(cause.what()) == "original procedural decoder cause";
        }
        require(original, "Underlying callback exception is preserved without native generation");
        decoder.mode = 5;
        bool shape = false;
        try {
            prepareProceduralImagePixels(inputs, &decoder, options);
        } catch (const ImportedRenderError& cause) {
            shape = cause.code() == ImportedRenderErrorCode::invalidImage;
        }
        require(shape, "Inconsistent tight RGBA receipt rejects in pure validation");
        auto two = inputs;
        two.push_back({ "{ffffffff-ffff-4fff-8fff-ffffffffffff}", "images/{ffffffff-ffff-4fff-8fff-ffffffffffff}.png", { 4, 5, 6 } });
        decoder.mode = 0;
        decoder.calls = 0;
        rejected(LegacyProducerErrorCode::decodedLimitExceeded, [&] { prepareProceduralImagePixels(two, &decoder, options); });
        require(decoder.calls == 1, "Exhausted tiny pixel quota rejects before invoking a second callback");
        for (int policy = 0; policy < 3; ++policy) {
            auto bounded = options;
            if (policy == 0)
                bounded.images.maximumReferences = 1;
            if (policy == 1)
                bounded.images.maximumEncodedImageBytes = 2;
            if (policy == 2)
                bounded.images.maximumEncodedTotalBytes = 5;
            const auto before = decoder.calls;
            rejected(LegacyProducerErrorCode::inputLimitExceeded, [&] { prepareProceduralImagePixels(two, &decoder, bounded); });
            require(decoder.calls == before, "All count and encoded quotas preflight before the first callback");
        }
        decoder.calls = 0;
        const auto decoded = prepareProceduralImagePixels(inputs, &decoder, options);
        require(decoded.size() == 1 && decoded[0].pixels == std::vector<uint8_t>({ 17, 23, 41, 0 }), "Independent pixels survive pure procedural preparation");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

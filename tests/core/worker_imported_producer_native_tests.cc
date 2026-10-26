// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerImportedProducerNative.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
class Decoder final : public ImportedRenderImageDecoder {
public:
    size_t calls = 0;
    ImportedRenderImage decode(const ImportedEncodedImage& encoded, std::string_view id, uint64_t input, uint64_t output) override
    {
        if (encoded.bytes.empty() || id.empty() || encoded.bytes.size() > input || output < 4)
            throw std::runtime_error("Healthy fake decoder contract failed");
        ++calls;
        return { 1, 1, { 1, 2, 3, 0 } };
    }
};
std::vector<uint8_t> read(const char* directory, const char* name)
{
    std::ifstream input(std::string(directory) + '/' + name, std::ios::binary);
    if (!input)
        throw std::runtime_error("Existing licensed positive fixture missing");
    return { std::istreambuf_iterator<char>(input), {} };
}
}
int main(int argc, char** argv)
{
    try {
        if (argc != 2)
            throw std::runtime_error("Existing fixture directory required");
        FailurePayload failure { FailureStatus::unsupportedAsset, { 'o', 'r', 'i', 'g', 'i', 'n', 'a', 'l' } };
        ImportedProducerNativeError copied(failure);
        if (copied.failure().status != failure.status || copied.failure().cause != failure.cause || std::string(copied.what()) != "original")
            throw std::runtime_error("Typed native cause storage did not survive");
        Decoder guardDecoder;
        for (int field = 0; field < 2; ++field)
            for (uint64_t bad : { uint64_t(0), maximumPayloadBytes + 1 }) {
                NativeImportedProducerLimits limits;
                if (field == 0)
                    limits.maximumMaterialNativeBytes = bad;
                if (field == 1)
                    limits.maximumSurfaceNativeBytes = bad;
                bool rejected = false;
                try {
                    generateImportedRender({}, guardDecoder, limits);
                } catch (const ImportedProducerError& cause) {
                    rejected = cause.code() == ImportedProducerErrorCode::invalidLimits;
                }
                if (!rejected)
                    throw std::runtime_error("Scalar policy should reject before native entry");
            }
        bool emptyRejected = false;
        try {
            generateImportedRender({}, guardDecoder, {});
        } catch (const ImportedProducerError& cause) {
            emptyRejected = cause.code() == ImportedProducerErrorCode::invalidLimits;
        }
        if (!emptyRejected || guardDecoder.calls)
            throw std::runtime_error("Empty input guard must precede native entry");
        Decoder decoder;
        auto archive = read(argv[1], "ImportedSurface.ds3");
        auto value = generateImportedRender(archive, decoder, {});
        if (!value || !decoder.calls || value->surface.models.size() != value->materials.materials.size())
            throw std::runtime_error("Same-archive imported producer did not prepare matching values");
        archive.clear();
        const auto wire = encodeImportedRenderPayload(*value, {});
        if (encodeImportedRenderPayload(decodeImportedRenderPayload(wire.data(), wire.size(), {}), {}) != wire)
            throw std::runtime_error("Produced storage did not outlive native and archive owners");
        const auto calls = decoder.calls;
        if (generateImportedRender(read(argv[1], "Capsule.ds3"), decoder, {}) || calls != decoder.calls)
            throw std::runtime_error("Procedural archive should skip imported generation and decoding");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

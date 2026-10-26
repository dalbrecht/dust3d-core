// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerLegacyProducerNative.h>
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
    size_t calls = 0;
    ProceduralDecodedImage decode(const ProceduralImageInput& input, uint64_t encodedMaximum, uint64_t decodedMaximum) override
    {
        require(input.identity == "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}" && input.encoded.size() <= encodedMaximum && decodedMaximum >= 16,
            "Distinct procedural decoder receives original identity and independent quotas");
        ++calls;
        return { 2, 2, { 17, 23, 41, 255, 17, 23, 41, 255, 17, 23, 41, 255, 17, 23, 41, 255 } };
    }
};
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Existing positive fixture directory required");
        Decoder decoder;
        LegacyProducerOptions options;
        options.atlasTextureSize = options.colorSize = options.normalSize = options.materialSize = 8;
        for (const char* name : { "Capsule.ds3", "ProceduralImage.ds3" }) {
            std::ifstream input(std::string(argv[1]) + '/' + name, std::ios::binary);
            require(bool(input), "Licensed ordinary fixture opens");
            const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
            auto result = generateLegacyBakedRender(archive, &decoder, options);
            require(!result.geometry.vertices.empty() && result.tangents.size() == result.uv.triangles.size(),
                "Complete coherent legacy geometry, UVs and tangent ownership survives native release");
            const auto wire = encodeLegacyBakedPayload(result, options.output);
            require(encodeLegacyBakedPayload(decodeLegacyBakedPayload(wire.data(), wire.size(), options.output), options.output) == wire,
                "Native producer output satisfies complete legacy rendering contract");
        }
        require(decoder.calls == 1, "Only the referenced procedural PNG invokes the procedural decoder");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

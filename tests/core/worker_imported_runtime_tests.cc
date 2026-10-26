// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerImportedOperation.h>
#include <native/Engine/WorkerImportedRequest.h>
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
    ImportedRenderImage decode(const ImportedEncodedImage& encoded, std::string_view id, uint64_t input, uint64_t output) override
    {
        require(!encoded.bytes.empty() && !id.empty() && encoded.bytes.size() <= input && output >= 4, "Explicit fake worker decoder receives bounded inputs");
        ++calls;
        return { 1, 1, { 11, 23, 37, 0 } };
    }
};
Frame request(const char* directory, const char* name)
{
    std::ifstream input(std::string(directory) + '/' + name, std::ios::binary);
    require(bool(input), "Existing positive worker fixture opens");
    Frame frame;
    frame.payload = { std::istreambuf_iterator<char>(input), {} };
    frame.payload = encodeImportedRequest({ std::move(frame.payload), {} }, {});
    frame.header.operation = Operation::generateImportedRender;
    frame.header.identity.document[0] = 1;
    frame.header.identity.request = 7;
    frame.header.identity.revision = 3;
    frame.header.payloadBytes = frame.payload.size();
    return frame;
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Existing positive fixture directory required");
        Decoder decoder;
        const auto imported = request(argv[1], "ImportedSurface.ds3");
        const auto reply = generateImportedRenderReply(imported, &decoder, {});
        require(reply.header.kind == FrameKind::response && reply.header.operation == Operation::generateImportedRender
                && reply.header.identity == imported.header.identity && reply.header.payloadBytes == reply.payload.size() && decoder.calls,
            "Imported operation preserves identity and explicit decoder execution");
        const auto value = decodeImportedRenderPayload(reply.payload.data(), reply.payload.size(), {});
        require(!value.images.empty() && value.images[0]->pixels[0] == 11, "Complete owned source rendering reaches the reply");
        const auto calls = decoder.calls;
        const auto procedural = request(argv[1], "Capsule.ds3");
        const auto absent = generateImportedRenderReply(procedural, &decoder, {});
        require(absent.header.kind == FrameKind::response && absent.payload.empty() && !absent.header.payloadBytes && calls == decoder.calls,
            "No imported model returns explicit successful absence without decoder work");
        const auto unsupported = generateImportedRenderReply(imported, nullptr, {});
        require(unsupported.header.kind == FrameKind::failure && decodeFailurePayload(unsupported.payload.data(), unsupported.payload.size(), {}).status == FailureStatus::unsupportedAsset,
            "Missing platform decoder is explicit and never invokes parent fallback");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

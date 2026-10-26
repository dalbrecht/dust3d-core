// SPDX-License-Identifier: MIT
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerJobIO.h>
#include <native/Engine/WorkerSourceBakeOperation.h>
#include <native/Engine/WorkerSourceBakeRequest.h>
#include <native/Engine/WorkerSourceNativeFailure.h>
#include <native/Engine/WorkerSourceProducerNative.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
class DecoderFailure final : public ImportedImageDecoderFailure {
public:
    DecoderFailure()
        : ImportedImageDecoderFailure("Original fake provider cause")
    {
    }
    FailurePayload failure() const override
    {
        return { FailureStatus::unsupportedAsset, { 'a', 0, 255 }, FailureDetail { FailureDomain::imageDecoder, -42, 3, 1 } };
    }
};
class FailingDecoder final : public ProceduralImageDecoder {
public:
    ProceduralDecodedImage decode(const ProceduralImageInput&, uint64_t, uint64_t) override { throw DecoderFailure(); }
};
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
        require(copySourceNativeFailure(DUST3D_STATUS_INVALID_DOCUMENT, {}).cause.empty(), "Empty original metadata diagnostics remain empty");
        const auto nativeFailure = copySourceNativeFailure(DUST3D_STATUS_LIMIT_EXCEEDED, "Original metadata cause");
        require(nativeFailure.status == FailureStatus::limitExceeded && nativeFailure.detail
                && nativeFailure.detail->domain == FailureDomain::native && nativeFailure.detail->status == DUST3D_STATUS_LIMIT_EXCEEDED
                && std::string(nativeFailure.cause.begin(), nativeFailure.cause.end()) == "Original metadata cause",
            "Metadata diagnostics retain original native status and cause without a generic generation failure");
        const auto longFailure = copySourceNativeFailure(DUST3D_STATUS_INVALID_DOCUMENT, std::string(5000, 'x'));
        require(longFailure.detail->causeTruncated && longFailure.cause.size() == FailurePayloadLimits {}.maximumWireBytes - detailedFailurePrefixBytes,
            "Original metadata diagnostics truncate only at the explicit worker wire quota");
        require(argc == 2, "Existing positive fixture directory required");
        Decoder decoder;
        NativeSourceProducerOptions options;
        options.preparation.procedural.atlasTextureSize = options.preparation.procedural.colorSize = options.preparation.procedural.normalSize = options.preparation.procedural.materialSize = 8;
        for (const char* name : { "Capsule.ds3", "ProceduralImage.ds3" }) {
            std::ifstream input(std::string(argv[1]) + '/' + name, std::ios::binary);
            require(bool(input), "Licensed ordinary fixture opens");
            const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
            auto result = generateSourceBakedRender(archive, &decoder, nullptr, options);
            require(!result.geometry.vertices.empty() && result.surface.triangles.size() * 3 == result.geometry.vertices.size(),
                "Complete coherent legacy geometry, UVs and tangent ownership survives native release");
            const auto wire = encodeSourceBakedPayload(result, options.output);
            require(encodeSourceBakedPayload(decodeSourceBakedPayload(wire.data(), wire.size(), options.output), options.output) == wire,
                "Native producer output satisfies complete legacy rendering contract");
        }
        std::ifstream mipInput(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(bool(mipInput), "Positive mip fixture opens");
        const std::vector<uint8_t> mipArchive { std::istreambuf_iterator<char>(mipInput), {} };
        SourceBakeRequest operationInput;
        operationInput.legacy.archive = mipArchive;
        operationInput.legacy.options.atlasTextureSize = operationInput.legacy.options.colorSize = operationInput.legacy.options.normalSize = operationInput.legacy.options.materialSize = 8;
        Header header;
        header.operation = Operation::generateSourceBaked;
        header.identity.document[0] = 1;
        header.identity.request = 29;
        auto body = encodeSourceBakeRequest(operationInput, {});
        header.payloadBytes = body.size();
        require(std::string(workerOperationName(header.operation)) == "generateSourceBaked", "Source operation has explicit diagnostic identity");
        const auto framed = encodeFrame({ header, body }, {});
        std::unique_ptr<FILE, decltype(&std::fclose)> source(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose);
        require(source && output && std::fwrite(framed.data(), 1, framed.size(), source.get()) == framed.size(), "Regular-file job channels hold complete request");
        std::rewind(source.get());
        WorkerJobContext context;
        serveWorkerJob(fileno(source.get()), fileno(output.get()), context);
        std::fflush(output.get());
        std::rewind(output.get());
        std::vector<uint8_t> jobWire;
        int byte;
        while ((byte = std::fgetc(output.get())) != EOF)
            jobWire.push_back(static_cast<uint8_t>(byte));
        const auto jobReply = decodeFrame(jobWire.data(), jobWire.size(), {});
        validateResponseIdentity(jobReply.header, header);
        require(context.operation == header.operation && context.requestIdentity == header.identity.request
                && jobReply.header.kind == FrameKind::response,
            "Descriptor dispatcher preserves exact source request context and complete response");
        const auto reply = generateSourceBakedReply({ header, body }, nullptr, nullptr, {});
        validateResponseIdentity(reply.header, header);
        require(reply.header.kind == FrameKind::response && !decodeSourceBakedPayload(reply.payload.data(), reply.payload.size(), {}).geometry.vertices.empty(),
            "Positive source operation returns correlated complete rendering output");
        options.shouldGenerateMips = true;
        options.maximumLevels = 3;
        const auto mipResult = generateSourceBakedRender(mipArchive, nullptr, nullptr, options);
        const auto mipWire = encodeSourceBakedPayload(mipResult, options.output);
        require(encodeSourceBakedPayload(decodeSourceBakedPayload(mipWire.data(), mipWire.size(), options.output), options.output) == mipWire,
            "Opt-in mip output retains exact levels and sampling cap through owned wire round trip");
        require(decoder.calls == 1, "Only the referenced procedural PNG invokes the procedural decoder");
        std::ifstream proceduralInput(std::string(argv[1]) + "/ProceduralImage.ds3", std::ios::binary);
        require(bool(proceduralInput), "Existing positive procedural fixture opens for fake-provider contract");
        operationInput.legacy.archive = { std::istreambuf_iterator<char>(proceduralInput), {} };
        body = encodeSourceBakeRequest(operationInput, {});
        header.payloadBytes = body.size();
        FailingDecoder failingDecoder;
        const auto failed = generateSourceBakedReply({ header, body }, &failingDecoder, nullptr, {});
        validateResponseIdentity(failed.header, header);
        require(failed.header.kind == FrameKind::failure, "Fake provider failure returns correlated explicit failure");
        const auto diagnostic = decodeFailurePayload(failed.payload.data(), failed.payload.size(), {});
        require(diagnostic.cause == std::vector<uint8_t>({ 'a', 0, 255 }) && diagnostic.detail
                && diagnostic.detail->code == -42 && diagnostic.detail->status == 3 && diagnostic.detail->causeTruncated == 1
                && diagnostic.context && diagnostic.context->role == FailureResourceRole::proceduralImage
                && diagnostic.context->identity == "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}",
            "Source image failure preserves original opaque cause, typed receipt, truncation and exact resource context");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

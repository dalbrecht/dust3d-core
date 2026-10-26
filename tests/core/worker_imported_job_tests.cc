// SPDX-License-Identifier: MIT
#include <Dust3DWorkerImageDecoder.h>
#include <Dust3DWorkerRuntime.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerImportedRender.h>
#include <native/Engine/WorkerImportedRequest.h>
#include <stdexcept>
#include <unistd.h>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
Dust3DWorkerImage* decode(void* context, const Dust3DWorkerImageInput* input, Dust3DWorkerImageResult* result)
{
    ++*static_cast<size_t*>(context);
    require(input && input->encoded_count && input->model_count && input->maximum_decoded_bytes >= 4, "Explicit worker callback receives positive bounded inputs");
    const uint8_t pixels[] = { 17, 23, 41, 0 };
    return dust3d_worker_image_create(1, 1, pixels, 4, input->maximum_decoded_bytes, result);
}
Frame invoke(const char* fixtures, const char* build, const char* name, const Dust3DWorkerImageDecoder* decoder)
{
    std::ifstream input(std::string(fixtures) + '/' + name, std::ios::binary);
    require(bool(input), "Existing positive job fixture opens");
    Frame request;
    request.payload = { std::istreambuf_iterator<char>(input), {} };
    Dust3DWorkerRequestIdentity identity {};
    identity.document[0] = 1;
    identity.request = 7;
    Dust3DWorkerRequestResult created {};
    std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> encoded(
        dust3d_worker_imported_request_create(identity, request.payload.data(), request.payload.size(), nullptr,
            { maximumPayloadBytes, maximumPayloadBytes, 4096 }, &created),
        dust3d_worker_request_destroy);
    require(encoded && created.status == DUST3D_WORKER_REQUEST_OK, "C factory owns the complete budgeted imported request");
    size_t count = 0;
    const auto* storage = dust3d_worker_request_bytes(encoded.get(), &count);
    const std::vector<uint8_t> bytes(storage, storage + count);
    request = decodeFrame(bytes.data(), bytes.size(), {});
    const auto sourcePath = std::string(build) + "/imported-job-input.bin", outputPath = std::string(build) + "/imported-job-output.bin";
    std::unique_ptr<FILE, decltype(&std::fclose)> source(std::fopen(sourcePath.c_str(), "w+b"), std::fclose), output(std::fopen(outputPath.c_str(), "w+b"), std::fclose);
    require(source && output && std::fwrite(bytes.data(), 1, bytes.size(), source.get()) == bytes.size(), "Owned shared-build test files open and write");
    std::rewind(source.get());
    const auto result = decoder ? dust3d_worker_serve_with_image_decoder(fileno(source.get()), fileno(output.get()), decoder) : dust3d_worker_serve(fileno(source.get()), fileno(output.get()));
    require(result.status == DUST3D_WORKER_JOB_COMPLETE && result.operation == DUST3D_WORKER_OPERATION_IMPORTED_RENDER && result.request_identity == 7,
        "C worker entry point reports complete correlated imported transaction");
    std::fflush(output.get());
    std::rewind(output.get());
    std::vector<uint8_t> response;
    int byte;
    while ((byte = std::fgetc(output.get())) != EOF)
        response.push_back(static_cast<uint8_t>(byte));
    source.reset();
    output.reset();
    std::remove(sourcePath.c_str());
    std::remove(outputPath.c_str());
    auto reply = decodeFrame(response.data(), response.size(), {});
    validateResponseIdentity(reply.header, request.header);
    return reply;
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Existing fixture and shared-build directories required");
        size_t calls = 0;
        Dust3DWorkerImageDecoder configuration { sizeof(Dust3DWorkerImageDecoder), &calls, decode };
        const auto imported = invoke(argv[1], argv[2], "ImportedSurface.ds3", &configuration);
        require(imported.header.kind == FrameKind::response && calls, "Explicit callback runs in imported job");
        const auto value = decodeImportedRenderPayload(imported.payload.data(), imported.payload.size(), {});
        require(value.images[0]->pixels[0] == 17, "Owned complete imported pixels traverse descriptor job output");
        const auto previous = calls;
        const auto absent = invoke(argv[1], argv[2], "Capsule.ds3", &configuration);
        require(absent.header.kind == FrameKind::response && absent.payload.empty() && calls == previous, "Procedural job returns successful absence without decoder work");
        const auto unavailable = invoke(argv[1], argv[2], "ImportedSurface.ds3", nullptr);
        require(unavailable.header.kind == FrameKind::failure && decodeFailurePayload(unavailable.payload.data(), unavailable.payload.size(), {}).status == FailureStatus::unsupportedAsset,
            "Default entry point explicitly rejects unavailable image decoder");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

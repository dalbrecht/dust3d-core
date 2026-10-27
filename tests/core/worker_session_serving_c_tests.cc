// SPDX-License-Identifier: MIT
#include <Dust3DWorkerSession.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerProtocol.h>
#include <stdexcept>
#include <unistd.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("C session serving contract");
}
using File = std::unique_ptr<FILE, decltype(&std::fclose)>;
using Output = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
Output generate(Dust3DWorkerSession* session, Dust3DWorkerRequestIdentity identity, const std::vector<uint8_t>& archive, bool correlated = true)
{
    Dust3DWorkerRequestResult requestResult {};
    std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> request(
        dust3d_worker_session_request_create(identity, archive.data(), archive.size(), 0,
            { 128ULL * 1024 * 1024, 256ULL * 1024 * 1024, 4096 }, &requestResult),
        dust3d_worker_request_destroy);
    require(request && requestResult.status == DUST3D_WORKER_REQUEST_OK);
    size_t size = 0;
    const auto* bytes = dust3d_worker_request_bytes(request.get(), &size);
    File input(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose);
    require(input && output);
    require(std::fwrite(bytes, 1, size, input.get()) == size);
    require(std::fflush(input.get()) == 0 && std::fseek(input.get(), 0, SEEK_SET) == 0);
    const Dust3DWorkerSessionOutputLimits limits { 256ULL * 1024 * 1024, 128ULL * 1024 * 1024, 0 };
    const auto receipt = correlated
        ? dust3d_worker_session_serve_correlated(session, identity, 9, fileno(input.get()), fileno(output.get()), limits)
        : dust3d_worker_session_serve(session, 9, fileno(input.get()), fileno(output.get()), limits);
    require(receipt.status == DUST3D_WORKER_JOB_COMPLETE
        && receipt.operation == DUST3D_WORKER_OPERATION_SESSION_INDEXED_SOURCED_MESH
        && receipt.request_identity == identity.request);
    require(std::fseek(output.get(), 0, SEEK_END) == 0);
    const auto length = std::ftell(output.get());
    require(length > 0 && std::fseek(output.get(), 0, SEEK_SET) == 0);
    std::vector<uint8_t> reply(static_cast<size_t>(length));
    require(std::fread(reply.data(), 1, reply.size(), output.get()) == reply.size());
    auto frame = decodeFrame(reply.data(), reply.size(), {});
    require(frame.header.kind == FrameKind::response
        && frame.header.operation == Operation::generateSessionIndexedSourcedMesh
        && frame.header.identity.request == identity.request && frame.header.identity.revision == identity.revision);
    Dust3DWorkerDecodeResult result {};
    Output owner(dust3d_worker_session_output_decode(frame.payload.data(), frame.payload.size(), limits, &result), dust3d_worker_session_output_destroy);
    require(owner && result.status == DUST3D_WORKER_DECODE_OK);
    return owner;
}
void rejectForeignExpectation(Dust3DWorkerSession* session, Dust3DWorkerRequestIdentity identity, const std::vector<uint8_t>& archive)
{
    Dust3DWorkerRequestResult result {};
    std::unique_ptr<Dust3DWorkerRequest, decltype(&dust3d_worker_request_destroy)> request(
        dust3d_worker_session_request_create(identity, archive.data(), archive.size(), 0,
            { 128ULL * 1024 * 1024, 256ULL * 1024 * 1024, 4096 }, &result),
        dust3d_worker_request_destroy);
    require(request && result.status == DUST3D_WORKER_REQUEST_OK);
    size_t size = 0;
    const auto* bytes = dust3d_worker_request_bytes(request.get(), &size);
    for (int mode = 0; mode < 3; ++mode) {
        File input(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose);
        require(input && output);
        require(std::fwrite(bytes, 1, size, input.get()) == size);
        require(!std::fflush(input.get()) && !std::fseek(input.get(), 0, SEEK_SET));
        auto expected = identity;
        if (!mode)
            expected.document[0] = 2;
        else if (mode == 1)
            ++expected.request;
        else
            ++expected.revision;
        const auto receipt = dust3d_worker_session_serve_correlated(session, expected, 9, fileno(input.get()), fileno(output.get()),
            { 256ULL * 1024 * 1024, 128ULL * 1024 * 1024, 0 });
        require(receipt.status == DUST3D_WORKER_JOB_PROTOCOL_REJECTED && receipt.detail_source == DUST3D_WORKER_DETAIL_FRAME
            && receipt.detail_code == static_cast<int32_t>(ProtocolErrorCode::invalidIdentity) && receipt.request_identity == identity.request);
        require(::lseek(fileno(input.get()), 0, SEEK_CUR) == headerBytes && ::lseek(fileno(output.get()), 0, SEEK_CUR) == 0);
    }
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2);
        std::ifstream source(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(static_cast<bool>(source));
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(source), {} };
        Dust3DWorkerSessionConfiguration configuration { { 1 }, 9, 64ULL * 1024 * 1024 };
        Dust3DWorkerDecodeResult result {};
        std::unique_ptr<Dust3DWorkerSession, decltype(&dust3d_worker_session_destroy)> session(
            dust3d_worker_session_create(configuration, &result), dust3d_worker_session_destroy);
        require(session && result.status == DUST3D_WORKER_DECODE_OK);
        Dust3DWorkerRequestIdentity identity {};
        identity.document[0] = 1;
        identity.request = 1;
        identity.revision = 42;
        rejectForeignExpectation(session.get(), identity, archive);
        auto first = generate(session.get(), identity, archive);
        ++identity.request;
        auto reused = generate(session.get(), identity, archive, false);
        Dust3DWorkerSessionStatistics statistics {};
        require(dust3d_worker_session_output_statistics(reused.get(), &statistics, sizeof(statistics))
            && statistics.reused_components > 0);
        require(dust3d_worker_session_reset(session.get(), 9, &result));
        ++identity.request;
        auto rebuilt = generate(session.get(), identity, archive);
        require(dust3d_worker_session_output_statistics(rebuilt.get(), &statistics, sizeof(statistics))
            && statistics.rebuilt_components > 0);
        require(dust3d_worker_session_close(session.get(), &result));
        session.reset();
        require(dust3d_worker_session_output_summary(first.get()).outcome == DUST3D_WORKER_SESSION_SOURCED);
        require(dust3d_worker_session_output_primary(reused.get()) && dust3d_worker_session_output_primary(rebuilt.get()));
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

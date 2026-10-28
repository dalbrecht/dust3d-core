// SPDX-License-Identifier: MIT
#include <Dust3DWorkerPreviewSession.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerPreviewArchiveIO.h>
#include <native/Engine/WorkerPreviewSessionIO.h>
#include <native/Engine/WorkerSessionScope.h>
using namespace dust3d::worker;
namespace {
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Preview serving contract");
}
using File = std::unique_ptr<FILE, decltype(&fclose)>;
using Session = std::unique_ptr<Dust3DWorkerSession, decltype(&dust3d_worker_session_destroy)>;
using Output = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2);
        std::ifstream source(argv[1], std::ios::binary);
        require(bool(source));
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(source), {} };
        require(!archive.empty() && archive.size() < 1048576);
        Dust3DWorkerPreviewSessionScope scope {};
        scope.identity.document[0] = 1;
        scope.identity.request = 1;
        scope.identity.revision = 7;
        scope.connection_generation = 3;
        Dust3DWorkerSessionConfiguration configuration {};
        configuration.document[0] = 1;
        configuration.connection_generation = 3;
        configuration.maximum_cache_payload_bytes = 64ULL * 1024 * 1024;
        Dust3DWorkerDecodeResult created {};
        Session session(dust3d_worker_session_create(configuration, &created), dust3d_worker_session_destroy);
        require(session && created.status == DUST3D_WORKER_DECODE_OK);
        const Dust3DWorkerPreviewArchiveContext context { archive.size(), 0, 0 };
        const Dust3DWorkerPreviewChunkPolicy inputPolicy { archive.size() + sourceRequestPrefixBytes, 64 }, outputPolicy { 64ULL * 1024 * 1024, 256 };
        auto prepare = [&](FILE* input, Dust3DWorkerPreviewSessionScope admitted) {
            require(dust3d_worker_preview_archive_write(fileno(input), admitted, context, archive.data(), archive.size(), inputPolicy).status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK);
            rewind(input);
        };
        size_t vertices = 0;
        std::vector<uint32_t> indices;
        for (uint64_t request = 1; request <= 3; ++request) {
            if (request == 3)
                require(dust3d_worker_session_reset(session.get(), 3, &created));
            scope.identity.request = request;
            File input(tmpfile(), fclose), output(tmpfile(), fclose);
            require(input && output);
            prepare(input.get(), scope);
            const auto served = dust3d_worker_preview_session_serve(session.get(), fileno(input.get()), fileno(output.get()), scope, context, inputPolicy, outputPolicy);
            require(served.status == DUST3D_WORKER_PREVIEW_SESSION_OK);
            rewind(output.get());
            Dust3DWorkerPreviewSessionResult receipt {};
            Output owner(dust3d_worker_preview_session_read(fileno(output.get()), scope, context, outputPolicy, &receipt), dust3d_worker_session_output_destroy);
            require(owner && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_OK && dust3d_worker_session_output_summary(owner.get()).outcome == DUST3D_WORKER_SESSION_SOURCED);
            input.reset();
            output.reset();
            const auto* primary = dust3d_worker_session_output_primary(owner.get());
            const auto summary = dust3d_worker_decoded_summary(primary);
            const auto count = dust3d_worker_decoded_index_count(primary);
            require(summary.vertex_count && count);
            std::vector<uint32_t> copied(count);
            require(dust3d_worker_decoded_indices(primary, 0, count, copied.data()));
            if (request == 1) {
                vertices = summary.vertex_count;
                indices = copied;
            } else
                require(vertices == summary.vertex_count && indices == copied);
            Dust3DWorkerSessionStatistics statistics {};
            require(dust3d_worker_session_output_statistics(owner.get(), &statistics, sizeof(statistics)) && statistics.retained_payload_bytes > 0);
            require(request == 2 ? statistics.reused_components > 0 : statistics.reused_components == 0);
        }
        File replay(tmpfile(), fclose), rejectedOutput(tmpfile(), fclose);
        require(replay && rejectedOutput);
        prepare(replay.get(), scope);
        const auto rejected = dust3d_worker_preview_session_serve(session.get(), fileno(replay.get()), fileno(rejectedOutput.get()), scope, context, inputPolicy, outputPolicy);
        require(rejected.status == DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED && rejected.detail_source == DUST3D_WORKER_DETAIL_SESSION_SCOPE
            && rejected.detail_code == static_cast<int32_t>(SessionScopeErrorCode::reusedRequest) && ftell(rejectedOutput.get()) == 0);
        const auto missing = dust3d_worker_preview_session_serve(nullptr, -1, -1, scope, context, inputPolicy, outputPolicy);
        require(missing.status == DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED && missing.detail_source == DUST3D_WORKER_DETAIL_FRAME);
        scope.identity.request = 4;
        File empty(tmpfile(), fclose), output(tmpfile(), fclose);
        require(empty && output);
        const auto incomplete = dust3d_worker_preview_session_serve(session.get(), fileno(empty.get()), fileno(output.get()), scope, context, inputPolicy, outputPolicy);
        require(incomplete.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED && ftell(output.get()) == 0);
        File prefix(tmpfile(), fclose);
        require(bool(prefix));
        prepare(prefix.get(), scope);
        require(fseek(prefix.get(), 163, SEEK_SET) == 0 && fputc(2, prefix.get()) == 2);
        rewind(prefix.get());
        const auto unsupported = dust3d_worker_preview_session_serve(session.get(), fileno(prefix.get()), fileno(output.get()), scope, context, inputPolicy, outputPolicy);
        require(unsupported.status == DUST3D_WORKER_PREVIEW_SESSION_CODEC_REJECTED && unsupported.detail_source == DUST3D_WORKER_DETAIL_SOURCE_REQUEST
            && unsupported.detail_code == static_cast<int32_t>(SourceRequestErrorCode::unsupportedVersion) && ftell(output.get()) == 0);
        File quotaInput(tmpfile(), fclose);
        require(bool(quotaInput));
        prepare(quotaInput.get(), scope);
        const auto invalidPolicy = dust3d_worker_preview_session_serve(session.get(), fileno(quotaInput.get()), fileno(output.get()), scope, context, inputPolicy, { 1, 256 });
        require(invalidPolicy.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED && ftell(quotaInput.get()) == 0 && ftell(output.get()) == 0);
        const auto sameChannel = dust3d_worker_preview_session_serve(session.get(), fileno(quotaInput.get()), fileno(quotaInput.get()), scope, context, inputPolicy, outputPolicy);
        require(sameChannel.status == DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED && ftell(quotaInput.get()) == 0);
        const auto invalidInput = dust3d_worker_preview_session_serve(session.get(), -1, fileno(output.get()), scope, context, inputPolicy, outputPolicy);
        require(invalidInput.status == DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED && ftell(output.get()) == 0);
        const auto invalidOutput = dust3d_worker_preview_session_serve(session.get(), fileno(quotaInput.get()), -1, scope, context, inputPolicy, outputPolicy);
        require(invalidOutput.status == DUST3D_WORKER_PREVIEW_SESSION_PROTOCOL_REJECTED && ftell(quotaInput.get()) == 0);
        // Native input/budgets stay healthy; only the reply codec allowance is small.
        const auto smallReply = dust3d_worker_preview_session_serve(session.get(), fileno(quotaInput.get()), fileno(output.get()), scope, context, inputPolicy, { 128, 256 });
        require(smallReply.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED && ftell(output.get()) == 0);
        rewind(quotaInput.get());
        const auto consumed = dust3d_worker_preview_session_serve(session.get(), fileno(quotaInput.get()), fileno(output.get()), scope, context, inputPolicy, outputPolicy);
        require(consumed.status == DUST3D_WORKER_PREVIEW_SESSION_ADMISSION_REJECTED && consumed.detail_code == static_cast<int32_t>(SessionScopeErrorCode::reusedRequest));
        std::array<uint8_t, 16> document {};
        document[0] = 1;
        ConnectionGenerationSession direct(document, 3, 0);
        PreviewChunkIdentity expected;
        expected.request.document = document;
        expected.request.request = 4;
        expected.request.revision = 7;
        expected.connectionGeneration = 3;
        File input(tmpfile(), fclose);
        require(bool(input));
        prepare(input.get(), scope);
        auto wrongDirection = expected;
        wrongDirection.direction = PreviewChunkDirection::session;
        try {
            servePreviewSessionJob(fileno(input.get()), fileno(output.get()), direct, wrongDirection, { archive.size(), 0, 0 }, { inputPolicy.maximum_transfer_bytes, 64 }, { outputPolicy.maximum_transfer_bytes, 256 });
            throw std::runtime_error("Reply-direction serving scope accepted");
        } catch (const PreviewChunkError& cause) {
            require(cause.code() == PreviewChunkErrorCode::invalidIdentity && ftell(input.get()) == 0 && ftell(output.get()) == 0);
        }
        servePreviewSessionJob(fileno(input.get()), fileno(output.get()), direct, expected, { archive.size(), 0, 0 }, { inputPolicy.maximum_transfer_bytes, 64 }, { outputPolicy.maximum_transfer_bytes, 256 });
        rewind(output.get());
        expected.direction = PreviewChunkDirection::session;
        const auto result = readPreviewSessionChannel(fileno(output.get()), expected, { archive.size(), 0, 0 }, { outputPolicy.maximum_transfer_bytes, 256 });
        require(std::get<IndexedSourcesPayload>(result.outcome).geometry.indices == indices && result.statistics && result.statistics->maximumCachePayloadBytes == 0);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

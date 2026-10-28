// SPDX-License-Identifier: MIT
#include <Dust3DWorkerPreviewSession.h>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerJobIO.h>
#include <native/Engine/WorkerPreviewSessionIO.h>
using namespace dust3d::worker;
namespace {
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Preview session channel contract");
}
using File = std::unique_ptr<FILE, decltype(&fclose)>;
using Owner = std::unique_ptr<Dust3DWorkerSessionOutput, decltype(&dust3d_worker_session_output_destroy)>;
const PreviewArchiveContext context { 4096, 0, 0 };
const PreviewChunkPolicy policy { 16384, 64 };
PreviewChunkIdentity identity()
{
    PreviewChunkIdentity value;
    value.direction = PreviewChunkDirection::session;
    value.request.document[0] = 1;
    value.request.request = 1;
    value.request.revision = 2;
    value.connectionGeneration = 3;
    return value;
}
SessionOutput fixture()
{
    IndexedSourcesPayload pair;
    pair.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
    pair.geometry.indices = { 0, 1, 2 };
    pair.geometry.nodeCount = pair.geometry.partCount = 1;
    pair.geometry.warning.assign(5000, 'w');
    pair.sources.identities = { { std::string(1500, 'n'), "part" } };
    for (size_t index = 0; index < 3; ++index) {
        pair.sources.corners.push_back({ SourceHintKind::nodeHint, index, 1 });
        pair.sources.influences.push_back({ SourceHintKind::nodeHint, 0, 0.75f });
    }
    SessionOutput result;
    result.outcome = std::move(pair);
    result.statistics = GenerationStatistics { 64, 0, 0, 0, 0, 0, 0, {} };
    return result;
}
std::vector<uint8_t> wire(const SessionOutput& value)
{
    std::vector<uint8_t> bytes;
    writePreviewSession(identity(), context, value, policy, [&](const uint8_t* data, size_t size) { bytes.insert(bytes.end(), data, data + size); });
    return bytes;
}
}
int main(int argc, char** argv)
{
    try {
        Dust3DWorkerPreviewSessionScope scope {};
        scope.identity.document[0] = 1;
        scope.identity.request = 1;
        scope.identity.revision = 2;
        scope.connection_generation = 3;
        const Dust3DWorkerPreviewArchiveContext captured { 4096, 0, 0 };
        const Dust3DWorkerPreviewChunkPolicy chunking { 16384, 64 };
        Dust3DWorkerPreviewSessionResult receipt {};
        const FailurePayload issue { FailureStatus::limitExceeded, { 0, 255 }, FailureDetail { FailureDomain::previewRange, 2, 0, 0 } };
        for (int mode = 0; mode < 4; ++mode) {
            auto value = fixture();
            if (mode == 1) {
                auto mesh = std::move(std::get<IndexedSourcesPayload>(value.outcome).geometry);
                value.outcome = std::move(mesh);
                value.sourceIssue = issue;
            } else if (mode == 2) {
                value.outcome = issue;
                value.statistics.reset();
                value.statisticsIssue = issue;
            } else if (mode == 3) {
                value.outcome = std::monostate {};
                value.sourceIssue = issue;
                value.statistics.reset();
            }
            File channel(tmpfile(), fclose);
            require(bool(channel));
            writePreviewSessionChannel(fileno(channel.get()), identity(), context, value, policy);
            rewind(channel.get());
            require(wire(readPreviewSessionChannel(fileno(channel.get()), identity(), context, policy)) == wire(value));
            rewind(channel.get());
            Owner owner(dust3d_worker_preview_session_read(fileno(channel.get()), scope, captured, chunking, &receipt), dust3d_worker_session_output_destroy);
            require(owner && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_OK);
            channel.reset();
            const auto summary = dust3d_worker_session_output_summary(owner.get());
            const Dust3DWorkerSessionOutcome outcomes[] { DUST3D_WORKER_SESSION_SOURCED, DUST3D_WORKER_SESSION_ORDINARY, DUST3D_WORKER_SESSION_FAILURE, DUST3D_WORKER_SESSION_EMPTY };
            require(summary.outcome == outcomes[mode] && bool(summary.statistics_available) == value.statistics.has_value());
            require(bool(dust3d_worker_session_output_source_issue(owner.get())) == value.sourceIssue.has_value());
            require(bool(dust3d_worker_session_output_statistics_issue(owner.get())) == value.statisticsIssue.has_value());
            const auto* primary = dust3d_worker_session_output_primary(owner.get());
            if (mode <= 1) {
                size_t count = 0;
                const auto* warning = dust3d_worker_decoded_warning(primary, &count);
                require(warning && count == 5000 && std::string(warning, count) == std::string(5000, 'w'));
                uint32_t indices[3] {};
                require(dust3d_worker_decoded_indices(primary, 0, 3, indices) && indices[0] == 0 && indices[1] == 1 && indices[2] == 2);
            }
            if (!mode) {
                size_t count = 0;
                const auto* node = dust3d_worker_decoded_source_node(primary, 0, &count);
                require(node && count == 1500 && std::string(node, count) == std::string(1500, 'n'));
                Dust3DWorkerSourceInfluence influence {};
                require(dust3d_worker_decoded_source_influence(primary, 2, &influence) && influence.weight == 0.75f && influence.identity_index == 0);
            }
            if (value.sourceIssue || mode == 2) {
                const auto* failure = mode == 2 ? primary : dust3d_worker_session_output_source_issue(owner.get());
                const auto detail = dust3d_worker_decoded_failure_detail(failure);
                size_t count = 0;
                const auto* cause = dust3d_worker_decoded_failure_cause(failure, &count);
                require(detail.has_detail && detail.domain == 19 && detail.code == 2 && cause && count == 2 && cause[0] == 0 && cause[1] == 255);
            }
        }
        require(!dust3d_worker_preview_session_read(-1, scope, captured, chunking, nullptr));
        Owner invalid(dust3d_worker_preview_session_read(-1, scope, captured, chunking, &receipt), dust3d_worker_session_output_destroy);
        require(!invalid && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_IO_FAILED && receipt.system_error == EINVAL && receipt.io_stage == DUST3D_WORKER_IO_INPUT);
        File empty(tmpfile(), fclose);
        require(bool(empty));
        invalid.reset(dust3d_worker_preview_session_read(fileno(empty.get()), scope, captured, chunking, &receipt));
        require(!invalid && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED && receipt.detail_code == static_cast<int32_t>(PreviewChunkErrorCode::invalidSequence) && !receipt.system_error);
        SessionOutput taggedEmpty;
        taggedEmpty.outcome = IndexedMeshPayload {};
        try {
            writePreviewSessionChannel(fileno(empty.get()), identity(), context, taggedEmpty, policy);
            throw std::runtime_error("Invalid typed value emitted channel bytes");
        } catch (const IndexedMeshPayloadError& cause) {
            require(cause.code() == IndexedMeshPayloadErrorCode::invalidGeometry && ftell(empty.get()) == 0);
        }
        File bounded(tmpfile(), fclose);
        require(bool(bounded));
        writePreviewSessionChannel(fileno(bounded.get()), identity(), context, fixture(), policy);
        rewind(bounded.get());
        invalid.reset(dust3d_worker_preview_session_read(fileno(bounded.get()), scope, { 4096, 0, 1 }, chunking, &receipt));
        require(!invalid && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_RANGE_REJECTED && receipt.detail_code == static_cast<int32_t>(PreviewRangeErrorCode::nativeBudgetExceeded));
        require(fseek(bounded.get(), 0, SEEK_END) == 0 && fputc(0, bounded.get()) == 0);
        rewind(bounded.get());
        invalid.reset(dust3d_worker_preview_session_read(fileno(bounded.get()), scope, captured, chunking, &receipt));
        require(!invalid && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_CHUNK_REJECTED);
        File unsupported(tmpfile(), fclose);
        require(bool(unsupported));
        writePreviewSessionChannel(fileno(unsupported.get()), identity(), context, fixture(), { 16384, 65536 });
        require(fseek(unsupported.get(), 161, SEEK_SET) == 0 && fputc(2, unsupported.get()) == 2);
        rewind(unsupported.get());
        invalid.reset(dust3d_worker_preview_session_read(fileno(unsupported.get()), scope, captured, { 16384, 65536 }, &receipt));
        require(!invalid && receipt.status == DUST3D_WORKER_PREVIEW_SESSION_CODEC_REJECTED && receipt.detail_source == DUST3D_WORKER_DETAIL_SESSION_OUTPUT && receipt.detail_code == static_cast<int32_t>(SessionOutputErrorCode::unsupportedVersion));
        try {
            writePreviewSessionChannel(-1, identity(), context, fixture(), policy);
            throw std::runtime_error("Invalid output descriptor accepted");
        } catch (const WorkerJobIOError& cause) {
            require(cause.stage() == WorkerJobStage::output && cause.systemError() == EINVAL);
        }
        if (argc == 2) {
            File output(fopen(argv[1], "wb"), fclose);
            require(bool(output));
            writePreviewSessionChannel(fileno(output.get()), identity(), context, fixture(), policy);
        }
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

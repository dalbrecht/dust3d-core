// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerPreviewRange.h>
#include <native/Engine/WorkerSessionOutput.h>
#include <native/Engine/WorkerSourceRequest.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <class Action>
void reject(PreviewRangeErrorCode expected, Action action)
{
    try {
        action();
    } catch (const PreviewRangeError& cause) {
        require(cause.code() == expected && cause.what()[0], "Original typed accounting error survives");
        return;
    }
    throw std::runtime_error("Invalid scalar layout accepted");
}
}
int main()
{
    try {
        IndexedMeshPayload mesh;
        mesh.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } } };
        mesh.indices = { 0, 1, 2 };
        mesh.nodeCount = mesh.partCount = 1;
        mesh.warning = "keep";
        SourceHintPayload sources;
        sources.identities = { { "node", "part" } };
        sources.corners.resize(3);
        PreviewTransferShape shape;
        shape.archiveBytes = 37;
        shape.vertexCount = shape.indexCount = 3;
        shape.nodeCount = shape.partCount = 1;
        shape.warningBytes = mesh.warning.size();
        shape.auxiliaryWireBytes = encodeGenerationStatistics(std::nullopt, {}).size();
        auto ordinary = planPreviewTransfer(shape, {});
        SessionOutput value;
        value.outcome = mesh;
        require(ordinary.sessionPayloadBytes == encodeSessionOutput(value, {}).size(), "Ordinary plan matches actual complete tiny encoder");
        require(ordinary.requestPayloadBytes == sourceRequestPrefixBytes + 37, "Archive prefix is charged separately");
        shape.sources = PreviewSourceShape { 1, 3, 0, 8 };
        auto sourced = planPreviewTransfer(shape, {});
        value.outcome = IndexedSourcesPayload { mesh, sources };
        require(sourced.sessionPayloadBytes == encodeSessionOutput(value, {}).size(), "Source plan matches actual nested tiny encoder");
        require(sourced.sourceWireBytes == encodeSourceHintPayload(sources, {}).size(), "Exact source wire layout matches codec");
        shape.sources.reset();
        FailurePayload issue { FailureStatus::generationFailed, { 'c', 'a', 'u', 's', 'e' } };
        shape.auxiliaryWireBytes += encodeFailurePayload(issue, {}).size();
        shape.auxiliaryLogicalBytes = issue.cause.size();
        value.outcome = mesh;
        value.sourceIssue = issue;
        require(planPreviewTransfer(shape, {}).sessionPayloadBytes == encodeSessionOutput(value, {}).size(), "Ordinary fallback preserves independent diagnostic charge");
        PreviewTransferShape empty;
        empty.archiveBytes = 37;
        empty.auxiliaryWireBytes = encodeGenerationStatistics(std::nullopt, {}).size();
        value = {};
        require(planPreviewTransfer(empty, {}).sessionPayloadBytes == encodeSessionOutput(value, {}).size(), "Successful no-geometry is not a fake mesh");

        // Counts only: no large byte spans, arrays, archives or native calls.
        PreviewTransferShape large;
        large.archiveBytes = 256ULL * 1024 * 1024;
        large.vertexCount = large.indexCount = 6000000;
        large.auxiliaryWireBytes = generationStatisticsPrefixBytes;
        auto plan = planPreviewTransfer(large, {});
        require(plan.requestPayloadBytes > maximumPayloadBytes && plan.nativeIndexedBytes <= 256ULL * 1024 * 1024, "Full archive needs framing beyond the legacy payload ceiling");
        require(large.vertexCount > IndexedMeshPayloadLimits {}.maximumVertices, "Native default geometry range exceeds incidental worker vertex defaults");
        large.vertexCount = 4000000;
        large.indexCount = 4200000;
        large.nodeCount = large.partCount = 1;
        large.sources = PreviewSourceShape { 1, 4200000, 0, 8 };
        PreviewNativeBudgets configured;
        configured.maximumSourceBytes = 256ULL * 1024 * 1024;
        plan = planPreviewTransfer(large, configured);
        require(plan.nativeSourceBytes <= configured.maximumSourceBytes && plan.sessionPayloadBytes > maximumPayloadBytes, "Independent supported source budget can exceed legacy frame range");
        reject(PreviewRangeErrorCode::nativeBudgetExceeded, [&] { planPreviewTransfer(large, {}); });
        large.sources.reset();
        large.vertexCount = large.indexCount = 9999999;
        configured.maximumIndexedBytes = 512ULL * 1024 * 1024;
        require(planPreviewTransfer(large, configured).nativeIndexedBytes > 256ULL * 1024 * 1024, "Explicit positive native geometry policy is not silently capped at its default");
        configured.maximumSourceBytes = 256ULL * 1024 * 1024 + 1;
        reject(PreviewRangeErrorCode::invalidLimits, [&] { planPreviewTransfer(shape, configured); });
        PreviewNativeBudgets tiny;
        tiny.maximumIndexedBytes = 1;
        reject(PreviewRangeErrorCode::nativeBudgetExceeded, [&] { planPreviewTransfer(shape, tiny); });
        auto bad = shape;
        bad.vertexCount = 0;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.indexCount = 0;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.archiveBytes = 0;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.archiveBytes = 256ULL * 1024 * 1024 + 1;
        reject(PreviewRangeErrorCode::nativeBudgetExceeded, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.indexCount = 4;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.vertexCount = 4;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.indexCount = uint64_t(std::numeric_limits<uint32_t>::max()) + 3;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.sources = PreviewSourceShape { 2, 3, 0, 8 };
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad.sources = PreviewSourceShape { 1, 6, 0, 8 };
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = empty;
        bad.warningBytes = 1;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.warningBytes = std::numeric_limits<uint64_t>::max();
        reject(PreviewRangeErrorCode::arithmeticOverflow, [&] { planPreviewTransfer(bad, {}); });
        bad = shape;
        bad.sources = PreviewSourceShape { 1, 3, std::numeric_limits<uint64_t>::max(), 8 };
        reject(PreviewRangeErrorCode::arithmeticOverflow, [&] { planPreviewTransfer(bad, {}); });
        bad = empty;
        bad.auxiliaryWireBytes = generationStatisticsPrefixBytes - 1;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
        bad = empty;
        bad.auxiliaryLogicalBytes = bad.auxiliaryWireBytes + 1;
        reject(PreviewRangeErrorCode::invalidCounts, [&] { planPreviewTransfer(bad, {}); });
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

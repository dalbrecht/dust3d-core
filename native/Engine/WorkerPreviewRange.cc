// SPDX-License-Identifier: MIT
#include "WorkerPreviewRange.h"
#include "SourceProvenance.h"
#include "WorkerSessionOutput.h"
#include "WorkerSourceRequest.h"
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(PreviewRangeErrorCode code, const char* cause)
        {
            throw PreviewRangeError(code, cause);
        }
        uint64_t add(uint64_t left, uint64_t right)
        {
            if (right > std::numeric_limits<uint64_t>::max() - left)
                reject(PreviewRangeErrorCode::arithmeticOverflow, "Preview byte addition overflows UInt64.");
            return left + right;
        }
        uint64_t multiply(uint64_t count, uint64_t unit)
        {
            if (count > std::numeric_limits<uint64_t>::max() / unit)
                reject(PreviewRangeErrorCode::arithmeticOverflow, "Preview record charge overflows UInt64.");
            return count * unit;
        }
        void validateBudgets(const PreviewNativeBudgets& budgets)
        {
            if (budgets.maximumIndexedBytes > std::numeric_limits<size_t>::max() || budgets.maximumSourceBytes > DUST3D_MAX_SOURCE_PAYLOAD_BYTES)
                reject(PreviewRangeErrorCode::invalidLimits, "Preview budgets exceed native supported policies.");
        }
        void requireAddressable(uint64_t bytes)
        {
            if (bytes > std::numeric_limits<size_t>::max())
                reject(PreviewRangeErrorCode::addressSpaceExceeded, "Preview layout cannot fit the host address space.");
        }
    }
    PreviewRangeError::PreviewRangeError(PreviewRangeErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    PreviewRecordPlan planPreviewRecords(const PreviewGeometryShape& shape, const PreviewNativeBudgets& budgets)
    {
        validateBudgets(budgets);
        const auto indexedBudget = budgets.maximumIndexedBytes ? budgets.maximumIndexedBytes : 256ULL * 1024 * 1024;
        const auto sourceBudget = budgets.maximumSourceBytes ? budgets.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES;
        PreviewRecordPlan result;
        const bool isEmpty = !shape.vertexCount && !shape.indexCount;
        if (isEmpty) {
            if (shape.sources || shape.warningBytes || shape.nodeCount || shape.partCount)
                reject(PreviewRangeErrorCode::invalidCounts, "Successful empty preview has no geometry/source records.");
        } else {
            if (!shape.vertexCount || !shape.indexCount || shape.indexCount % 3 || shape.indexCount > std::numeric_limits<uint32_t>::max() || shape.vertexCount > shape.indexCount)
                reject(PreviewRangeErrorCode::invalidCounts, "Preview geometry counts cannot describe native indexed triangles.");
            const auto indexBytes = multiply(shape.indexCount, sizeof(uint32_t));
            result.nativeIndexedBytes = add(multiply(shape.vertexCount, sizeof(Dust3DVertex)), indexBytes);
            if (result.nativeIndexedBytes > indexedBudget)
                reject(PreviewRangeErrorCode::nativeBudgetExceeded, "Preview indexed records exceed the selected native budget.");
            const auto geometryLogical = add(add(multiply(shape.vertexCount, sizeof(MeshVertex)), indexBytes), shape.warningBytes);
            result.workerLogicalBytes = add(result.workerLogicalBytes, geometryLogical);
            result.geometryWireBytes = add(add(add(indexedMeshPrefixBytes, multiply(shape.vertexCount, meshVertexBytes)), indexBytes), shape.warningBytes);
            uint64_t primaryWire = result.geometryWireBytes;
            if (shape.sources) {
                const auto& source = *shape.sources;
                if (source.identities != shape.nodeCount || source.corners != shape.indexCount)
                    reject(PreviewRangeErrorCode::invalidCounts, "Preview source counts disagree with geometry membership/corners.");
                result.nativeSourceBytes = add(add(add(multiply(source.identities, sizeof(native::SourceIdentity) + 2), source.textBytes), multiply(source.corners, sizeof(Dust3DSourceCorner))), multiply(source.influences, sizeof(Dust3DSourceInfluence)));
                if (result.nativeSourceBytes > sourceBudget)
                    reject(PreviewRangeErrorCode::nativeBudgetExceeded, "Preview sources exceed their independent native budget.");
                const auto sourceLogical = add(add(add(multiply(source.identities, sizeof(SourceHintIdentity) + 2), source.textBytes), multiply(source.corners, sizeof(SourceHintCorner))), multiply(source.influences, sizeof(SourceHintInfluence)));
                result.workerLogicalBytes = add(result.workerLogicalBytes, sourceLogical);
                result.sourceWireBytes = add(add(add(add(sourceHintPrefixBytes, multiply(source.identities, 8)), source.textBytes), multiply(source.corners, sourceHintCornerBytes)), multiply(source.influences, sourceHintInfluenceBytes));
                primaryWire = add(add(indexedSourcesPrefixBytes, result.geometryWireBytes), result.sourceWireBytes);
            }
            result.primaryWireBytes = primaryWire;
        }
        requireAddressable(result.primaryWireBytes);
        requireAddressable(result.workerLogicalBytes);
        return result;
    }
    PreviewTransferPlan planPreviewTransfer(const PreviewTransferShape& shape, const PreviewNativeBudgets& budgets)
    {
        validateBudgets(budgets);
        if (!shape.archiveBytes || shape.auxiliaryWireBytes < generationStatisticsPrefixBytes || shape.auxiliaryLogicalBytes > shape.auxiliaryWireBytes)
            reject(PreviewRangeErrorCode::invalidCounts, "Preview archive or auxiliary accounting is incomplete.");
        if (shape.archiveBytes > 256ULL * 1024 * 1024)
            reject(PreviewRangeErrorCode::nativeBudgetExceeded, "Preview archive exceeds the native input budget.");
        const auto records = planPreviewRecords({ shape.vertexCount, shape.indexCount, shape.nodeCount, shape.partCount, shape.warningBytes, shape.sources }, budgets);
        PreviewTransferPlan result;
        result.requestPayloadBytes = add(sourceRequestPrefixBytes, shape.archiveBytes);
        result.sessionPayloadBytes = add(add(sessionOutputPrefixBytes, shape.auxiliaryWireBytes), records.primaryWireBytes);
        result.nativeIndexedBytes = records.nativeIndexedBytes;
        result.nativeSourceBytes = records.nativeSourceBytes;
        result.geometryWireBytes = records.geometryWireBytes;
        result.sourceWireBytes = records.sourceWireBytes;
        result.workerLogicalBytes = add(shape.auxiliaryLogicalBytes, records.workerLogicalBytes);
        requireAddressable(result.requestPayloadBytes);
        requireAddressable(result.sessionPayloadBytes);
        requireAddressable(result.workerLogicalBytes);
        return result;
    }
}
}

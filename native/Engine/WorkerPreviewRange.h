// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_RANGE_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_RANGE_H_
#include <cstdint>
#include <optional>
#include <stdexcept>
namespace dust3d {
namespace worker {
    /// Actual source counts and combined node/part UTF8 bytes, without terminators.
    struct PreviewSourceShape {
        uint64_t identities, corners, influences, textBytes;
    };
    /// Trusted scalar observations only; no borrowed buffers or platform objects.
    /// Auxiliary wire/logical totals come from independently validated diagnostic
    /// and statistics codecs, including the mandatory absent-statistics prefix.
    struct PreviewTransferShape {
        uint64_t archiveBytes = 0;
        uint64_t vertexCount = 0, indexCount = 0, nodeCount = 0, partCount = 0, warningBytes = 0;
        std::optional<PreviewSourceShape> sources;
        uint64_t auxiliaryWireBytes = 0, auxiliaryLogicalBytes = 0;
    };
    /// Zero indexed/source policy selects native defaults256MiB/64MiB. Explicit
    /// indexed size_t policies are not capped at their default; source max is256MiB.
    struct PreviewNativeBudgets {
        uint64_t maximumIndexedBytes = 0, maximumSourceBytes = 0;
    };
    /// Exact scalar charges for existing source/session encoding layouts, even
    /// when their current transport/semantic caps cannot admit the result.
    /// Excludes roots, capacity, temporary copies, framing per future chunk and RSS.
    struct PreviewTransferPlan {
        uint64_t requestPayloadBytes = 0, sessionPayloadBytes = 0;
        uint64_t nativeIndexedBytes = 0, nativeSourceBytes = 0;
        uint64_t geometryWireBytes = 0, sourceWireBytes = 0, workerLogicalBytes = 0;
    };
    enum class PreviewRangeErrorCode { invalidLimits,
        invalidCounts,
        nativeBudgetExceeded,
        arithmeticOverflow,
        addressSpaceExceeded };
    class PreviewRangeError : public std::runtime_error {
    public:
        PreviewRangeError(PreviewRangeErrorCode code, const char* cause);
        PreviewRangeErrorCode code() const noexcept { return m_code; }

    private:
        PreviewRangeErrorCode m_code;
    };
    /// Measure actual metadata with checked arithmetic and native budget rules.
    /// Never allocates geometry/archives, calls the engine or changes active caps.
    /// This is accounting, not semantic validation, authentication or admission:
    /// actual vertices/indices/text/diagnostics still require their full codecs.
    /// Throws PreviewRangeError preserving the specific accounting cause.
    /// Example: auto plan = planPreviewTransfer(observedCounts, selectedBudgets);
    PreviewTransferPlan planPreviewTransfer(const PreviewTransferShape& shape, const PreviewNativeBudgets& budgets);
}
}
#endif

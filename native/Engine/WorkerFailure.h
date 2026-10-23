// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_FAILURE_H_
#define DUST3D_NATIVE_WORKER_FAILURE_H_
#include "WorkerProtocol.h"

namespace dust3d {
namespace worker {
    constexpr size_t failurePrefixBytes = 16;
    constexpr uint64_t maximumFailureCauseBytes = maximumDiagnosticBytes - failurePrefixBytes;

    /// Stable wire outcomes; a future native adapter must map statuses explicitly.
    enum class FailureStatus : uint16_t { invalidDocument = 1,
        unsupportedAsset = 2,
        generationFailed = 3,
        limitExceeded = 4,
        noGeometry = 5,
        cancelled = 6 };
    /// Original diagnostic bytes are opaque data, never commands, format strings or logs.
    struct FailurePayload {
        FailureStatus status = FailureStatus::generationFailed;
        std::vector<uint8_t> cause;
    };
    /// Logical body limits exclude roots/capacities/allocator overhead and do not cap RSS.
    struct FailurePayloadLimits {
        uint64_t maximumCauseBytes = 4096 - failurePrefixBytes;
        uint64_t maximumWireBytes = 4096;
    };
    enum class FailurePayloadErrorCode { invalidLimits,
        invalidInput,
        unsupportedVersion,
        invalidLayout,
        invalidStatus,
        limitExceeded,
        truncated,
        trailingData };
    /// Typed payload validation error; messages never contain diagnostic/body bytes.
    class FailurePayloadError : public std::runtime_error {
    public:
        FailurePayloadError(FailurePayloadErrorCode code, const char* message);
        FailurePayloadErrorCode code() const noexcept { return m_code; }

    private:
        FailurePayloadErrorCode m_code;
    };
    /// Encode a known non-success status and bounded original cause using a version-one schema.
    /// Empty and non-UTF-8 causes survive unchanged. Throws FailurePayloadError or allocation errors.
    std::vector<uint8_t> encodeFailurePayload(const FailurePayload& failure, const FailurePayloadLimits& limits);
    /// Decode one exact complete payload into owned bytes; never log or interpret the cause.
    /// Rejects NULL/truncated/trailing spans, unsupported fields, success/unknown statuses and quotas.
    /// This does not validate frame identity, retire a pending request or provide process containment.
    FailurePayload decodeFailurePayload(const uint8_t* bytes, size_t size, const FailurePayloadLimits& limits);
}
}
#endif

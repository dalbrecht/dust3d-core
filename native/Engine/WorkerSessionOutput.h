// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_OUTPUT_H_
#define DUST3D_NATIVE_WORKER_SESSION_OUTPUT_H_
#include "WorkerSessionResult.h"
namespace dust3d {
namespace worker {
    constexpr size_t sessionOutputPrefixBytes = 64;
    /// Atomic final outcome with independent original source/statistics diagnostics.
    /// monostate means successful emptiness; unavailable statistics never imply zeros.
    struct SessionOutput {
        std::variant<std::monostate, IndexedSourcesPayload, IndexedMeshPayload, FailurePayload> outcome;
        std::optional<FailurePayload> sourceIssue;
        std::optional<GenerationStatistics> statistics;
        std::optional<FailurePayload> statisticsIssue;
    };
    struct SessionOutputLimits {
        IndexedSourcesPayloadLimits sourced;
        IndexedMeshPayloadLimits ordinary;
        FailurePayloadLimits failure;
        GenerationStatisticsLimits statistics;
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class SessionOutputErrorCode { invalidLimits,
        invalidInput,
        invalidOutcome,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded };
    class SessionOutputError : public std::runtime_error {
    public:
        SessionOutputError(SessionOutputErrorCode code, const char* cause);
        SessionOutputErrorCode code() const noexcept { return m_code; }

    private:
        SessionOutputErrorCode m_code;
    };
    /// Encode one complete explicit-endian v1 outcome with independent inner and
    /// aggregate quotas. Logical charge includes geometry, hints and diagnostic bytes,
    /// excluding root/scalar storage, capacities, temporary encoding workspace and RSS.
    /// Preserves original typed inner errors; no native calls, authentication or opcode.
    /// Throws SessionOutputError, inner codec or allocation errors.
    /// Example: auto wire = encodeSessionOutput(value, {});
    std::vector<uint8_t> encodeSessionOutput(const SessionOutput& value, const SessionOutputLimits& limits);
    /// Validate full envelope framing, bounded diagnostics, outcome invariants and
    /// remaining aggregate geometry allowance before retaining geometry arrays.
    /// Returns independent values; typed inner errors propagate without replacement.
    /// Diagnostic copies are bounded separately before aggregate admission.
    /// Example: auto value = decodeSessionOutput(wire.data(), wire.size(), {});
    SessionOutput decodeSessionOutput(const uint8_t* bytes, size_t size, const SessionOutputLimits& limits);
}
}
#endif

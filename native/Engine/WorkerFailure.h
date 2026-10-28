// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_FAILURE_H_
#define DUST3D_NATIVE_WORKER_FAILURE_H_
#include "WorkerProtocol.h"
#include <optional>
#include <string>

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
    constexpr size_t detailedFailurePrefixBytes = 24;
    /// Optional source-native diagnostic classification, independent of the outcome status.
    enum class FailureDomain : uint16_t { native = 1,
        imageDecoder = 2,
        producer = 3,
        surface = 4,
        materials = 5,
        render = 6,
        standard = 7,
        legacyBaked = 8,
        proceduralImages = 9,
        bakedTextures = 10,
        uv = 11,
        legacyProducer = 12,
        mesh = 13,
        sourceBaked = 14,
        generationStatistics = 15,
        indexedMesh = 16,
        sourceHints = 17,
        indexedSources = 18,
        previewRange = 19 };
    struct FailureDetail {
        FailureDomain domain;
        int32_t code = 0;
        uint32_t status = 0;
        uint8_t causeTruncated = 0;
    };
    constexpr size_t contextualFailurePrefixBytes = 32;
    constexpr size_t maximumFailureResourceIdentityBytes = 38;
    enum class FailureResourceRole : uint16_t { proceduralImage = 1,
        importedImage = 2 };
    struct FailureResourceContext {
        FailureResourceRole role;
        std::string identity;
    };
    /// Original diagnostic bytes are opaque data, never commands, format strings or logs.
    struct FailurePayload {
        FailureStatus status = FailureStatus::generationFailed;
        std::vector<uint8_t> cause;
        std::optional<FailureDetail> detail = std::nullopt;
        std::optional<FailureResourceContext> context = std::nullopt;
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
    /// Encode a known non-success status and bounded original cause using v1 without details, v2 with details, v3 with separate resource context.
    /// Empty and non-UTF-8 causes survive unchanged. Throws FailurePayloadError or allocation errors.
    std::vector<uint8_t> encodeFailurePayload(const FailurePayload& failure, const FailurePayloadLimits& limits);
    /// Decode one exact complete payload into owned bytes; never log or interpret the cause.
    /// Rejects NULL/truncated/trailing spans, unsupported fields, success/unknown statuses and quotas.
    /// This does not validate frame identity, retire a pending request or provide process containment.
    FailurePayload decodeFailurePayload(const uint8_t* bytes, size_t size, const FailurePayloadLimits& limits);
}
}
#endif

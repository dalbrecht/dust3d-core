// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_HINTS_H_
#define DUST3D_NATIVE_WORKER_SOURCE_HINTS_H_
#include "WorkerProtocol.h"
#include <limits>
#include <string>

namespace dust3d {
namespace worker {
    constexpr size_t sourceHintPrefixBytes = 40;
    constexpr size_t sourceHintCornerBytes = 24;
    constexpr size_t sourceHintInfluenceBytes = 24;
    constexpr uint64_t noSourceIdentity = std::numeric_limits<uint64_t>::max();

    /// Numeric values match source provenance. Unrecognized values remain opaque bounded hints.
    enum class SourceHintKind : uint32_t { unknown = 0,
        nodeHint = 1,
        weightedHint = 2,
        ambiguous = 3 };
    struct SourceHintIdentity {
        std::string node, part;
    };
    struct SourceHintCorner {
        SourceHintKind kind = SourceHintKind::unknown;
        uint64_t firstInfluence = 0, influenceCount = 0;
    };
    struct SourceHintInfluence {
        SourceHintKind kind = SourceHintKind::unknown;
        uint64_t identityIndex = noSourceIdentity;
        float weight = 0;
    };
    /// Independent values; aliases, ordering and nonnegative weights are never normalized.
    struct SourceHintPayload {
        std::vector<SourceHintIdentity> identities;
        std::vector<SourceHintCorner> corners;
        std::vector<SourceHintInfluence> influences;
    };
    /// Logical retained charge includes record objects, text and two terminators per identity.
    /// Excludes roots, capacity, validation workspace and process RSS.
    struct SourceHintPayloadLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 64ULL * 1024 * 1024;
        uint64_t maximumIdentities = 1000000;
        uint64_t maximumCorners = 3000000;
        uint64_t maximumInfluences = 8000000;
        uint64_t maximumTextBytes = 8ULL * 1024 * 1024;
        uint64_t maximumIdentityBytes = 1024;
    };
    struct SourceHintPayloadSize {
        uint64_t wireBytes, decodedBytes;
    };
    enum class SourceHintPayloadErrorCode { invalidLimits,
        invalidInput,
        unsupportedVersion,
        invalidLayout,
        invalidIdentity,
        invalidReference,
        invalidWeight,
        invalidCorner,
        limitExceeded };
    class SourceHintPayloadError : public std::runtime_error {
    public:
        SourceHintPayloadError(SourceHintPayloadErrorCode code, const char* cause);
        SourceHintPayloadErrorCode code() const noexcept { return m_code; }

    private:
        SourceHintPayloadErrorCode m_code;
    };
    /// Validate supported positive quotas without copying data or running the engine.
    /// @param limits Borrowed count/text/wire/logical policies.
    /// @throws SourceHintPayloadError(invalidLimits) with the original cause.
    /// Example: validateSourceHintPayloadLimits({});
    void validateSourceHintPayloadLimits(const SourceHintPayloadLimits& limits);
    /// Validate complete owned associations and measure wire/retained charges before encoding.
    /// @param value Borrowed source identities, corners and influences.
    /// @param limits Positive supported policies, not process-memory bounds.
    /// @return Checked independent wire and logical decoded sizes.
    /// @throws SourceHintPayloadError for invalid values or quotas.
    /// Example: auto size = measureSourceHintPayload(value, {});
    SourceHintPayloadSize measureSourceHintPayload(const SourceHintPayload& value, const SourceHintPayloadLimits& limits);
    /// Encode a version-one explicit-endian source payload after full validation.
    /// @param value Borrowed values; no source geometry or archive is evaluated.
    /// @param limits Supported byte/count/text policies.
    /// @return Independent complete wire bytes, with zero reserved fields.
    /// @throws SourceHintPayloadError or original allocation exception; no partial result.
    /// Example: auto bytes = encodeSourceHintPayload(value, {});
    std::vector<uint8_t> encodeSourceHintPayload(const SourceHintPayload& value, const SourceHintPayloadLimits& limits);
    /// Validate complete framing and semantics before constructing returned strings/arrays.
    /// @param bytes Borrowed exact wire storage for this call.
    /// @param size Readable byte count; trailing/truncated bytes reject.
    /// @param limits Supported wire/logical/count/text policies.
    /// @return Independent owned values; pairing with geometry is a separate admission step.
    /// @throws SourceHintPayloadError or original allocation exception; no engine work.
    /// Example: auto value = decodeSourceHintPayload(bytes.data(), bytes.size(), {});
    SourceHintPayload decodeSourceHintPayload(const uint8_t* bytes, size_t size, const SourceHintPayloadLimits& limits);
}
}
#endif

// SPDX-License-Identifier: MIT
#include "WorkerFailure.h"
#include "WorkerUtf8.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(FailurePayloadErrorCode code, const char* message)
        {
            throw FailurePayloadError(code, message);
        }
        void validateLimits(const FailurePayloadLimits& limits)
        {
            if (!limits.maximumCauseBytes || limits.maximumCauseBytes > maximumFailureCauseBytes
                || !limits.maximumWireBytes || limits.maximumWireBytes > maximumDiagnosticBytes)
                reject(FailurePayloadErrorCode::invalidLimits, "Worker failure limits are outside their supported range.");
        }
        void validateStatus(FailureStatus status)
        {
            switch (status) {
            case FailureStatus::invalidDocument:
            case FailureStatus::unsupportedAsset:
            case FailureStatus::generationFailed:
            case FailureStatus::limitExceeded:
            case FailureStatus::noGeometry:
            case FailureStatus::cancelled:
                return;
            }
            reject(FailurePayloadErrorCode::invalidStatus, "Worker failure requires a known non-success status.");
        }
        void validateDetail(const FailureDetail& detail)
        {
            if (static_cast<uint16_t>(detail.domain) < 1 || static_cast<uint16_t>(detail.domain) > 14 || detail.causeTruncated > 1)
                reject(FailurePayloadErrorCode::invalidLayout, "Worker failure diagnostic domain or truncation flag is invalid.");
        }
        void validateContext(const FailureResourceContext& value)
        {
            if ((value.role != FailureResourceRole::proceduralImage && value.role != FailureResourceRole::importedImage)
                || value.identity.empty() || value.identity.size() > 38 || !detail::isIdentityUtf8(value.identity))
                reject(FailurePayloadErrorCode::invalidLayout, "Worker failure resource context is invalid.");
        }
        size_t plan(uint64_t causeBytes, size_t prefix, const FailurePayloadLimits& limits)
        {
            if (causeBytes > limits.maximumCauseBytes || prefix + causeBytes > limits.maximumWireBytes)
                reject(FailurePayloadErrorCode::limitExceeded, "Worker failure payload exceeds its configured quota.");
            const auto total = prefix + causeBytes;
            if (total > std::numeric_limits<size_t>::max())
                reject(FailurePayloadErrorCode::limitExceeded, "Worker failure cannot fit the address space.");
            return static_cast<size_t>(total);
        }
    }
    FailurePayloadError::FailurePayloadError(FailurePayloadErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    std::vector<uint8_t> encodeFailurePayload(const FailurePayload& failure, const FailurePayloadLimits& limits)
    {
        validateLimits(limits);
        validateStatus(failure.status);
        if (failure.detail)
            validateDetail(*failure.detail);
        if (failure.context) {
            validateContext(*failure.context);
            if (!failure.detail)
                reject(FailurePayloadErrorCode::invalidLayout, "Contextual failure requires original diagnostic details.");
        }
        const auto prefix = failure.context ? contextualFailurePrefixBytes : failure.detail ? detailedFailurePrefixBytes
                                                                                            : failurePrefixBytes;
        const auto contextBytes = failure.context ? failure.context->identity.size() : 0;
        const auto count = plan(failure.cause.size(), prefix + contextBytes, limits);
        std::vector<uint8_t> result(count);
        detail::writeUnsigned(result.data(), failure.context ? 3 : failure.detail ? 2
                                                                                  : 1,
            2);
        detail::writeUnsigned(result.data() + 2, prefix, 2);
        detail::writeUnsigned(result.data() + 4, static_cast<uint16_t>(failure.status), 2);
        detail::writeUnsigned(result.data() + 12, failure.cause.size(), 4);
        if (failure.detail) {
            const auto& value = *failure.detail;
            detail::writeUnsigned(result.data() + 6, static_cast<uint16_t>(value.domain), 2);
            uint32_t code;
            std::memcpy(&code, &value.code, sizeof(code));
            detail::writeUnsigned(result.data() + 8, code, 4);
            detail::writeUnsigned(result.data() + 16, value.status, 4);
            detail::writeUnsigned(result.data() + 20, value.causeTruncated, 4);
        }
        if (failure.context) {
            detail::writeUnsigned(result.data() + 24, static_cast<uint16_t>(failure.context->role), 2);
            detail::writeUnsigned(result.data() + 26, contextBytes, 2);
            std::copy(failure.context->identity.begin(), failure.context->identity.end(), result.begin() + prefix);
        }
        std::copy(failure.cause.begin(), failure.cause.end(), result.begin() + prefix + contextBytes);
        return result;
    }
    FailurePayload decodeFailurePayload(const uint8_t* bytes, size_t size, const FailurePayloadLimits& limits)
    {
        validateLimits(limits);
        if (size < failurePrefixBytes)
            reject(FailurePayloadErrorCode::truncated, "Worker failure prefix is incomplete.");
        if (!bytes)
            reject(FailurePayloadErrorCode::invalidInput, "Worker failure bytes are missing.");
        const auto version = detail::readUnsigned(bytes, 2);
        if (version != 1 && version != 2 && version != 3)
            reject(FailurePayloadErrorCode::unsupportedVersion, "Worker failure schema version is unsupported.");
        const auto prefix = version == 1 ? failurePrefixBytes : version == 2 ? detailedFailurePrefixBytes
                                                                             : contextualFailurePrefixBytes;
        if (size < prefix)
            reject(FailurePayloadErrorCode::truncated, "Worker detailed failure prefix is incomplete.");
        if (detail::readUnsigned(bytes + 2, 2) != prefix || (version == 1 && (detail::readUnsigned(bytes + 6, 2) || detail::readUnsigned(bytes + 8, 4))))
            reject(FailurePayloadErrorCode::invalidLayout, "Worker failure prefix, flags or reserved fields are invalid.");
        std::optional<FailureDetail> metadata;
        if (version >= 2) {
            const auto code = static_cast<uint32_t>(detail::readUnsigned(bytes + 8, 4));
            int32_t signedCode;
            std::memcpy(&signedCode, &code, sizeof(code));
            const auto truncated = detail::readUnsigned(bytes + 20, 4);
            if (truncated > 1)
                reject(FailurePayloadErrorCode::invalidLayout, "Worker failure diagnostic truncation flag is invalid.");
            metadata = FailureDetail { static_cast<FailureDomain>(detail::readUnsigned(bytes + 6, 2)), signedCode,
                static_cast<uint32_t>(detail::readUnsigned(bytes + 16, 4)), static_cast<uint8_t>(truncated) };
            validateDetail(*metadata);
        }
        const auto status = static_cast<FailureStatus>(detail::readUnsigned(bytes + 4, 2));
        validateStatus(status);
        size_t contextBytes = 0;
        std::optional<FailureResourceContext> context;
        if (version == 3) {
            contextBytes = static_cast<size_t>(detail::readUnsigned(bytes + 26, 2));
            if (detail::readUnsigned(bytes + 28, 4) || !contextBytes || contextBytes > 38)
                reject(FailurePayloadErrorCode::invalidLayout, "Worker failure resource layout is invalid.");
            if (size - prefix < contextBytes)
                reject(FailurePayloadErrorCode::truncated, "Worker failure resource identity is incomplete.");
            context = FailureResourceContext { static_cast<FailureResourceRole>(detail::readUnsigned(bytes + 24, 2)),
                std::string(reinterpret_cast<const char*>(bytes + prefix), contextBytes) };
            validateContext(*context);
        }
        const auto count = plan(detail::readUnsigned(bytes + 12, 4), prefix + contextBytes, limits);
        if (size < count)
            reject(FailurePayloadErrorCode::truncated, "Worker failure cause is incomplete.");
        if (size != count)
            reject(FailurePayloadErrorCode::trailingData, "Worker failure payload contains trailing bytes.");
        return { status, std::vector<uint8_t>(bytes + prefix + contextBytes, bytes + size), metadata, std::move(context) };
    }
}
}

// SPDX-License-Identifier: MIT
#include "WorkerFailure.h"
#include "WorkerWire.h"
#include <algorithm>
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
        size_t plan(uint64_t causeBytes, const FailurePayloadLimits& limits)
        {
            if (causeBytes > limits.maximumCauseBytes || failurePrefixBytes + causeBytes > limits.maximumWireBytes)
                reject(FailurePayloadErrorCode::limitExceeded, "Worker failure payload exceeds its configured quota.");
            const auto total = failurePrefixBytes + causeBytes;
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
        const auto count = plan(failure.cause.size(), limits);
        std::vector<uint8_t> result(count);
        detail::writeUnsigned(result.data(), 1, 2);
        detail::writeUnsigned(result.data() + 2, failurePrefixBytes, 2);
        detail::writeUnsigned(result.data() + 4, static_cast<uint16_t>(failure.status), 2);
        detail::writeUnsigned(result.data() + 12, failure.cause.size(), 4);
        std::copy(failure.cause.begin(), failure.cause.end(), result.begin() + failurePrefixBytes);
        return result;
    }
    FailurePayload decodeFailurePayload(const uint8_t* bytes, size_t size, const FailurePayloadLimits& limits)
    {
        validateLimits(limits);
        if (size < failurePrefixBytes)
            reject(FailurePayloadErrorCode::truncated, "Worker failure prefix is incomplete.");
        if (!bytes)
            reject(FailurePayloadErrorCode::invalidInput, "Worker failure bytes are missing.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(FailurePayloadErrorCode::unsupportedVersion, "Worker failure schema version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != failurePrefixBytes || detail::readUnsigned(bytes + 6, 2) || detail::readUnsigned(bytes + 8, 4))
            reject(FailurePayloadErrorCode::invalidLayout, "Worker failure prefix, flags or reserved fields are invalid.");
        const auto status = static_cast<FailureStatus>(detail::readUnsigned(bytes + 4, 2));
        validateStatus(status);
        const auto count = plan(detail::readUnsigned(bytes + 12, 4), limits);
        if (size < count)
            reject(FailurePayloadErrorCode::truncated, "Worker failure cause is incomplete.");
        if (size != count)
            reject(FailurePayloadErrorCode::trailingData, "Worker failure payload contains trailing bytes.");
        return { status, std::vector<uint8_t>(bytes + failurePrefixBytes, bytes + size) };
    }
}
}

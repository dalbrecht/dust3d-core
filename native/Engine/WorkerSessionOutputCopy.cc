// SPDX-License-Identifier: MIT
#include "WorkerSessionOutputCopy.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        FailurePayload boundFailure(FailurePayload value, const FailurePayloadLimits& limits)
        {
            // Encoding validates exact prefix/context and policies before subtraction.
            auto allowance = [&] {
                FailurePayload prefix { value.status, {}, value.detail, value.context };
                const auto bytes = encodeFailurePayload(prefix, limits).size();
                return std::min<uint64_t>(limits.maximumCauseBytes, limits.maximumWireBytes - bytes);
            };
            auto maximum = allowance();
            if (value.cause.size() > maximum) {
                if (!value.detail) {
                    value.detail = FailureDetail { FailureDomain::standard, -1, 0, 1 };
                    maximum = allowance();
                }
                value.cause.resize(static_cast<size_t>(maximum));
                value.detail->causeTruncated = 1;
            }
            (void)encodeFailurePayload(value, limits);
            return value;
        }
        FailurePayload copyMessage(const char* cause, FailureDomain domain, int32_t code, FailureStatus status, const FailurePayloadLimits& limits)
        {
            FailurePayload result { status, {}, FailureDetail { domain, code, 0, 0 } };
            const auto prefix = encodeFailurePayload(result, limits).size();
            const auto maximum = static_cast<size_t>(std::min<uint64_t>(limits.maximumCauseBytes, limits.maximumWireBytes - prefix));
            size_t count = 0;
            while (count < maximum && cause[count])
                ++count;
            result.cause.assign(cause, cause + count);
            result.detail->causeTruncated = cause[count] ? 1 : 0;
            return result;
        }
        FailurePayload copyException(const std::exception_ptr& exception, const FailurePayloadLimits& limits)
        {
            try {
                std::rethrow_exception(exception);
            } catch (const NativeSessionError& cause) {
                return boundFailure(cause.failure(), limits);
            } catch (const GenerationStatisticsError& cause) {
                return copyMessage(cause.what(), FailureDomain::generationStatistics, static_cast<int32_t>(cause.code()), FailureStatus::generationFailed, limits);
            } catch (const std::bad_alloc& cause) {
                return copyMessage(cause.what(), FailureDomain::standard, 1, FailureStatus::limitExceeded, limits);
            } catch (const std::exception& cause) {
                return copyMessage(cause.what(), FailureDomain::standard, 0, FailureStatus::generationFailed, limits);
            } catch (...) {
                return copyMessage("Statistics exception has no available diagnostic.", FailureDomain::standard, -1, FailureStatus::generationFailed, limits);
            }
        }
    }
    SessionOutput copySessionOutput(NativeSessionGeneration generated, const FailurePayloadLimits& limits)
    {
        (void)encodeFailurePayload(FailurePayload { FailureStatus::generationFailed, {}, FailureDetail { FailureDomain::standard, 0, 0, 0 } }, limits);
        if (static_cast<unsigned>(generated.mesh.has_value()) + static_cast<unsigned>(generated.ordinaryMesh.has_value()) + static_cast<unsigned>(generated.failure.has_value()) + static_cast<unsigned>(generated.isEmpty) != 1
            || (generated.statistics && generated.statisticsIssue) || (generated.statisticsException && (generated.statistics || generated.statisticsIssue)))
            throw SessionOutputError(SessionOutputErrorCode::invalidOutcome, "Native session output has inconsistent outcomes or observations.");
        SessionOutput result;
        if (generated.mesh)
            result.outcome = std::move(*generated.mesh);
        else if (generated.ordinaryMesh)
            result.outcome = std::move(*generated.ordinaryMesh);
        else if (generated.failure)
            result.outcome = std::move(*generated.failure);
        result.sourceIssue = std::move(generated.sourceIssue);
        result.statistics = std::move(generated.statistics);
        result.statisticsIssue = generated.statisticsException ? copyException(generated.statisticsException, limits) : std::move(generated.statisticsIssue);
        return result;
    }
}
}

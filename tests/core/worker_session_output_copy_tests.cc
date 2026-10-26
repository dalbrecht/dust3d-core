// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerSessionOutputCopy.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session output copy contract");
}
int main()
{
    try {
        NativeSessionGeneration empty;
        empty.isEmpty = true;
        auto copied = copySessionOutput(std::move(empty), {});
        require(std::holds_alternative<std::monostate>(copied.outcome));
        NativeSessionGeneration error;
        error.isEmpty = true;
        error.statisticsException = std::make_exception_ptr(GenerationStatisticsError(GenerationStatisticsErrorCode::invalidStatistics, "original statistics"));
        copied = copySessionOutput(std::move(error), {});
        require(copied.statisticsIssue && !copied.statistics && copied.statisticsIssue->detail->domain == FailureDomain::generationStatistics && copied.statisticsIssue->detail->code == static_cast<int32_t>(GenerationStatisticsErrorCode::invalidStatistics));
        require(std::string(copied.statisticsIssue->cause.begin(), copied.statisticsIssue->cause.end()) == "original statistics");
        NativeSessionGeneration native;
        native.isEmpty = true;
        native.statisticsException = std::make_exception_ptr(NativeSessionError(FailurePayload { FailureStatus::generationFailed, { 0, 255 }, FailureDetail { FailureDomain::native, 3, 3, 0 } }));
        copied = copySessionOutput(std::move(native), {});
        require(copied.statisticsIssue->cause == std::vector<uint8_t> { 0, 255 } && copied.statisticsIssue->detail->domain == FailureDomain::native);
        NativeSessionGeneration standard;
        standard.isEmpty = true;
        standard.statisticsException = std::make_exception_ptr(std::runtime_error("original standard cause"));
        FailurePayloadLimits tight;
        tight.maximumCauseBytes = 4;
        copied = copySessionOutput(std::move(standard), tight);
        require(copied.statisticsIssue->cause == std::vector<uint8_t> { 'o', 'r', 'i', 'g' } && copied.statisticsIssue->detail->causeTruncated == 1);
        NativeSessionGeneration noDetails;
        noDetails.isEmpty = true;
        noDetails.statisticsException = std::make_exception_ptr(NativeSessionError(FailurePayload { FailureStatus::generationFailed, { 42 } }));
        copied = copySessionOutput(std::move(noDetails), {});
        require(!copied.statisticsIssue->detail && copied.statisticsIssue->cause == std::vector<uint8_t> { 42 });
        for (bool details : { false, true }) {
            NativeSessionGeneration bounded;
            bounded.isEmpty = true;
            FailurePayload original { FailureStatus::generationFailed, { 1, 2, 3 } };
            if (details)
                original.detail = FailureDetail { FailureDomain::native, 3, 3, 0 };
            bounded.statisticsException = std::make_exception_ptr(NativeSessionError(original));
            FailurePayloadLimits quota;
            quota.maximumCauseBytes = 1;
            copied = copySessionOutput(std::move(bounded), quota);
            require(copied.statisticsIssue->cause == std::vector<uint8_t> { 1 } && copied.statisticsIssue->detail->causeTruncated == 1);
        }
        NativeSessionGeneration unclassified;
        unclassified.isEmpty = true;
        unclassified.statisticsException = std::make_exception_ptr(42);
        copied = copySessionOutput(std::move(unclassified), {});
        require(copied.statisticsIssue->detail->code == -1 && !copied.statisticsIssue->cause.empty());
        NativeSessionGeneration sourced;
        sourced.mesh = IndexedSourcesPayload {};
        sourced.mesh->geometry.warning = "owned source output";
        sourced.statistics = GenerationStatistics {};
        sourced.statistics->reusedComponents = 9;
        copied = copySessionOutput(std::move(sourced), {});
        require(std::get<IndexedSourcesPayload>(copied.outcome).geometry.warning == "owned source output" && copied.statistics->reusedComponents == 9);
        NativeSessionGeneration ordinary;
        ordinary.ordinaryMesh = IndexedMeshPayload {};
        ordinary.ordinaryMesh->warning = "owned fallback output";
        ordinary.sourceIssue = FailurePayload { FailureStatus::limitExceeded, { 6 } };
        ordinary.statisticsIssue = FailurePayload { FailureStatus::generationFailed, { 7 } };
        copied = copySessionOutput(std::move(ordinary), {});
        require(std::get<IndexedMeshPayload>(copied.outcome).warning == "owned fallback output" && copied.sourceIssue->cause[0] == 6 && copied.statisticsIssue->cause[0] == 7);
        NativeSessionGeneration primary;
        primary.failure = FailurePayload { FailureStatus::unsupportedAsset, { 8 } };
        copied = copySessionOutput(std::move(primary), {});
        require(std::get<FailurePayload>(copied.outcome).cause[0] == 8);
        for (int conflict = 0; conflict < 3; ++conflict) {
            NativeSessionGeneration invalid;
            invalid.isEmpty = true;
            if (conflict != 2)
                invalid.statistics = GenerationStatistics {};
            if (conflict != 1)
                invalid.statisticsIssue = FailurePayload {};
            if (conflict)
                invalid.statisticsException = std::make_exception_ptr(std::runtime_error("conflicting observation"));
            bool rejected = false;
            try {
                (void)copySessionOutput(std::move(invalid), {});
            } catch (const SessionOutputError& cause) {
                rejected = cause.code() == SessionOutputErrorCode::invalidOutcome;
            }
            require(rejected);
        }
        NativeSessionGeneration invalid;
        bool rejected = false;
        try {
            (void)copySessionOutput(std::move(invalid), {});
        } catch (const SessionOutputError& cause) {
            rejected = cause.code() == SessionOutputErrorCode::invalidOutcome;
        }
        require(rejected);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerIndexedSources.h>
#include <native/Engine/WorkerPreviewFailure.h>
#include <native/Engine/WorkerPreviewRange.h>
#include <native/Engine/WorkerSessionResult.h>
using namespace dust3d::worker;
namespace {
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Preview failure receipt contract");
}
template <typename Error>
void check(Error error, FailureDomain domain, FailureStatus status)
{
    const auto value = copyPreviewGenerationFailure(std::make_exception_ptr(error));
    require(value.detail && value.detail->domain == domain && value.detail->code == static_cast<int32_t>(error.code())
        && value.detail->status == 0 && !value.detail->causeTruncated && value.status == status);
    require(std::string(value.cause.begin(), value.cause.end()) == error.what());
    const auto wire = encodeFailurePayload(value, {});
    const auto copied = decodeFailurePayload(wire.data(), wire.size(), {});
    require(copied.detail->domain == domain && copied.detail->code == value.detail->code && copied.cause == value.cause);
}
}
int main()
{
    try {
        for (auto code : { IndexedMeshPayloadErrorCode::limitExceeded, IndexedMeshPayloadErrorCode::invalidGeometry })
            check(IndexedMeshPayloadError(code, "original indexed copy cause"), FailureDomain::indexedMesh,
                code == IndexedMeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed);
        for (auto code : { IndexedSourcesPayloadErrorCode::limitExceeded, IndexedSourcesPayloadErrorCode::countMismatch })
            check(IndexedSourcesPayloadError(code, "original pair cause"), FailureDomain::indexedSources,
                code == IndexedSourcesPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed);
        for (auto code : { SourceHintPayloadErrorCode::limitExceeded, SourceHintPayloadErrorCode::invalidReference })
            check(SourceHintPayloadError(code, "original source cause"), FailureDomain::sourceHints,
                code == SourceHintPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed);
        for (auto code : { MeshPayloadErrorCode::limitExceeded, MeshPayloadErrorCode::invalidGeometry })
            check(MeshPayloadError(code, "original mesh cause"), FailureDomain::mesh,
                code == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed);
        for (auto code : { PreviewRangeErrorCode::invalidCounts, PreviewRangeErrorCode::invalidLimits, PreviewRangeErrorCode::nativeBudgetExceeded,
                 PreviewRangeErrorCode::arithmeticOverflow, PreviewRangeErrorCode::addressSpaceExceeded })
            check(PreviewRangeError(code, "original range cause"), FailureDomain::previewRange,
                code == PreviewRangeErrorCode::invalidCounts || code == PreviewRangeErrorCode::invalidLimits ? FailureStatus::generationFailed : FailureStatus::limitExceeded);
        const auto longCause = copyPreviewGenerationFailure(std::make_exception_ptr(std::logic_error(std::string(5000, 'q'))));
        require(longCause.detail->domain == FailureDomain::standard && longCause.detail->code == 0 && longCause.detail->causeTruncated);
        require(encodeFailurePayload(longCause, {}).size() == FailurePayloadLimits {}.maximumWireBytes);
        const auto unknown = copyPreviewGenerationFailure(std::make_exception_ptr(42));
        require(unknown.detail->domain == FailureDomain::standard && unknown.detail->code == -1 && !unknown.cause.empty());
        const auto sourceIssue = copyPreviewGenerationFailure(std::make_exception_ptr(PreviewRangeError(PreviewRangeErrorCode::nativeBudgetExceeded, "original source budget cause")));
        size_t fallbackCalls = 0;
        auto empty = resolveSessionGeneration(sourceIssue, [&]() -> SessionIndexedResult {
            ++fallbackCalls;
            return FailurePayload { FailureStatus::noGeometry, { 'e' } };
        });
        require(fallbackCalls == 1 && empty.isEmpty && empty.sourceIssue->detail->domain == FailureDomain::previewRange && empty.sourceIssue->cause == sourceIssue.cause);
        const auto ordinaryIssue = copyPreviewGenerationFailure(std::make_exception_ptr(IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidGeometry, "original ordinary cause")));
        auto failed = resolveSessionGeneration(sourceIssue, [&]() -> SessionIndexedResult { return ordinaryIssue; });
        require(failed.sourceIssue->cause == sourceIssue.cause && failed.failure->cause == ordinaryIssue.cause && failed.failure->detail->domain == FailureDomain::indexedMesh);
        try {
            (void)copyPreviewGenerationFailure(nullptr);
            throw std::runtime_error("Absent exception accepted");
        } catch (const ProtocolError& cause) {
            require(cause.code() == ProtocolErrorCode::invalidInput);
        }
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

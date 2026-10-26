// SPDX-License-Identifier: MIT
#include <native/Engine/WorkerSessionResult.h>
#include <stdexcept>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session result contract");
}
int main()
{
    int calls = 0;
    FailurePayload original { FailureStatus::generationFailed, { 0, 255 }, FailureDetail { FailureDomain::native, 3, 7, 0 }, FailureResourceContext { FailureResourceRole::importedImage, "image" } };
    SessionIndexedResult fallbackResult = IndexedMeshPayload {};
    auto fallback = [&]() -> SessionIndexedResult { ++calls; return fallbackResult; };
    auto success = resolveSessionGeneration(IndexedSourcesPayload {}, fallback);
    require(success.mesh.has_value() && calls == 0 && !success.failure && !success.isEmpty);
    for (auto status : { FailureStatus::invalidDocument, FailureStatus::unsupportedAsset, FailureStatus::cancelled, FailureStatus::noGeometry }) {
        auto cause = original;
        cause.status = status;
        auto result = resolveSessionGeneration(cause, fallback);
        require(calls == 0 && result.isEmpty == (status == FailureStatus::noGeometry));
        require(result.failure.has_value() == (status != FailureStatus::noGeometry));
    }
    for (auto status : { FailureStatus::generationFailed, FailureStatus::limitExceeded }) {
        original.status = status;
        fallbackResult = IndexedMeshPayload {};
        auto result = resolveSessionGeneration(original, fallback);
        require(result.ordinaryMesh.has_value() && result.sourceIssue->cause == original.cause);
        require(result.sourceIssue->detail->status == 7 && result.sourceIssue->context->identity == "image");
        fallbackResult = FailurePayload { FailureStatus::unsupportedAsset, { 42 } };
        auto failed = resolveSessionGeneration(original, fallback);
        require(failed.failure->cause == std::vector<uint8_t> { 42 } && failed.sourceIssue->cause == original.cause);
        fallbackResult = FailurePayload { FailureStatus::noGeometry, {} };
        auto empty = resolveSessionGeneration(original, fallback);
        require(empty.isEmpty && empty.sourceIssue && !empty.failure);
    }
    require(calls == 6);
    attachSessionStatistics(success, original);
    require(success.mesh && !success.statistics && success.statisticsIssue->cause == original.cause);
    attachSessionStatistics(success, GenerationStatistics {});
    require(success.mesh && success.statistics && !success.statisticsIssue && success.statistics->reusedComponents == 0);
    auto observationError = std::make_exception_ptr(std::runtime_error("original statistics cause"));
    attachSessionStatistics(success, observationError);
    require(success.mesh && !success.statistics && !success.statisticsIssue && success.statisticsException);
    bool preserved = false;
    try {
        std::rethrow_exception(success.statisticsException);
    } catch (const std::runtime_error& cause) {
        preserved = std::string(cause.what()) == "original statistics cause";
    }
    require(preserved);
    attachSessionStatistics(success, GenerationStatistics {});
    require(success.statistics && !success.statisticsException);
}

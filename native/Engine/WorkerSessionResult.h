// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_RESULT_H_
#define DUST3D_NATIVE_WORKER_SESSION_RESULT_H_
#include "WorkerFailure.h"
#include "WorkerGenerationStatistics.h"
#include "WorkerIndexedSources.h"
#include <exception>
#include <variant>
namespace dust3d {
namespace worker {
    using SessionSourceResult = std::variant<IndexedSourcesPayload, FailurePayload>;
    using SessionIndexedResult = std::variant<IndexedMeshPayload, FailurePayload>;
    struct NativeSessionGeneration {
        std::optional<IndexedSourcesPayload> mesh;
        std::optional<IndexedMeshPayload> ordinaryMesh;
        std::optional<FailurePayload> sourceIssue, failure, statisticsIssue;
        std::optional<GenerationStatistics> statistics;
        // Process-local original exception; transport must explicitly map it later.
        std::exception_ptr statisticsException;
        bool isEmpty = false;
    };
    /// Resolve owned outcomes without native calls. Only source statuses3/4 invoke
    /// the synchronous borrowed fallback exactly once; both original causes survive.
    /// Status5 means successful emptiness. Callback/allocation errors propagate.
    /// Example: auto value = resolveSessionGeneration(source, generateOrdinary);
    template <typename Fallback>
    NativeSessionGeneration resolveSessionGeneration(SessionSourceResult generated, Fallback&& fallback)
    {
        NativeSessionGeneration result;
        if (auto* mesh = std::get_if<IndexedSourcesPayload>(&generated))
            result.mesh = std::move(*mesh);
        else {
            auto original = std::get<FailurePayload>(std::move(generated));
            if (original.status == FailureStatus::noGeometry)
                result.isEmpty = true;
            else if (original.status == FailureStatus::generationFailed || original.status == FailureStatus::limitExceeded) {
                result.sourceIssue = std::move(original);
                auto ordinary = fallback();
                if (auto* mesh = std::get_if<IndexedMeshPayload>(&ordinary))
                    result.ordinaryMesh = std::move(*mesh);
                else {
                    auto cause = std::get<FailurePayload>(std::move(ordinary));
                    if (cause.status == FailureStatus::noGeometry)
                        result.isEmpty = true;
                    else
                        result.failure = std::move(cause);
                }
            } else
                result.failure = std::move(original);
        }
        return result;
    }
    /// Attach actual observations or their original failure, preserving geometry.
    /// A measured zero remains available; a failure never manufactures statistics.
    /// Example: attachSessionStatistics(result, actualStatistics);
    inline void attachSessionStatistics(NativeSessionGeneration& result, std::variant<GenerationStatistics, FailurePayload, std::exception_ptr> observation)
    {
        result.statistics.reset();
        result.statisticsIssue.reset();
        result.statisticsException = nullptr;
        if (auto* stats = std::get_if<GenerationStatistics>(&observation))
            result.statistics = std::move(*stats);
        else if (auto* issue = std::get_if<FailurePayload>(&observation))
            result.statisticsIssue = std::move(*issue);
        else
            result.statisticsException = std::get<std::exception_ptr>(std::move(observation));
    }
}
}
#endif

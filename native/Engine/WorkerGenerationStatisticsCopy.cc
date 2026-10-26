// SPDX-License-Identifier: MIT
#include "WorkerGenerationStatisticsCopy.h"
#include <algorithm>
#include <iterator>
#include <limits>
namespace dust3d {
namespace worker {
    GenerationStatistics copyNativeGenerationStatistics(const Dust3DGenerationStats& source, const GenerationStatisticsLimits& limits)
    {
        static_assert(sizeof(size_t) <= sizeof(uint64_t), "Native statistics counts must fit the portable representation.");
        GenerationStatistics value;
        value.maximumCachePayloadBytes = source.maximum_cache_payload_bytes;
        value.retainedPayloadBytes = source.retained_payload_bytes;
        value.reusedComponents = source.reused_components;
        value.rebuiltComponents = source.rebuilt_components;
        value.rebuiltParts = source.rebuilt_parts;
        value.cacheOutcome = static_cast<uint32_t>(source.cache_outcome);
        value.cacheStatus = static_cast<int32_t>(source.cache_status);
        // Reuse scalar/limit admission before retaining diagnostic bytes. This bounded
        // fixed prefix is separate validation workspace, never native cache accounting.
        (void)encodeGenerationStatistics(value, limits);
        const auto end = std::find(std::begin(source.cache_message), std::end(source.cache_message), '\0');
        if (end == std::end(source.cache_message))
            throw GenerationStatisticsError(GenerationStatisticsErrorCode::invalidStatistics, "Native cache diagnostic lacks bounded termination.");
        const auto count = static_cast<size_t>(end - std::begin(source.cache_message));
        if (count > limits.maximumCauseBytes || count > limits.maximumWireBytes - generationStatisticsPrefixBytes)
            throw GenerationStatisticsError(GenerationStatisticsErrorCode::limitExceeded, "Native cache diagnostic exceeds independent copy quota.");
        value.cacheCause.assign(source.cache_message, end);
        return value;
    }
}
}

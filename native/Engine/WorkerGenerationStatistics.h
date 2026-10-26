// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_GENERATION_STATISTICS_H_
#define DUST3D_NATIVE_WORKER_GENERATION_STATISTICS_H_
#include "WorkerProtocol.h"
#include <optional>
namespace dust3d {
namespace worker {
    constexpr size_t generationStatisticsPrefixBytes = 64;
    /// Actual session observations only. Absence is represented by optional, never
    /// manufactured zero counts. Cache cause is opaque and independent of generation.
    struct GenerationStatistics {
        uint64_t maximumCachePayloadBytes = 0, retainedPayloadBytes = 0;
        uint64_t reusedComponents = 0, rebuiltComponents = 0, rebuiltParts = 0;
        uint32_t cacheOutcome = 0;
        int32_t cacheStatus = 0;
        std::vector<uint8_t> cacheCause;
    };
    /// Bounded diagnostic wire/cause storage; excludes roots/capacity/workspace/RSS.
    struct GenerationStatisticsLimits {
        uint64_t maximumWireBytes = 4096, maximumCauseBytes = 4000;
    };
    enum class GenerationStatisticsErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        invalidStatistics,
        truncated,
        trailingData,
        limitExceeded };
    class GenerationStatisticsError : public std::runtime_error {
    public:
        GenerationStatisticsError(GenerationStatisticsErrorCode code, const char* cause);
        GenerationStatisticsErrorCode code() const noexcept { return m_code; }

    private:
        GenerationStatisticsErrorCode m_code;
    };
    /// Encode optional actual statistics to explicit-endian version1. Validates scalar
    /// cache bounds, availability and complete quotas before allocation. Numeric status
    /// and opaque cause survive unchanged. No native work or authenticity inference.
    /// Throws GenerationStatisticsError/allocation errors. Example: auto wire = encodeGenerationStatistics(stats, {});
    std::vector<uint8_t> encodeGenerationStatistics(const std::optional<GenerationStatistics>& value, const GenerationStatisticsLimits& limits);
    /// Inspect complete framing, scalar policies and quotas before copying cause bytes.
    /// Returns owned optional statistics; input remains borrowed through return.
    /// Throws original typed codec/allocation errors. Example: auto stats = decodeGenerationStatistics(bytes.data(), bytes.size(), {});
    std::optional<GenerationStatistics> decodeGenerationStatistics(const uint8_t* bytes, size_t size, const GenerationStatisticsLimits& limits);
}
}
#endif

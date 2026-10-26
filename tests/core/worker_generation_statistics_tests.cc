// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerGenerationStatistics.h>
#include <native/Engine/WorkerWire.h>
using namespace dust3d::worker;
namespace {
template <typename Action>
void reject(GenerationStatisticsErrorCode expected, Action action)
{
    try {
        action();
    } catch (const GenerationStatisticsError& cause) {
        if (cause.code() == expected && cause.what()[0])
            return;
        throw;
    }
    throw std::runtime_error("Invalid pure statistics value was accepted");
}
}
int main()
{
    try {
        GenerationStatistics value;
        value.maximumCachePayloadBytes = 1024;
        value.retainedPayloadBytes = 512;
        value.reusedComponents = 3;
        value.rebuiltComponents = 5;
        value.rebuiltParts = 7;
        value.cacheOutcome = 2;
        value.cacheStatus = -42;
        value.cacheCause = { 'a', 0, 255 };
        const auto wire = encodeGenerationStatistics(value, {});
        const auto copy = decodeGenerationStatistics(wire.data(), wire.size(), {});
        if (!copy || copy->reusedComponents != 3 || copy->cacheStatus != -42 || copy->cacheCause != value.cacheCause
            || encodeGenerationStatistics(copy, {}) != wire)
            throw std::runtime_error("Owned actual session statistics must retain counts, original status and opaque cause");
        const auto absent = encodeGenerationStatistics(std::nullopt, {});
        const auto zero = encodeGenerationStatistics(GenerationStatistics {}, {});
        if (absent == zero || decodeGenerationStatistics(absent.data(), absent.size(), {}))
            throw std::runtime_error("Unavailable metrics must remain distinct from actual measured zeros");
        const GenerationStatisticsLimits exact { wire.size(), value.cacheCause.size() };
        if (encodeGenerationStatistics(value, exact) != wire)
            throw std::runtime_error("Exact independent statistics wire/cause quotas accept");
        reject(GenerationStatisticsErrorCode::limitExceeded, [&] { encodeGenerationStatistics(value, { wire.size() - 1, 3 }); });
        reject(GenerationStatisticsErrorCode::limitExceeded, [&] { encodeGenerationStatistics(value, { wire.size(), 2 }); });
        for (const auto limits : { GenerationStatisticsLimits { 0, 1 }, { maximumDiagnosticBytes + 1, 1 }, { 64, 0 }, { 64, maximumDiagnosticBytes } })
            reject(GenerationStatisticsErrorCode::invalidLimits, [&] { encodeGenerationStatistics(value, limits); });
        reject(GenerationStatisticsErrorCode::limitExceeded, [&] { encodeGenerationStatistics(std::nullopt, { 63, 1 }); });
        auto invalid = value;
        invalid.retainedPayloadBytes = 1025;
        reject(GenerationStatisticsErrorCode::invalidStatistics, [&] { encodeGenerationStatistics(invalid, {}); });
        invalid = value;
        invalid.maximumCachePayloadBytes = maximumPayloadBytes + 1;
        reject(GenerationStatisticsErrorCode::invalidStatistics, [&] { encodeGenerationStatistics(invalid, {}); });
        invalid = value;
        invalid.cacheOutcome = 5;
        reject(GenerationStatisticsErrorCode::invalidStatistics, [&] { encodeGenerationStatistics(invalid, {}); });
        reject(GenerationStatisticsErrorCode::truncated, [&] { decodeGenerationStatistics(nullptr, 0, {}); });
        reject(GenerationStatisticsErrorCode::invalidInput, [&] { decodeGenerationStatistics(nullptr, 64, {}); });
        auto truncated = wire;
        truncated.pop_back();
        reject(GenerationStatisticsErrorCode::truncated, [&] { decodeGenerationStatistics(truncated.data(), truncated.size(), {}); });
        auto trailing = wire;
        trailing.push_back(0);
        reject(GenerationStatisticsErrorCode::trailingData, [&] { decodeGenerationStatistics(trailing.data(), trailing.size(), {}); });
        auto version = wire;
        version[1] = 2;
        reject(GenerationStatisticsErrorCode::unsupportedVersion, [&] { decodeGenerationStatistics(version.data(), version.size(), {}); });
        auto prefix = wire;
        prefix[3] = 63;
        reject(GenerationStatisticsErrorCode::invalidLayout, [&] { decodeGenerationStatistics(prefix.data(), prefix.size(), {}); });
        auto availability = wire;
        availability[7] = 2;
        reject(GenerationStatisticsErrorCode::invalidLayout, [&] { decodeGenerationStatistics(availability.data(), availability.size(), {}); });
        auto fabricated = absent;
        fabricated[8] = 1;
        reject(GenerationStatisticsErrorCode::invalidLayout, [&] { decodeGenerationStatistics(fabricated.data(), fabricated.size(), {}); });
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

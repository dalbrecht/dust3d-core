// SPDX-License-Identifier: MIT
#include "WorkerGenerationStatistics.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cstring>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(GenerationStatisticsErrorCode code, const char* cause) { throw GenerationStatisticsError(code, cause); }
        void validateLimits(const GenerationStatisticsLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumDiagnosticBytes
                || !limits.maximumCauseBytes || limits.maximumCauseBytes > maximumDiagnosticBytes - generationStatisticsPrefixBytes)
                reject(GenerationStatisticsErrorCode::invalidLimits, "Generation statistics policies exceed supported positive bounds.");
        }
        size_t plan(uint64_t cause, const GenerationStatisticsLimits& limits)
        {
            if (cause > limits.maximumCauseBytes || generationStatisticsPrefixBytes > limits.maximumWireBytes
                || cause > limits.maximumWireBytes - generationStatisticsPrefixBytes)
                reject(GenerationStatisticsErrorCode::limitExceeded, "Generation statistics exceed wire or cause quota.");
            return static_cast<size_t>(generationStatisticsPrefixBytes + cause);
        }
        void validate(const GenerationStatistics& value)
        {
            if (value.maximumCachePayloadBytes > maximumPayloadBytes || value.retainedPayloadBytes > value.maximumCachePayloadBytes || value.cacheOutcome > 4)
                reject(GenerationStatisticsErrorCode::invalidStatistics, "Generation cache observations exceed supported scalar bounds.");
        }
    }
    GenerationStatisticsError::GenerationStatisticsError(GenerationStatisticsErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    std::vector<uint8_t> encodeGenerationStatistics(const std::optional<GenerationStatistics>& value, const GenerationStatisticsLimits& limits)
    {
        validateLimits(limits);
        if (value)
            validate(*value);
        std::vector<uint8_t> bytes(plan(value ? value->cacheCause.size() : 0, limits), 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, generationStatisticsPrefixBytes, 2);
        if (!value)
            return bytes;
        detail::writeUnsigned(bytes.data() + 4, 1, 4);
        const std::array<uint64_t, 5> fields { value->maximumCachePayloadBytes, value->retainedPayloadBytes, value->reusedComponents, value->rebuiltComponents, value->rebuiltParts };
        for (size_t i = 0; i < fields.size(); ++i)
            detail::writeUnsigned(bytes.data() + 8 + i * 8, fields[i], 8);
        detail::writeUnsigned(bytes.data() + 48, value->cacheOutcome, 4);
        uint32_t status;
        std::memcpy(&status, &value->cacheStatus, sizeof(status));
        detail::writeUnsigned(bytes.data() + 52, status, 4);
        detail::writeUnsigned(bytes.data() + 56, value->cacheCause.size(), 8);
        std::copy(value->cacheCause.begin(), value->cacheCause.end(), bytes.begin() + generationStatisticsPrefixBytes);
        return bytes;
    }
    std::optional<GenerationStatistics> decodeGenerationStatistics(const uint8_t* bytes, size_t size, const GenerationStatisticsLimits& limits)
    {
        validateLimits(limits);
        if (size < generationStatisticsPrefixBytes)
            reject(GenerationStatisticsErrorCode::truncated, "Generation statistics prefix is incomplete.");
        if (!bytes)
            reject(GenerationStatisticsErrorCode::invalidInput, "Generation statistics storage is missing.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(GenerationStatisticsErrorCode::unsupportedVersion, "Generation statistics version is unsupported.");
        const auto present = detail::readUnsigned(bytes + 4, 4);
        if (detail::readUnsigned(bytes + 2, 2) != generationStatisticsPrefixBytes || present > 1)
            reject(GenerationStatisticsErrorCode::invalidLayout, "Generation statistics layout or availability is invalid.");
        const auto expected = plan(detail::readUnsigned(bytes + 56, 8), limits);
        if (size < expected)
            reject(GenerationStatisticsErrorCode::truncated, "Generation statistics cause is incomplete.");
        if (size > expected)
            reject(GenerationStatisticsErrorCode::trailingData, "Generation statistics contain trailing bytes.");
        if (!present) {
            if (std::any_of(bytes + 8, bytes + size, [](uint8_t value) { return value != 0; }))
                reject(GenerationStatisticsErrorCode::invalidLayout, "Unavailable statistics contain fabricated values.");
            return std::nullopt;
        }
        GenerationStatistics value;
        value.maximumCachePayloadBytes = detail::readUnsigned(bytes + 8, 8);
        value.retainedPayloadBytes = detail::readUnsigned(bytes + 16, 8);
        value.reusedComponents = detail::readUnsigned(bytes + 24, 8);
        value.rebuiltComponents = detail::readUnsigned(bytes + 32, 8);
        value.rebuiltParts = detail::readUnsigned(bytes + 40, 8);
        value.cacheOutcome = static_cast<uint32_t>(detail::readUnsigned(bytes + 48, 4));
        const auto status = static_cast<uint32_t>(detail::readUnsigned(bytes + 52, 4));
        std::memcpy(&value.cacheStatus, &status, sizeof(status));
        validate(value);
        value.cacheCause.assign(bytes + generationStatisticsPrefixBytes, bytes + size);
        return value;
    }
}
}

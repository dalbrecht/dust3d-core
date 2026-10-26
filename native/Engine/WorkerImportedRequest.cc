// SPDX-License-Identifier: MIT
#include "WorkerImportedRequest.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Imported request spans must fit size_t.");
        [[noreturn]] void reject(ImportedRequestErrorCode code, const char* cause) { throw ImportedRequestError(code, cause); }
        std::array<uint64_t, 8> fields(const ImportedRequestOptions& value)
        {
            return { value.maximumMaterialNativeBytes, value.maximumSurfaceNativeBytes, value.maximumFactsCopyBytes, value.maximumSurfaceCopyBytes,
                value.maximumEncodedImageBytes, value.maximumDecodedPixelBytes, value.maximumAggregateDecodedBytes, value.maximumOutputWireBytes };
        }
        void validateLimits(const ImportedRequestLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
                reject(ImportedRequestErrorCode::invalidLimits, "Imported request quotas are outside supported positive bounds.");
        }
        size_t plan(uint64_t archive, const ImportedRequestOptions& options, const ImportedRequestLimits& limits)
        {
            validateImportedRequestOptions(options);
            if (!archive)
                reject(ImportedRequestErrorCode::invalidInput, "Imported request archive is empty.");
            if (importedRequestPrefixBytes > limits.maximumWireBytes || archive > limits.maximumWireBytes - importedRequestPrefixBytes || archive > limits.maximumDecodedBytes)
                reject(ImportedRequestErrorCode::limitExceeded, "Imported request exceeds its wire or retained archive quota.");
            return static_cast<size_t>(importedRequestPrefixBytes + archive);
        }
        uint64_t tighten(uint64_t requested, uint64_t standard, uint64_t receiver) { return std::min(receiver, requested ? requested : standard); }
    }
    ImportedRequestError::ImportedRequestError(ImportedRequestErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateImportedRequestOptions(const ImportedRequestOptions& options)
    {
        for (uint64_t value : fields(options))
            if (value > maximumPayloadBytes)
                reject(ImportedRequestErrorCode::invalidOptions, "Imported request budget exceeds its supported range.");
    }
    NativeImportedProducerLimits applyImportedRequestOptions(const ImportedRequestOptions& options, const NativeImportedProducerLimits& receiver)
    {
        validateNativeImportedProducerLimits(receiver);
        validateImportedRequestOptions(options);
        const NativeImportedProducerLimits defaults;
        auto result = receiver;
        result.maximumMaterialNativeBytes = tighten(options.maximumMaterialNativeBytes, defaults.maximumMaterialNativeBytes, receiver.maximumMaterialNativeBytes);
        result.maximumSurfaceNativeBytes = tighten(options.maximumSurfaceNativeBytes, defaults.maximumSurfaceNativeBytes, receiver.maximumSurfaceNativeBytes);
        auto& preparation = result.preparation;
        auto& render = preparation.render;
        render.maximumDecodedBytes = tighten(options.maximumAggregateDecodedBytes, defaults.preparation.render.maximumDecodedBytes, render.maximumDecodedBytes);
        render.maximumWireBytes = tighten(options.maximumOutputWireBytes, defaults.preparation.render.maximumWireBytes, render.maximumWireBytes);
        preparation.facts.maximumDecodedBytes = std::min(render.maximumDecodedBytes, tighten(options.maximumFactsCopyBytes, defaults.preparation.facts.maximumDecodedBytes, preparation.facts.maximumDecodedBytes));
        render.surface.maximumDecodedBytes = std::min(render.maximumDecodedBytes, tighten(options.maximumSurfaceCopyBytes, defaults.preparation.render.surface.maximumDecodedBytes, render.surface.maximumDecodedBytes));
        preparation.maximumEncodedImageBytes = tighten(options.maximumEncodedImageBytes, defaults.preparation.maximumEncodedImageBytes, preparation.maximumEncodedImageBytes);
        render.maximumDecodedPixelBytes = tighten(options.maximumDecodedPixelBytes, defaults.preparation.render.maximumDecodedPixelBytes, render.maximumDecodedPixelBytes);
        render.surface.maximumWireBytes = std::min(render.surface.maximumWireBytes, render.maximumWireBytes);
        render.surface.geometry.maximumWireBytes = std::min(render.surface.geometry.maximumWireBytes, render.maximumWireBytes);
        render.surface.geometry.maximumDecodedBytes = std::min(render.surface.geometry.maximumDecodedBytes, render.surface.maximumDecodedBytes);
        render.surface.surface.maximumWireBytes = std::min(render.surface.surface.maximumWireBytes, render.maximumWireBytes);
        render.surface.surface.maximumDecodedBytes = std::min(render.surface.surface.maximumDecodedBytes, render.surface.maximumDecodedBytes);
        render.materials.maximumWireBytes = std::min(render.materials.maximumWireBytes, render.maximumWireBytes);
        render.materials.maximumDecodedBytes = std::min(render.materials.maximumDecodedBytes, preparation.facts.maximumDecodedBytes);
        validateNativeImportedProducerLimits(result);
        return result;
    }
    std::vector<uint8_t> encodeImportedRequestView(const ImportedRequestView& value, const ImportedRequestLimits& limits)
    {
        validateLimits(limits);
        if (!value.archive)
            reject(ImportedRequestErrorCode::invalidInput, "Imported request archive span is missing.");
        const auto size = plan(value.archiveBytes, value.options, limits);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, importedRequestPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, value.archiveBytes, 8);
        const auto budgets = fields(value.options);
        for (size_t index = 0; index < budgets.size(); ++index)
            detail::writeUnsigned(bytes.data() + 16 + index * 8, budgets[index], 8);
        std::copy(value.archive, value.archive + value.archiveBytes, bytes.begin() + importedRequestPrefixBytes);
        return bytes;
    }
    std::vector<uint8_t> encodeImportedRequest(const ImportedRequest& value, const ImportedRequestLimits& limits)
    {
        return encodeImportedRequestView({ value.archive.data(), value.archive.size(), value.options }, limits);
    }
    ImportedRequest decodeImportedRequest(const uint8_t* bytes, size_t size, const ImportedRequestLimits& limits)
    {
        validateLimits(limits);
        if (!bytes)
            reject(ImportedRequestErrorCode::invalidInput, "Imported request wire span is missing.");
        if (size < importedRequestPrefixBytes)
            reject(ImportedRequestErrorCode::truncated, "Imported request prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(ImportedRequestErrorCode::unsupportedVersion, "Imported request version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != importedRequestPrefixBytes || detail::readUnsigned(bytes + 4, 4))
            reject(ImportedRequestErrorCode::invalidLayout, "Imported request prefix/reserved fields are invalid.");
        std::array<uint64_t, 8> budgets;
        for (size_t index = 0; index < budgets.size(); ++index)
            budgets[index] = detail::readUnsigned(bytes + 16 + index * 8, 8);
        ImportedRequestOptions options { budgets[0], budgets[1], budgets[2], budgets[3], budgets[4], budgets[5], budgets[6], budgets[7] };
        const auto expected = plan(detail::readUnsigned(bytes + 8, 8), options, limits);
        if (size < expected)
            reject(ImportedRequestErrorCode::truncated, "Imported request archive is incomplete.");
        if (size > expected)
            reject(ImportedRequestErrorCode::trailingData, "Imported request contains trailing bytes.");
        return { { bytes + importedRequestPrefixBytes, bytes + size }, options };
    }
}
}

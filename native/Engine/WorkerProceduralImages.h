// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PROCEDURAL_IMAGES_H_
#define DUST3D_NATIVE_WORKER_PROCEDURAL_IMAGES_H_
#include "WorkerProtocol.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace dust3d {
namespace worker {
    /// Borrowed validated archive metadata/payload; all spans stay stable through copying.
    struct ProceduralImageAssetView {
        std::string_view type, name;
        const uint8_t* bytes;
        size_t size;
    };
    struct ProceduralImageInput {
        std::string identity, assetName;
        std::vector<uint8_t> encoded;
    };
    /// Bounded metadata workspace and retained selected values; no pixel/native/RSS quota.
    struct ProceduralImageLimits {
        uint64_t maximumReferences = 10000, maximumAssets = 100000;
        uint64_t maximumAssetNameBytes = 1024 * 1024;
        uint64_t maximumEncodedImageBytes = 32ULL * 1024 * 1024;
        uint64_t maximumEncodedTotalBytes = 128ULL * 1024 * 1024;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
    };
    enum class ProceduralImageErrorCode { invalidInput,
        invalidLimits,
        invalidIdentity,
        missingAsset,
        ambiguousAsset,
        invalidAssetKind,
        limitExceeded };
    class ProceduralImageError : public std::runtime_error {
    public:
        ProceduralImageError(ProceduralImageErrorCode code, const std::string& cause);
        ProceduralImageErrorCode code() const noexcept { return m_code; }

    private:
        ProceduralImageErrorCode m_code;
    };
    /// Validate positive bounded policies before archive/native work; throws ProceduralImageError.
    /// Example: validateProceduralImageLimits({});
    void validateProceduralImageLimits(const ProceduralImageLimits& limits);
    /// Select referenced image assets with first-reference brace-alias deduplication.
    /// @param references Exact component colorImageId values; empty values are absent.
    /// @param assets Stable metadata and actual readable selected payload spans.
    /// @param limits Positive supported independent quotas. Logical retained charge is
    /// records plus identity/key/encoded bytes, excluding roots/capacity/workspace/RSS.
    /// @return Independent encoded inputs in reference order, before platform decoding.
    /// @throws ProceduralImageError or allocation errors; every selected span and total
    /// validates before any encoded payload copy. No archive parsing or native code.
    /// Example: auto inputs = copyProceduralImageInputs(references, assets, {});
    std::vector<ProceduralImageInput> copyProceduralImageInputs(const std::vector<std::string>& references,
        const std::vector<ProceduralImageAssetView>& assets, const ProceduralImageLimits& limits);
}
}
#endif

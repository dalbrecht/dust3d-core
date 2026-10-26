// SPDX-License-Identifier: MIT
#include "WorkerProceduralImages.h"
#include "WorkerUtf8.h"
#include <optional>
#include <set>
#include <unordered_map>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(ProceduralImageErrorCode code, const std::string& cause) { throw ProceduralImageError(code, cause); }
        std::optional<std::string_view> identity(std::string_view value)
        {
            if (value.size() == 38 && value.front() == '{' && value.back() == '}')
                value = value.substr(1, 36);
            if (value.size() != 36 || !detail::isIdentityUtf8(value))
                return std::nullopt;
            for (size_t index : { size_t(8), size_t(13), size_t(18), size_t(23) })
                if (value[index] != '-')
                    return std::nullopt;
            return value;
        }
        std::optional<std::string_view> assetIdentity(std::string_view name)
        {
            if (name.substr(0, 7) != "images/")
                return std::nullopt;
            const auto tail = name.substr(7);
            return identity(tail.substr(0, tail.find_first_of("/.")));
        }
        void charge(uint64_t& total, uint64_t bytes, uint64_t maximum)
        {
            // Totals start at zero and advance only after this checked addition.
            if (bytes > maximum - total)
                reject(ProceduralImageErrorCode::limitExceeded, "Procedural image inputs exceed their independent quota.");
            total += bytes;
        }
        struct Candidate {
            const ProceduralImageAssetView* asset = nullptr;
            uint64_t count = 0;
        };
    }
    ProceduralImageError::ProceduralImageError(ProceduralImageErrorCode code, const std::string& cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateProceduralImageLimits(const ProceduralImageLimits& limits)
    {
        if (!limits.maximumReferences || limits.maximumReferences > 100000 || !limits.maximumAssets || limits.maximumAssets > 100000
            || !limits.maximumAssetNameBytes || limits.maximumAssetNameBytes > maximumPayloadBytes
            || !limits.maximumEncodedImageBytes || limits.maximumEncodedImageBytes > maximumPayloadBytes
            || !limits.maximumEncodedTotalBytes || limits.maximumEncodedTotalBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
            reject(ProceduralImageErrorCode::invalidLimits, "Procedural image policies are outside supported positive bounds.");
    }
    std::vector<ProceduralImageInput> copyProceduralImageInputs(const std::vector<std::string>& references,
        const std::vector<ProceduralImageAssetView>& assets, const ProceduralImageLimits& limits)
    {
        validateProceduralImageLimits(limits);
        if (references.size() > limits.maximumReferences || assets.size() > limits.maximumAssets)
            reject(ProceduralImageErrorCode::limitExceeded, "Procedural image metadata count exceeds policy.");
        uint64_t names = 0;
        for (const auto& asset : assets)
            charge(names, asset.name.size(), limits.maximumAssetNameBytes);
        std::unordered_map<std::string_view, Candidate> indexed;
        for (const auto& asset : assets) {
            const auto key = assetIdentity(asset.name);
            if (!key)
                continue;
            auto& entry = indexed[*key];
            if (asset.type == "asset") {
                entry.asset = &asset;
                ++entry.count;
            }
        }
        std::set<std::string_view> seen;
        std::vector<const ProceduralImageAssetView*> selected;
        uint64_t encoded = 0, logical = 0;
        for (const auto& reference : references) {
            if (reference.empty())
                continue;
            const auto key = identity(reference);
            if (!key)
                reject(ProceduralImageErrorCode::invalidIdentity, "Procedural image identity has an invalid length, delimiter or non-NUL UTF8.");
            if (!seen.insert(*key).second)
                continue;
            const auto found = indexed.find(*key);
            const auto canonical = "{" + std::string(*key) + "}";
            if (found == indexed.end())
                reject(ProceduralImageErrorCode::missingAsset, "Missing procedural image asset: images/" + canonical + ".png");
            const auto& candidate = found->second;
            if (candidate.count > 1)
                reject(ProceduralImageErrorCode::ambiguousAsset, "Multiple procedural assets match image: " + canonical);
            if (!candidate.asset)
                reject(ProceduralImageErrorCode::invalidAssetKind, "Referenced procedural image has the wrong archive kind: " + canonical);
            const auto& asset = *candidate.asset;
            if (!detail::isIdentityUtf8(asset.name))
                reject(ProceduralImageErrorCode::invalidInput, "Selected procedural image archive key is not non-NUL UTF8.");
            if (asset.size && !asset.bytes)
                reject(ProceduralImageErrorCode::invalidInput, "Selected procedural image payload is missing.");
            if (asset.size > limits.maximumEncodedImageBytes)
                reject(ProceduralImageErrorCode::limitExceeded, "Selected procedural image exceeds its encoded quota.");
            charge(encoded, asset.size, limits.maximumEncodedTotalBytes);
            charge(logical, sizeof(ProceduralImageInput), limits.maximumDecodedBytes);
            charge(logical, canonical.size(), limits.maximumDecodedBytes);
            charge(logical, asset.name.size(), limits.maximumDecodedBytes);
            charge(logical, asset.size, limits.maximumDecodedBytes);
            selected.push_back(&asset);
        }
        std::vector<ProceduralImageInput> result;
        result.reserve(selected.size());
        for (const auto* asset : selected) {
            ProceduralImageInput value { "{" + std::string(*assetIdentity(asset->name)) + "}", std::string(asset->name), {} };
            if (asset->size)
                value.encoded.assign(asset->bytes, asset->bytes + asset->size);
            result.push_back(std::move(value));
        }
        return result;
    }
}
}

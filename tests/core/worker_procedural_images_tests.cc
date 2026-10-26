// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerProceduralImages.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(ProceduralImageErrorCode expected, Action action)
{
    try {
        action();
    } catch (const ProceduralImageError& cause) {
        require(cause.code() == expected && cause.what()[0], "Procedural image rejection retains its original typed cause");
        return;
    }
    throw std::runtime_error("Invalid procedural image input accepted");
}
}
int main()
{
    try {
        const std::string key = "QQQQQQQQ-QQQQ-QQQQ-QQQQ-QQQQQQQQQQQQ";
        const std::string assetName = "images/{" + key + "}.png";
        const uint8_t encoded[] = { 0, 255, 42 };
        const std::vector<ProceduralImageAssetView> assets { { "asset", assetName, encoded, sizeof(encoded) } };
        const auto images = copyProceduralImageInputs({ key, "{" + key + "}", "" }, assets, {});
        require(images.size() == 1 && images[0].identity == "{" + key + "}" && images[0].assetName == assetName
                && images[0].encoded == std::vector<uint8_t>({ 0, 255, 42 }),
            "First-reference UUID aliases deduplicate while exact encoded bytes and archive key survive");
        require(copyProceduralImageInputs({}, assets, {}).empty(), "No references returns successful empty input");
        const auto copied = images;
        require(copied[0].encoded.data() != encoded, "Encoded output owns independent storage");
        auto exact = ProceduralImageLimits {};
        exact.maximumEncodedImageBytes = 3;
        exact.maximumEncodedTotalBytes = 3;
        exact.maximumAssetNameBytes = assetName.size();
        exact.maximumDecodedBytes = sizeof(ProceduralImageInput) + 38 + assetName.size() + 3;
        require(copyProceduralImageInputs({ key }, assets, exact)[0].encoded == copied[0].encoded, "Every exact quota accepts");
        for (int field = 0; field < 4; ++field) {
            auto shortQuota = exact;
            if (field == 0)
                --shortQuota.maximumEncodedImageBytes;
            if (field == 1)
                --shortQuota.maximumEncodedTotalBytes;
            if (field == 2)
                --shortQuota.maximumAssetNameBytes;
            if (field == 3)
                --shortQuota.maximumDecodedBytes;
            reject(ProceduralImageErrorCode::limitExceeded, [&] { copyProceduralImageInputs({ key }, assets, shortQuota); });
        }
        for (const auto& invalid : { std::string("wrong"), "[" + key + "]", "{" + key + "]", std::string(36, 'x'), std::string(36, '\0') })
            reject(ProceduralImageErrorCode::invalidIdentity, [&] { copyProceduralImageInputs({ invalid }, assets, {}); });
        reject(ProceduralImageErrorCode::missingAsset, [&] { copyProceduralImageInputs({ key }, {}, {}); });
        auto wrongKind = assets;
        wrongKind[0].type = "metadata";
        reject(ProceduralImageErrorCode::invalidAssetKind, [&] { copyProceduralImageInputs({ key }, wrongKind, {}); });
        auto ambiguous = assets;
        const std::string alias = "images/" + key + ".anything/nested";
        ambiguous.push_back({ "asset", alias, encoded, sizeof(encoded) });
        reject(ProceduralImageErrorCode::ambiguousAsset, [&] { copyProceduralImageInputs({ key }, ambiguous, {}); });
        auto missingSpan = assets;
        missingSpan[0].bytes = nullptr;
        reject(ProceduralImageErrorCode::invalidInput, [&] { copyProceduralImageInputs({ key }, missingSpan, {}); });
        missingSpan[0].size = 0;
        require(copyProceduralImageInputs({ key }, missingSpan, {})[0].encoded.empty(), "Empty encoded asset is retained for an explicit later decoder error");
        const std::string withNul = assetName + std::string(1, '\0');
        auto invalidKey = assets;
        invalidKey[0].name = withNul;
        reject(ProceduralImageErrorCode::invalidInput, [&] { copyProceduralImageInputs({ key }, invalidKey, {}); });
        auto invalidUtf8 = key;
        invalidUtf8[0] = static_cast<char>(255);
        const std::string invalidUtf8Asset = "images/" + invalidUtf8 + ".png";
        const std::vector<ProceduralImageAssetView> invalidUtf8Assets { { "asset", invalidUtf8Asset, encoded, sizeof(encoded) } };
        reject(ProceduralImageErrorCode::invalidIdentity, [&] { copyProceduralImageInputs({ invalidUtf8 }, invalidUtf8Assets, {}); });
        const std::string invalidSuffix = assetName + std::string(1, static_cast<char>(255));
        auto invalidSuffixAssets = assets;
        invalidSuffixAssets[0].name = invalidSuffix;
        reject(ProceduralImageErrorCode::invalidInput, [&] { copyProceduralImageInputs({ key }, invalidSuffixAssets, {}); });
        for (int field = 0; field < 6; ++field) {
            for (uint64_t invalid : { uint64_t(0), maximumPayloadBytes + 1 }) {
                auto policy = ProceduralImageLimits {};
                uint64_t* members[] = { &policy.maximumReferences, &policy.maximumAssets, &policy.maximumAssetNameBytes,
                    &policy.maximumEncodedImageBytes, &policy.maximumEncodedTotalBytes, &policy.maximumDecodedBytes };
                *members[field] = invalid;
                reject(ProceduralImageErrorCode::invalidLimits, [&] { validateProceduralImageLimits(policy); });
            }
        }
        auto counts = ProceduralImageLimits {};
        counts.maximumReferences = 1;
        reject(ProceduralImageErrorCode::limitExceeded, [&] { copyProceduralImageInputs({ key, key }, assets, counts); });
        counts = {};
        counts.maximumAssets = 1;
        reject(ProceduralImageErrorCode::limitExceeded, [&] { copyProceduralImageInputs({ key }, ambiguous, counts); });
        const std::string secondKey = "00000000-0000-0000-0000-000000000000";
        const std::string secondName = "images/" + secondKey + ".png";
        auto multiple = assets;
        multiple.push_back({ "asset", secondName, encoded, sizeof(encoded) });
        const auto ordered = copyProceduralImageInputs({ secondKey, key, secondKey }, multiple, {});
        require(ordered.size() == 2 && ordered[0].identity == "{" + secondKey + "}" && ordered[1].identity == "{" + key + "}",
            "Opaque zero UUID and case-sensitive first-reference order survive");
        counts = {};
        counts.maximumEncodedTotalBytes = 5;
        reject(ProceduralImageErrorCode::limitExceeded, [&] { copyProceduralImageInputs({ secondKey, key }, multiple, counts); });
        auto metadataAndAsset = wrongKind;
        metadataAndAsset.push_back(assets[0]);
        require(copyProceduralImageInputs({ key }, metadataAndAsset, {}).size() == 1, "One matching asset wins over unrelated matching metadata kind");
        const std::string badImageKey = "images/not-an-identity.png";
        multiple.push_back({ "asset", badImageKey, nullptr, 1 });
        require(copyProceduralImageInputs({ key }, multiple, {}).size() == 1, "Malformed unreferenced image key is ignored without reading payload");
        const std::string unrelated = "unrelated/" + key;
        auto ignored = assets;
        ignored.push_back({ "asset", unrelated, nullptr, 3 });
        require(copyProceduralImageInputs({ key }, ignored, {}).size() == 1, "Unreferenced unrelated spans are never read");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

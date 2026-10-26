// SPDX-License-Identifier: MIT
#include "WorkerProceduralImagesNative.h"
#include "EngineInternal.h"
#include <exception>
#include <set>
namespace dust3d {
namespace worker {
    namespace {
        struct Asset {
            std::string type, name;
            const uint8_t* bytes;
            size_t size;
        };
        class Observer final : public native::DocumentAssetObserver {
        public:
            explicit Observer(const ProceduralImageLimits& policy)
                : limits(policy)
            {
            }
            void item(std::string_view type, std::string_view name, const uint8_t* data, size_t size) noexcept override
            {
                if (failure)
                    return;
                try {
                    if (assets.size() >= limits.maximumAssets || name.size() > limits.maximumAssetNameBytes - names)
                        throw ProceduralImageError(ProceduralImageErrorCode::limitExceeded, "Archive image metadata exceeds its configured quota.");
                    names += name.size();
                    assets.push_back({ std::string(type), std::string(name), data, size });
                } catch (...) {
                    failure = std::current_exception();
                }
            }
            void attribute(std::string_view, std::string_view) noexcept override { }
            const ProceduralImageLimits& limits;
            uint64_t names = 0;
            std::vector<Asset> assets;
            std::exception_ptr failure;
        };
        void appendChildren(const std::map<std::string, std::string>& attributes, std::vector<std::string>& pending)
        {
            const auto children = String::split(String::valueOrEmpty(attributes, "children"), ',');
            pending.insert(pending.end(), children.rbegin(), children.rend());
        }
    }
    std::vector<ProceduralImageInput> loadProceduralImageInputs(const std::vector<uint8_t>& archive, const ProceduralImageLimits& limits)
    {
        validateProceduralImageLimits(limits);
        Observer observer(limits);
        std::unique_ptr<Snapshot> snapshot;
        std::string warning;
        {
            std::lock_guard<std::mutex> lock(native::engineMutex());
            snapshot = native::readDocumentMetadata(archive.data(), archive.size(), warning, &observer);
        }
        if (observer.failure)
            std::rethrow_exception(observer.failure);
        std::vector<std::string> pending, references;
        std::set<std::string> seen;
        appendChildren(snapshot->rootComponent, pending);
        while (!pending.empty()) {
            auto key = std::move(pending.back());
            pending.pop_back();
            if (key.empty())
                continue;
            if (!seen.insert(key).second || seen.size() > limits.maximumReferences)
                throw ProceduralImageError(ProceduralImageErrorCode::limitExceeded, "Procedural image component traversal exceeds its unique-reference policy.");
            const auto found = snapshot->components.find(key);
            if (found == snapshot->components.end())
                throw ProceduralImageError(ProceduralImageErrorCode::invalidInput, "Procedural image component has no exact hierarchy owner.");
            references.push_back(String::valueOrEmpty(found->second, "colorImageId"));
            appendChildren(found->second, pending);
        }
        std::vector<ProceduralImageAssetView> assets;
        assets.reserve(observer.assets.size());
        for (const auto& asset : observer.assets)
            assets.push_back({ asset.type, asset.name, asset.bytes, asset.size });
        return copyProceduralImageInputs(references, assets, limits);
    }
}
}

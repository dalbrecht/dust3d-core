// SPDX-License-Identifier: MIT
#include "ImportedModels.h"
#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>
#include <set>
#include <string_view>

namespace dust3d {
namespace native {
    namespace {
        std::string modelIdentity(std::string_view source)
        {
            if (source.size() == 38 && source.front() == '{' && source.back() == '}')
                source = source.substr(1, 36);
            if (source.size() != 36)
                return {};
            for (size_t index = 0; index < source.size(); ++index) {
                const char value = source[index];
                if (index == 8 || index == 13 || index == 18 || index == 23) {
                    if (value != '-')
                        return {};
                } else if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F'))) {
                    return {};
                }
            }
            return "{" + std::string(source) + "}";
        }
        std::string assetIdentity(const std::string& name)
        {
            if (name.size() < 11 || name.compare(0, 7, "models/") || name.compare(name.size() - 4, 4, ".glb"))
                return {};
            return modelIdentity(std::string_view(name).substr(7, name.size() - 11));
        }
        void accountAsset(RetainedPayloadBudget& budget, const GlbAsset& asset) noexcept
        {
            const auto& model = asset.model;
            budget.accountDynamicFields(model.vertices, model.vertexNormals, model.faces, model.triangleUvs, model.vertexColors, model.triangleCornerUvs);
            if (asset.image)
                budget.accountDynamicFields(asset.image->mimeType, asset.image->bytes);
        }
    }
    void ImportedModels::load(Snapshot& snapshot, const std::vector<ModelAssetView>& assets, std::string& warning)
    {
        std::map<std::string, const ModelAssetView*> bindings;
        std::set<std::string> ambiguous;
        for (const auto& asset : assets) {
            const auto identity = assetIdentity(asset.name);
            if (!identity.empty() && !bindings.emplace(identity, &asset).second)
                ambiguous.insert(identity);
        }
        // Construct privately so failed primary decoding cannot publish partial imports.
        std::map<std::string, GlbAsset> decoded;
        RetainedPayloadBudget budget(maximumPayloadBytes);
        budget.tryAdd(1, sizeof(decoded));
        for (auto& part : snapshot.parts) {
            const auto target = String::valueOrEmpty(part.second, "target");
            if (target != "ImportedModel") {
                if (!String::valueOrEmpty(part.second, "importedModelId").empty()
                    && !target.empty() && target != "Model" && target != "CutFace" && target != "StitchingLine" && target != "StitchingLoop")
                    throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported part target for imported model reference.");
                continue;
            }
            const auto identity = modelIdentity(String::valueOrEmpty(part.second, "importedModelId"));
            if (identity.empty())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An imported model part requires a valid importedModelId UUID.");
            if (ambiguous.count(identity))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "An imported model UUID has ambiguous archive asset aliases: " + identity);
            const auto binding = bindings.find(identity);
            if (binding == bindings.end())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Missing required embedded model asset: " + identity);
            if (!decoded.count(identity)) {
                GlbAsset asset;
                try {
                    asset = readGlb(binding->second->bytes, binding->second->size);
                } catch (const DocumentError& cause) {
                    throw DocumentError(cause.status(), "Embedded model " + identity + ": " + cause.what());
                }
                const auto inserted = decoded.emplace(identity, std::move(asset)).first;
                budget.tryAdd(1, sizeof(decltype(decoded)::value_type));
                budget.accountDynamic(inserted->first);
                accountAsset(budget, inserted->second);
                if (budget.isExceeded())
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Decoded imported model payload exceeds the 64 MiB aggregate limit.");
            }
            part.second["importedModelId"] = identity;
        }
        m_assets = std::move(decoded);
        if (!m_assets.empty()) {
            if (!warning.empty())
                warning += " ";
            warning += "Imported geometry preview: GLB materials, textures and transparency are not yet applied.";
        }
    }
    bool ImportedModels::hasVertexColors(const std::string& identity) const
    {
        const auto found = m_assets.find(identity);
        if (found == m_assets.end())
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Unknown model while resolving source color encoding.");
        return !found->second.model.vertexColors.empty();
    }
    std::map<std::string, GlbAsset> ImportedModels::takeAssets()
    {
        return std::move(m_assets);
    }
    std::map<std::string, MeshGenerator::ImportedModelData> ImportedModels::takeGeometry()
    {
        std::map<std::string, MeshGenerator::ImportedModelData> geometry;
        for (auto& [identity, asset] : m_assets)
            geometry.emplace(identity, std::move(asset.model));
        return geometry;
    }
}
}

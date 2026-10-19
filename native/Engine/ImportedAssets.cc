// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include <algorithm>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/string.h>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {
using dust3d::RetainedPayloadBudget;
using dust3d::native::DocumentError;
using dust3d::native::GlbAsset;

struct Material {
    std::string identity;
    GlbAsset asset;
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        const auto& [model, baseColor, metallic, roughness, emissive, alphaMode, alphaCutoff, isDoubleSided, sampler, image] = asset;
        const auto& [vertices, normals, faces, uvs, colors, cornerUvs] = model;
        budget.accountDynamicFields(identity, vertices, normals, faces, uvs, colors, cornerUvs);
        if (image)
            budget.accountDynamicFields(image->mimeType, image->bytes);
    }
};
struct Binding {
    std::string component;
    std::string part;
    size_t modelIndex;
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        budget.accountDynamicFields(component, part);
    }
};
void requirePayload(const RetainedPayloadBudget& budget)
{
    if (budget.isExceeded())
        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Imported material payload exceeds the configured output limit.");
}
template <typename Value>
void reserve(std::vector<Value>& values, size_t count, RetainedPayloadBudget& budget)
{
    budget.tryAdd(count, sizeof(Value));
    requirePayload(budget);
    values.reserve(count);
    budget.tryAdd(values.capacity() - count, sizeof(Value));
    requirePayload(budget);
}
size_t payloadLimit(const Dust3DImportedAssetOptions* options)
{
    if (options && options->struct_size < sizeof(Dust3DImportedAssetOptions))
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Imported asset options are too short.");
    const size_t maximum = options && options->maximum_payload_bytes ? options->maximum_payload_bytes : DUST3D_DEFAULT_IMPORTED_ASSET_PAYLOAD_BYTES;
    if (maximum > DUST3D_MAX_IMPORTED_ASSET_PAYLOAD_BYTES)
        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Imported asset payload quota exceeds its hard maximum.");
    return maximum;
}
}

struct Dust3DImportedAssets {
    std::vector<Material> materials;
    std::vector<Binding> bindings;
    size_t payloadBytes = 0;
};

namespace {
std::unique_ptr<Dust3DImportedAssets> build(const dust3d::Snapshot& snapshot,
    std::map<std::string, GlbAsset> assets, size_t maximumBytes)
{
    auto owner = std::make_unique<Dust3DImportedAssets>();
    RetainedPayloadBudget budget(maximumBytes);
    budget.tryAdd(1, sizeof(Dust3DImportedAssets));
    requirePayload(budget);
    reserve(owner->materials, assets.size(), budget);
    std::map<std::string, size_t> modelIndices;
    for (auto& [identity, asset] : assets) {
        // Inspection retains only material/image state, not unused mesh allocations.
        asset.model = {};
        Material material { identity, std::move(asset) };
        budget.accountDynamic(material);
        requirePayload(budget);
        modelIndices.emplace(identity, owner->materials.size());
        owner->materials.push_back(std::move(material));
    }
    std::map<std::string, std::vector<const std::string*>> components;
    for (const auto& [identity, component] : snapshot.components) {
        if (dust3d::String::valueOrEmpty(component, "linkDataType") != "partId")
            continue;
        const auto partId = dust3d::String::valueOrEmpty(component, "linkData");
        const auto part = snapshot.parts.find(partId);
        if (part != snapshot.parts.end() && dust3d::String::valueOrEmpty(part->second, "target") == "ImportedModel")
            components[partId].push_back(&identity);
    }
    size_t bindingCount = 0;
    for (const auto& [identity, part] : snapshot.parts) {
        if (dust3d::String::valueOrEmpty(part, "target") != "ImportedModel")
            continue;
        const size_t count = std::max(size_t(1), components[identity].size());
        // Charge before adding so an impossible count cannot overflow the sum.
        budget.tryAdd(count, sizeof(Binding));
        requirePayload(budget);
        bindingCount += count;
    }
    owner->bindings.reserve(bindingCount);
    budget.tryAdd(owner->bindings.capacity() - bindingCount, sizeof(Binding));
    requirePayload(budget);
    auto appendBinding = [&](const std::string& component, const std::string& part, size_t model) {
        Binding binding { component, part, model };
        budget.accountDynamic(binding);
        requirePayload(budget);
        owner->bindings.push_back(std::move(binding));
    };
    for (const auto& [identity, part] : snapshot.parts) {
        if (dust3d::String::valueOrEmpty(part, "target") != "ImportedModel")
            continue;
        const auto model = modelIndices.at(dust3d::String::valueOrEmpty(part, "importedModelId"));
        const auto& instances = components.at(identity);
        if (instances.empty())
            appendBinding("", identity, model);
        else
            for (const auto* component : instances)
                appendBinding(*component, identity, model);
    }
    owner->payloadBytes = budget.usedBytes();
    return owner;
}
}

Dust3DImportedAssets* dust3d_imported_assets_load(const uint8_t* bytes, size_t size,
    const Dust3DImportedAssetOptions* options, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    try {
        const auto maximum = payloadLimit(options);
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        dust3d::native::ImportedModels imports;
        auto snapshot = dust3d::native::readDocument(bytes, size, warning, nullptr, &imports, true);
        return build(*snapshot, imports.takeAssets(), maximum).release();
    } catch (const DocumentError& cause) {
        dust3d::native::setError(error, cause.status(), cause.what());
    } catch (const std::bad_alloc& cause) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, cause.what());
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown imported material ownership failure.");
    }
    return nullptr;
}

void dust3d_imported_assets_destroy(Dust3DImportedAssets* assets) { delete assets; }
size_t dust3d_imported_assets_model_count(const Dust3DImportedAssets* assets) { return assets ? assets->materials.size() : 0; }
size_t dust3d_imported_assets_binding_count(const Dust3DImportedAssets* assets) { return assets ? assets->bindings.size() : 0; }
size_t dust3d_imported_assets_payload_bytes(const Dust3DImportedAssets* assets) { return assets ? assets->payloadBytes : 0; }

Dust3DImportedMaterialView dust3d_imported_assets_material(const Dust3DImportedAssets* assets, size_t index)
{
    Dust3DImportedMaterialView view {};
    if (!assets || index >= assets->materials.size())
        return view;
    const auto& material = assets->materials[index];
    const auto& asset = material.asset;
    view.model_id = material.identity.c_str();
    std::copy(asset.baseColor.begin(), asset.baseColor.end(), view.base_color);
    view.metallic = asset.metallic;
    view.roughness = asset.roughness;
    std::copy(asset.emissive.begin(), asset.emissive.end(), view.emissive);
    view.alpha_mode = asset.alphaMode == dust3d::native::GlbAlphaMode::Mask ? DUST3D_IMPORTED_ALPHA_MASK
        : asset.alphaMode == dust3d::native::GlbAlphaMode::Blend            ? DUST3D_IMPORTED_ALPHA_BLEND
                                                                            : DUST3D_IMPORTED_ALPHA_OPAQUE;
    view.alpha_cutoff = asset.alphaCutoff;
    view.is_double_sided = asset.isDoubleSided ? 1 : 0;
    if (asset.sampler) {
        view.has_sampler = 1;
        view.has_magnification_filter = asset.sampler->magnification.has_value() ? 1 : 0;
        view.magnification_filter = asset.sampler->magnification.value_or(0);
        view.has_minification_filter = asset.sampler->minification.has_value() ? 1 : 0;
        view.minification_filter = asset.sampler->minification.value_or(0);
        view.wrap_s = asset.sampler->wrapS;
        view.wrap_t = asset.sampler->wrapT;
    }
    if (asset.image) {
        view.image.mime_type = asset.image->mimeType.c_str();
        view.image.bytes = asset.image->bytes.data();
        view.image.byte_count = asset.image->bytes.size();
    }
    return view;
}

Dust3DImportedBindingView dust3d_imported_assets_binding(const Dust3DImportedAssets* assets, size_t index)
{
    if (!assets || index >= assets->bindings.size())
        return {};
    const auto& binding = assets->bindings[index];
    return { binding.component.empty() ? nullptr : binding.component.c_str(), binding.part.c_str(), binding.modelIndex };
}

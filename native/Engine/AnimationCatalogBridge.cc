// SPDX-License-Identifier: MIT
#include "EngineSupport.h"
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
class CatalogMetadataError final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
void validateIdentity(const std::string& value)
{
    if (value.empty() || value.find('\0') != std::string::npos)
        throw CatalogMetadataError("Animation catalog identity is empty or contains a NUL byte.");
}
struct ClipRecord {
    std::string type;
    std::string rig;
    std::string generator;
    double duration;
    size_t samples;
    bool loop;
    bool alias;
    std::vector<std::pair<std::string, double>> parameters;
};
struct RigRecord {
    std::string name;
    std::vector<std::string> bones;
};
}
struct Dust3DAnimationCatalog {
    std::vector<ClipRecord> clips;
    std::vector<RigRecord> rigs;
};
Dust3DAnimationCatalog* dust3d_animation_catalog_load(Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        std::lock_guard<std::mutex> lock(engineMutex());
        auto owner = std::make_unique<Dust3DAnimationCatalog>();
        const auto& inherited = dust3d::animation::clipCatalog();
        owner->clips.reserve(inherited.size());
        std::set<std::string> rigs;
        for (const auto& entry : inherited) {
            const auto& info = entry.second;
            validateIdentity(entry.first);
            validateIdentity(info.rig);
            validateIdentity(info.generator);
            if (!std::isfinite(info.duration) || info.duration <= 0 || info.samples <= 0)
                throw CatalogMetadataError("Animation catalog timing must be finite and positive.");
            ClipRecord clip { entry.first, info.rig, info.generator, info.duration, static_cast<size_t>(info.samples), info.loop, info.alias, {} };
            clip.parameters.reserve(info.parameters.size());
            for (const auto& parameter : info.parameters) {
                validateIdentity(parameter.first);
                if (!std::isfinite(parameter.second))
                    throw CatalogMetadataError("Animation catalog override must be finite.");
                clip.parameters.push_back(parameter);
            }
            owner->clips.push_back(std::move(clip));
            rigs.insert(info.rig);
        }
        owner->rigs.reserve(rigs.size());
        for (const auto& name : rigs) {
            const auto& bones = dust3d::animation::standardBones(name);
            RigRecord rig { name, {} };
            rig.bones.reserve(bones.size());
            for (const auto& bone : bones) {
                validateIdentity(bone);
                rig.bones.push_back(bone);
            }
            owner->rigs.push_back(std::move(rig));
        }
        return owner.release();
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown non-standard animation catalog construction failure.");
    }
    return nullptr;
}
void dust3d_animation_catalog_destroy(Dust3DAnimationCatalog* catalog) { delete catalog; }
size_t dust3d_animation_catalog_clip_count(const Dust3DAnimationCatalog* catalog) { return catalog ? catalog->clips.size() : 0; }
int dust3d_animation_catalog_clip_at(const Dust3DAnimationCatalog* catalog, size_t index, Dust3DAnimationClipInfo* output)
{
    if (!output)
        return 0;
    *output = {};
    if (!catalog || index >= catalog->clips.size())
        return 0;
    const auto& clip = catalog->clips[index];
    *output = { clip.type.c_str(), clip.rig.c_str(), clip.generator.c_str(), clip.duration, clip.samples, clip.loop ? 1 : 0, clip.alias ? 1 : 0, clip.parameters.size() };
    return 1;
}
int dust3d_animation_catalog_parameter_at(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t parameterIndex, Dust3DAnimationParameterInfo* output)
{
    if (!output)
        return 0;
    *output = {};
    if (!catalog || clipIndex >= catalog->clips.size())
        return 0;
    const auto& parameters = catalog->clips[clipIndex].parameters;
    if (parameterIndex >= parameters.size())
        return 0;
    *output = { parameters[parameterIndex].first.c_str(), parameters[parameterIndex].second };
    return 1;
}
size_t dust3d_animation_catalog_rig_count(const Dust3DAnimationCatalog* catalog) { return catalog ? catalog->rigs.size() : 0; }
const char* dust3d_animation_catalog_rig_name(const Dust3DAnimationCatalog* catalog, size_t index)
{
    return catalog && index < catalog->rigs.size() ? catalog->rigs[index].name.c_str() : nullptr;
}
size_t dust3d_animation_catalog_bone_count(const Dust3DAnimationCatalog* catalog, size_t rigIndex)
{
    return catalog && rigIndex < catalog->rigs.size() ? catalog->rigs[rigIndex].bones.size() : 0;
}
const char* dust3d_animation_catalog_bone_name(const Dust3DAnimationCatalog* catalog, size_t rigIndex, size_t boneIndex)
{
    if (!catalog || rigIndex >= catalog->rigs.size())
        return nullptr;
    const auto& bones = catalog->rigs[rigIndex].bones;
    return boneIndex < bones.size() ? bones[boneIndex].c_str() : nullptr;
}

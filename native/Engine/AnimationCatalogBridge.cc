// SPDX-License-Identifier: MIT
#include "EngineSupport.h"
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/parameter_catalog.h>
#include <dust3d/animation/surface_material.h>
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
    std::vector<dust3d::animation::ParameterDefinition> controls;
};
struct RigRecord {
    std::string name;
    std::vector<std::string> bones;
};
}
struct Dust3DAnimationCatalog {
    std::vector<ClipRecord> clips;
    std::vector<RigRecord> rigs;
    std::vector<std::string> surfaceMaterials;
};
Dust3DAnimationCatalog* dust3d_animation_catalog_load(Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        std::lock_guard<std::mutex> lock(engineMutex());
        auto owner = std::make_unique<Dust3DAnimationCatalog>();
        for (const auto material : dust3d::surfaceMaterials()) {
            auto name = dust3d::surfaceMaterialName(material);
            validateIdentity(name);
            owner->surfaceMaterials.push_back(std::move(name));
        }
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
            ClipRecord clip { entry.first, info.rig, info.generator, info.duration, static_cast<size_t>(info.samples), info.loop, info.alias, {}, {} };
            clip.parameters.reserve(info.parameters.size());
            for (const auto& parameter : info.parameters) {
                validateIdentity(parameter.first);
                if (!std::isfinite(parameter.second))
                    throw CatalogMetadataError("Animation catalog override must be finite.");
                clip.parameters.push_back(parameter);
            }
            clip.controls = dust3d::animation::parameterDefinitions(entry.first);
            for (const auto& control : clip.controls) {
                validateIdentity(control.parameterName);
                validateIdentity(control.displayName);
                (void)dust3d::animation::parameterValue(control, control.minimumSliderValue);
                (void)dust3d::animation::parameterValue(control, control.maximumSliderValue);
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

namespace {
const dust3d::animation::ParameterDefinition* controlAt(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t controlIndex)
{
    if (!catalog || clipIndex >= catalog->clips.size())
        return nullptr;
    const auto& controls = catalog->clips[clipIndex].controls;
    return controlIndex < controls.size() ? &controls[controlIndex] : nullptr;
}
template <typename Output, typename Operation>
int convertControl(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t controlIndex,
    Output* output, Dust3DError* error, Operation operation)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    if (output)
        *output = {};
    const auto* control = controlAt(catalog, clipIndex, controlIndex);
    if (!output || !control) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Animation control conversion requires valid owner, indices and output.");
        return 0;
    }
    try {
        *output = operation(*control);
        return 1;
    } catch (const dust3d::animation::ParameterConversionError& cause) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown non-standard animation control conversion failure.");
    }
    return 0;
}
}
size_t dust3d_animation_catalog_control_count(const Dust3DAnimationCatalog* catalog, size_t clipIndex)
{
    return catalog && clipIndex < catalog->clips.size() ? catalog->clips[clipIndex].controls.size() : 0;
}
int dust3d_animation_catalog_control_at(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t controlIndex, Dust3DAnimationControlInfo* output)
{
    if (!output)
        return 0;
    *output = {};
    const auto* control = controlAt(catalog, clipIndex, controlIndex);
    if (!control)
        return 0;
    *output = { control->parameterName.c_str(), control->displayName.c_str(), control->defaultSliderValue,
        control->minimumSliderValue, control->maximumSliderValue, control->defaultParameterValue,
        control->sliderDivisor, control->parameterMultiplier };
    return 1;
}
int dust3d_animation_catalog_control_value(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t controlIndex,
    int slider, double* output, Dust3DError* error)
{
    return convertControl(catalog, clipIndex, controlIndex, output, error,
        [slider](const auto& control) { return dust3d::animation::parameterValue(control, slider); });
}
int dust3d_animation_catalog_control_slider(const Dust3DAnimationCatalog* catalog, size_t clipIndex, size_t controlIndex,
    double value, int* output, Dust3DError* error)
{
    return convertControl(catalog, clipIndex, controlIndex, output, error,
        [value](const auto& control) { return dust3d::animation::sliderValue(control, value); });
}

int dust3d_animation_timing_definition(Dust3DAnimationTimingDefinition* output)
{
    if (!output)
        return 0;
    const auto& definition = dust3d::animation::commonTimingDefinition();
    *output = { definition.minimumDurationSeconds, definition.maximumDurationSeconds,
        definition.durationDecimalPlaces, definition.durationStepSeconds,
        definition.minimumFrameCount, definition.maximumFrameCount, definition.frameCountStep };
    return 1;
}
int dust3d_animation_timing_check(double durationSeconds, double frameCount, Dust3DAnimationTiming* output, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    if (!output) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Animation timing requires an output destination.");
        return 0;
    }
    *output = {};
    try {
        const auto timing = dust3d::animation::checkedTiming(durationSeconds, frameCount);
        *output = { timing.durationSeconds, timing.frameCount };
        return 1;
    } catch (const dust3d::animation::ParameterConversionError& cause) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown non-standard animation timing validation failure.");
    }
    return 0;
}

size_t dust3d_animation_catalog_surface_material_count(const Dust3DAnimationCatalog* catalog)
{
    return catalog ? catalog->surfaceMaterials.size() : 0;
}
const char* dust3d_animation_catalog_surface_material_name(const Dust3DAnimationCatalog* catalog, size_t index)
{
    return catalog && index < catalog->surfaceMaterials.size() ? catalog->surfaceMaterials[index].c_str() : nullptr;
}

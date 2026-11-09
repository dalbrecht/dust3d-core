// SPDX-License-Identifier: MIT
#include "ExperimentalFishIdle.h"
#include "Cancellation.h"
#include "GroundedFishIdleSound.h"
#include <algorithm>
#include <optional>

using namespace dust3d;
using namespace dust3d::native;

struct Dust3DExperimentalFishIdle {
    GroundedFishIdleDrawPreview preview;
    AnimationSoundData sound;
    size_t visualBytes;
    size_t outputBytes;
    std::string selectedId;
};

Dust3DExperimentalFishIdleOptions dust3d_experimental_fish_idle_default_options(void)
{
    return { 1, sizeof(Dust3DExperimentalFishIdleOptions), dust3d_generated_rig_default_options() };
}

Dust3DExperimentalFishIdlePCMOptions dust3d_experimental_fish_idle_default_pcm_options(void)
{
    return { 2, sizeof(Dust3DExperimentalFishIdlePCMOptions), dust3d_generated_rig_default_options(), nullptr, 0 };
}

static SurfaceMaterial admitSurface(std::string_view name)
{
    for (const auto material : surfaceMaterials()) {
        if (surfaceMaterialName(material) == name)
            return material;
    }
    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unknown experimental FishIdle sound surface.");
}

static Dust3DExperimentalFishIdle* loadFishIdle(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameterCount,
    const Dust3DExperimentalFishIdleOptions* options, const Dust3DExperimentalFishIdlePCMOptions* pcmOptions,
    Dust3DError* error, std::string_view selectedId = {}, bool includeSelectedPCM = false)
{
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!archive || !archiveSize || parameterCount > 9 || (parameterCount && !parameters)
            || (options && (options->version != 1 || options->struct_size != sizeof(*options))))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid experimental FishIdle request.");
        auto config = pcmOptions ? pcmOptions->rig
                                 : (options ? options->rig : dust3d_generated_rig_default_options());
        checkCancellation(config.cancellation);
        std::optional<SurfaceMaterial> surface;
        if (pcmOptions) {
            if (!pcmOptions->surface || !pcmOptions->surface_length || pcmOptions->surface_length > 5)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid experimental FishIdle surface span.");
            surface = admitSurface(std::string_view(pcmOptions->surface, pcmOptions->surface_length));
        }
        AnimationParams authored;
        if (!selectedId.empty()) {
            auto fields = readSelectedAnimationMetadata(archive, archiveSize, selectedId);
            if (fields["type"] != "FishIdle")
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The selected animation is not FishIdle.");
            if (includeSelectedPCM) {
                const auto material = fields.find("surfaceMaterial");
                surface = material == fields.end() ? surfaceMaterials().front() : admitSurface(material->second);
            }
            for (const auto* metadata : { "id", "name", "type", "surfaceMaterial" })
                fields.erase(metadata);
            if (fields.size() > 9 || std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                    return field.first.empty() || field.first.size() > 64 || field.second.empty() || field.second.size() > 64;
                }))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid selected FishIdle parameters.");
            authored.values.insert(fields.begin(), fields.end());
            if (config.maximum_output_bytes <= selectedId.size() || config.maximum_output_bytes > 256 * 1024 * 1024)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The selected FishIdle identity exceeds the output limit.");
            config.maximum_output_bytes -= selectedId.size();
            checkCancellation(config.cancellation);
        }
        for (size_t i = 0; i < parameterCount; ++i) {
            const auto& entry = parameters[i];
            if (!entry.name || !entry.name_length || entry.name_length > 64
                || !entry.value || !entry.value_length || entry.value_length > 64
                || !authored.values.emplace(std::string(entry.name, entry.name_length),
                    std::string(entry.value, entry.value_length)).second)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid or duplicate experimental FishIdle parameter span.");
        }
        auto source = prepareGeneratedRigSource(archive, archiveSize, "Fish", 4, bones, boneCount, config);
        auto result = surface
            ? generateGroundedFishIdleSoundPreview(*source.input, std::move(source.metadata), source.rig,
                authored, *surface, source.options, config.maximum_draw_vertices, config.cancellation)
            : GroundedFishIdleSoundPreview { generateGroundedFishIdleDrawPreview(*source.input,
                  std::move(source.metadata), source.rig, authored, source.options,
                  config.maximum_draw_vertices, config.cancellation),
                {} };
        auto& preview = result.visual;
        const auto frames = preview.clip.sampleTimes().size();
        const auto joints = preview.rest->joints.size();
        const auto& matrices = preview.clip.modelFromJoint();
        if (!joints || joints != preview.rest->names.size() || joints != preview.clip.jointCount()
            || frames < 2 || matrices.size() % joints || matrices.size() / joints != frames)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Experimental FishIdle draw and pose counts differ.");
        const auto restBytes = preview.rest->outputBytes;
        const auto poseBytes = preview.clip.logicalPayloadBytes();
        const auto limit = config.maximum_output_bytes;
        const auto sampleCount = result.sound.pcmSamples.size();
        if (restBytes > limit || poseBytes > limit - restBytes
            || sampleCount > (limit - restBytes - poseBytes) / sizeof(int16_t))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Experimental FishIdle output exceeds its aggregate quota.");
        const auto visualBytes = restBytes + poseBytes;
        auto owner = std::make_unique<Dust3DExperimentalFishIdle>(
            Dust3DExperimentalFishIdle { std::move(preview), std::move(result.sound), visualBytes,
                visualBytes + sampleCount * sizeof(int16_t), std::string(selectedId) });
        checkCancellation(config.cancellation);
        return owner.release();
    } catch (const RigFinalDataError& cause) {
        const auto status = cause.failure() == RigFinalDataFailure::invalidData ? DUST3D_STATUS_INVALID_DOCUMENT
            : cause.failure() == RigFinalDataFailure::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
                                                                  : DUST3D_STATUS_CANCELLED;
        setError(error, status, cause.what());
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const PositionQuantizationError& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown experimental FishIdle failure.");
    }
    return nullptr;
}

Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameterCount,
    const Dust3DExperimentalFishIdleOptions* options, Dust3DError* error)
{
    return loadFishIdle(archive, archiveSize, bones, boneCount, parameters, parameterCount, options, nullptr, error);
}

Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load_pcm(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameterCount,
    const Dust3DExperimentalFishIdlePCMOptions* options, Dust3DError* error)
{
    if (!options || options->version != 2 || options->struct_size != sizeof(*options)) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Invalid experimental FishIdle PCM options.");
        return nullptr;
    }
    return loadFishIdle(archive, archiveSize, bones, boneCount, parameters, parameterCount, nullptr, options, error);
}

Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load_selected(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount, const char* animationId, size_t animationIdLength,
    uint32_t includePCM, const Dust3DExperimentalFishIdleOptions* options, Dust3DError* error)
{
    if (!animationId || !animationIdLength || animationIdLength > 1024 || includePCM > 1) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Invalid selected FishIdle request.");
        return nullptr;
    }
    return loadFishIdle(archive, archiveSize, bones, boneCount, nullptr, 0, options, nullptr, error,
        std::string_view(animationId, animationIdLength), includePCM != 0);
}

const char* dust3d_experimental_fish_idle_selected_id(const Dust3DExperimentalFishIdle* owner, size_t* length)
{
    if (!length)
        return nullptr;
    *length = 0;
    if (!owner || owner->selectedId.empty())
        return nullptr;
    *length = owner->selectedId.size();
    return owner->selectedId.data();
}

void dust3d_experimental_fish_idle_destroy(Dust3DExperimentalFishIdle* owner)
{
    delete owner;
}

int dust3d_experimental_fish_idle_describe(const Dust3DExperimentalFishIdle* owner,
    Dust3DExperimentalFishIdleInfo* output)
{
    if (!output)
        return 0;
    *output = {};
    if (!owner)
        return 0;
    const auto& [rest, clip] = owner->preview;
    *output = { rest.get(), clip.sampleTimes().data(), clip.animationType().data(), rest->mesh->warning.data(),
        clip.animationType().size(), rest->mesh->warning.size(), clip.sampleTimes().size(), clip.jointCount(),
        clip.modelFromJoint().size(), owner->visualBytes, clip.durationSeconds(), clip.loops() ? 1u : 0u };
    return 1;
}

int dust3d_experimental_fish_idle_matrix_at(const Dust3DExperimentalFishIdle* owner,
    size_t frame, size_t joint, Dust3DExperimentalFishIdleMatrix* output)
{
    if (!owner || !output)
        return 0;
    const auto& clip = owner->preview.clip;
    const auto jointCount = clip.jointCount();
    if (frame >= clip.sampleTimes().size() || joint >= jointCount)
        return 0;
    const auto& matrix = clip.modelFromJoint()[frame * jointCount + joint];
    std::copy(matrix.begin(), matrix.end(), output->values);
    return 1;
}

int dust3d_experimental_fish_idle_ground_motion(const Dust3DExperimentalFishIdle* owner,
    Dust3DExperimentalFishIdleGroundMotion* output)
{
    if (!owner || !output)
        return 0;
    const auto motion = owner->preview.clip.groundMotion();
    *output = { motion.speed, motion.directionX, motion.directionZ };
    return 1;
}

int dust3d_experimental_fish_idle_describe_pcm(const Dust3DExperimentalFishIdle* owner,
    Dust3DExperimentalFishIdlePCMInfo* output)
{
    if (!output)
        return 0;
    *output = {};
    if (!dust3d_experimental_fish_idle_describe(owner, &output->visual))
        return 0;
    const auto& sound = owner->sound;
    if (!sound.pcmSamples.empty()) {
        output->sample_rate = static_cast<uint32_t>(sound.sampleRate);
        output->channel_count = static_cast<uint32_t>(sound.channels);
        output->sample_count = sound.pcmSamples.size();
    }
    output->output_bytes = owner->outputBytes;
    return 1;
}

int dust3d_experimental_fish_idle_pcm_at(const Dust3DExperimentalFishIdle* owner, size_t sample, int16_t* output)
{
    if (!owner || !output || sample >= owner->sound.pcmSamples.size())
        return 0;
    *output = owner->sound.pcmSamples[sample];
    return 1;
}

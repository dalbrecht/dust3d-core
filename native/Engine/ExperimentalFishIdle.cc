// SPDX-License-Identifier: MIT
#include "ExperimentalFishIdle.h"
#include "Cancellation.h"
#include "GroundedFishIdlePreview.h"
#include <algorithm>

using namespace dust3d;
using namespace dust3d::native;

struct Dust3DExperimentalFishIdle {
    GroundedFishIdleDrawPreview preview;
    size_t outputBytes;
};

Dust3DExperimentalFishIdleOptions dust3d_experimental_fish_idle_default_options(void)
{
    return { 1, sizeof(Dust3DExperimentalFishIdleOptions), dust3d_generated_rig_default_options() };
}

Dust3DExperimentalFishIdle* dust3d_experimental_fish_idle_load(const uint8_t* archive, size_t archiveSize,
    const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleParameter* parameters, size_t parameterCount,
    const Dust3DExperimentalFishIdleOptions* options, Dust3DError* error)
{
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!archive || !archiveSize || parameterCount > 9 || (parameterCount && !parameters)
            || (options && (options->version != 1 || options->struct_size != sizeof(*options))))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid experimental FishIdle request.");
        const auto config = options ? *options : dust3d_experimental_fish_idle_default_options();
        checkCancellation(config.rig.cancellation);
        AnimationParams authored;
        for (size_t i = 0; i < parameterCount; ++i) {
            const auto& entry = parameters[i];
            if (!entry.name || !entry.name_length || entry.name_length > 64
                || !entry.value || !entry.value_length || entry.value_length > 64
                || !authored.values.emplace(std::string(entry.name, entry.name_length),
                    std::string(entry.value, entry.value_length)).second)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid or duplicate experimental FishIdle parameter span.");
        }
        auto source = prepareGeneratedRigSource(archive, archiveSize, "Fish", 4, bones, boneCount, config.rig);
        auto preview = generateGroundedFishIdleDrawPreview(*source.input, std::move(source.metadata), source.rig,
            authored, source.options, config.rig.maximum_draw_vertices, config.rig.cancellation);
        const auto frames = preview.clip.sampleTimes().size();
        const auto joints = preview.rest->joints.size();
        const auto& matrices = preview.clip.modelFromJoint();
        if (!joints || joints != preview.rest->names.size() || joints != preview.clip.jointCount()
            || frames < 2 || matrices.size() % joints || matrices.size() / joints != frames)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Experimental FishIdle draw and pose counts differ.");
        const auto restBytes = preview.rest->outputBytes;
        const auto poseBytes = preview.clip.logicalPayloadBytes();
        const auto limit = config.rig.maximum_output_bytes;
        if (restBytes > limit || poseBytes > limit - restBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Experimental FishIdle output exceeds its aggregate quota.");
        auto owner = std::make_unique<Dust3DExperimentalFishIdle>(
            Dust3DExperimentalFishIdle { std::move(preview), restBytes + poseBytes });
        checkCancellation(config.rig.cancellation);
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
        clip.modelFromJoint().size(), owner->outputBytes, clip.durationSeconds(), clip.loops() ? 1u : 0u };
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

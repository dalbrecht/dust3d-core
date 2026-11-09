// SPDX-License-Identifier: MIT
#include "ExperimentalFishIdleFbx.h"
#include "Cancellation.h"
#include "GeneratedRigDrawOwner.h"
#include "GroundedFishIdleFbx.h"
#include <algorithm>

using namespace dust3d;
using namespace dust3d::native;

struct Dust3DExperimentalFishIdleFbx {
    OwnedFbxDocument document;
};

Dust3DExperimentalFishIdleFbx* dust3d_experimental_fish_idle_fbx_create(
    const uint8_t* archive, size_t archiveSize, const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleFbxClipID* ids, size_t clipCount, const Dust3DGeneratedRigOptions* options,
    size_t maximumFbxBytes, Dust3DError* error)
{
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!ids || !clipCount || clipCount > 128)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FishIdle FBX requires selected clip identities.");
        if (maximumFbxBytes <= sizeof(OwnedFbxDocument) || maximumFbxBytes > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Animated FishIdle FBX exceeds its writer quota.");
        const auto config = options ? *options : dust3d_generated_rig_default_options();
        if (config.version != 1 || config.struct_size != sizeof(config))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unsupported FishIdle FBX rig options.");
        checkCancellation(config.cancellation);
        std::vector<std::string_view> requested;
        requested.reserve(clipCount);
        for (size_t index = 0; index < clipCount; ++index) {
            if (!ids[index].id || !ids[index].length || ids[index].length > 1024)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid animated FishIdle clip identity span.");
            requested.emplace_back(ids[index].id, ids[index].length);
        }
        auto records = readSelectedAnimationMetadataBatch(archive, archiveSize, requested);
        std::vector<FishIdleFbxClipInput> clips;
        clips.reserve(records.size());
        for (size_t index = 0; index < records.size(); ++index) {
            checkCancellation(config.cancellation);
            auto& fields = records[index];
            if (fields["type"] != "FishIdle")
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The selected FBX clip is not FishIdle.");
            const auto name = fields.find("name");
            auto title = name == fields.end() || name->second.empty() ? std::string(requested[index]) : std::move(name->second);
            if (title.size() > 1024 || title.find('\0') != std::string::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid animated FishIdle FBX clip name.");
            for (const auto* field : { "id", "name", "type", "surfaceMaterial" })
                fields.erase(field);
            if (fields.size() > 9 || std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                    return field.first.empty() || field.first.size() > 64 || field.second.empty() || field.second.size() > 64;
                }))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid selected FishIdle FBX parameters.");
            FishIdleFbxClipInput input;
            input.name = std::move(title);
            input.parameters.values.insert(fields.begin(), fields.end());
            clips.push_back(std::move(input));
        }
        checkCancellation(config.cancellation);
        auto source = prepareGeneratedRigSource(archive, archiveSize, "Fish", 4, bones, boneCount, config);
        auto document = encodeGroundedFishIdleFbx(*source.input, source.rig, clips, nullptr,
            source.options, { maximumFbxBytes, 65536 });
        checkCancellation(config.cancellation);
        return new Dust3DExperimentalFishIdleFbx { std::move(document) };
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
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown animated FishIdle FBX failure.");
    }
    return nullptr;
}

const uint8_t* dust3d_experimental_fish_idle_fbx_bytes(const Dust3DExperimentalFishIdleFbx* owner)
{
    return owner ? owner->document.bytes().data() : nullptr;
}

size_t dust3d_experimental_fish_idle_fbx_size(const Dust3DExperimentalFishIdleFbx* owner)
{
    return owner ? owner->document.bytes().size() : 0;
}

void dust3d_experimental_fish_idle_fbx_destroy(Dust3DExperimentalFishIdleFbx* owner)
{
    delete owner;
}

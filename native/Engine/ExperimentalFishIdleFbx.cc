// SPDX-License-Identifier: MIT
#include "ExperimentalFishIdleFbx.h"
#include "BakedMesh.h"
#include "Cancellation.h"
#include "FishIdleEvaluation.h"
#include "GeneratedRigDrawOwner.h"
#include "GroundedFishIdleFbx.h"
#include <algorithm>
#include <optional>

using namespace dust3d;
using namespace dust3d::native;

struct Dust3DExperimentalFishIdleFbx {
    OwnedFbxDocument document;
};

static Dust3DExperimentalFishIdleFbx* createFishIdleFbx(
    const uint8_t* archive, size_t archiveSize, const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleFbxClipID* ids, size_t clipCount, const Dust3DGeneratedRigOptions* options,
    const Dust3DBakeOptions* bakeOptions, size_t maximumFbxBytes, Dust3DError* error)
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
        if (bakeOptions && (bakeOptions->struct_size != sizeof(*bakeOptions) || bakeOptions->cancellation != config.cancellation || !bakeOptions->maximum_uv_payload_bytes || !bakeOptions->maximum_bake_payload_bytes))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Animated FBX bake options require explicit budgets and a shared token.");
        checkCancellation(config.cancellation);
        std::vector<std::string_view> requested;
        requested.reserve(clipCount);
        for (size_t index = 0; index < clipCount; ++index) {
            if (!ids[index].id || !ids[index].length || ids[index].length > 1024)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid animated FishIdle clip identity span.");
            requested.emplace_back(ids[index].id, ids[index].length);
        }
        std::string rigType;
        auto records = readSelectedAnimationMetadataBatch(archive, archiveSize, requested, &rigType);
        std::vector<FishIdleFbxClipInput> clips;
        clips.reserve(records.size());
        for (size_t index = 0; index < records.size(); ++index) {
            checkCancellation(config.cancellation);
            auto& fields = records[index];
            const auto clipType = fields["type"];
            if (rigType.empty() || evaluatedClipRig(clipType) != rigType)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "The selected FBX clip does not match its authored rig.");
            const auto name = fields.find("name");
            auto title = name == fields.end() || name->second.empty() ? std::string(requested[index]) : std::move(name->second);
            if (title.size() > 1024 || title.find('\0') != std::string::npos)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid animated FishIdle FBX clip name.");
            for (const auto* field : { "id", "name", "type", "surfaceMaterial" })
                fields.erase(field);
            if (fields.size() > evaluatedClipParameterLimit(clipType) || std::any_of(fields.begin(), fields.end(), [](const auto& field) {
                    return field.first.empty() || field.first.size() > 64 || field.second.empty() || field.second.size() > 64;
                }))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid selected FishIdle FBX parameters.");
            FishIdleFbxClipInput input;
            input.name = std::move(title);
            input.clipType = clipType;
            input.parameters.values.insert(fields.begin(), fields.end());
            clips.push_back(std::move(input));
        }
        checkCancellation(config.cancellation);
        std::unique_ptr<Snapshot> authored;
        std::optional<FishIdleFbxBakeInput> bake;
        if (bakeOptions) {
            std::string warning;
            authored = readDocumentMetadata(archive, archiveSize, warning);
            bake.emplace(FishIdleFbxBakeInput { *authored, *bakeOptions });
        }
        auto source = prepareGeneratedRigSource(archive, archiveSize, rigType.data(), rigType.size(), bones, boneCount, config);
        auto document = encodeGroundedFishIdleFbx(*source.input, source.rig, clips, nullptr,
            source.options, { maximumFbxBytes, 65536 }, bake ? &*bake : nullptr);
        checkCancellation(config.cancellation);
        return new Dust3DExperimentalFishIdleFbx { std::move(document) };
    } catch (const RigFinalDataError& cause) {
        const auto status = cause.failure() == RigFinalDataFailure::invalidData ? DUST3D_STATUS_INVALID_DOCUMENT
            : cause.failure() == RigFinalDataFailure::limitExceeded             ? DUST3D_STATUS_LIMIT_EXCEEDED
                                                                                : DUST3D_STATUS_CANCELLED;
        setError(error, status, cause.what());
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const PositionQuantizationError& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const TextureBakeError& cause) {
        const auto status = cause.code() == TextureBakeErrorCode::cancelled ? DUST3D_STATUS_CANCELLED
            : cause.code() == TextureBakeErrorCode::limitExceeded           ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == TextureBakeErrorCode::invalidInput            ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                            : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const UvAtlasError& cause) {
        const auto status = cause.code() == UvAtlasErrorCode::cancelled ? DUST3D_STATUS_CANCELLED
            : cause.code() == UvAtlasErrorCode::limitExceeded           ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == UvAtlasErrorCode::invalidInput            ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                        : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown animated FishIdle FBX failure.");
    }
    return nullptr;
}

Dust3DExperimentalFishIdleFbx* dust3d_experimental_fish_idle_fbx_create(
    const uint8_t* archive, size_t archiveSize, const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleFbxClipID* ids, size_t clipCount, const Dust3DGeneratedRigOptions* options,
    size_t maximumFbxBytes, Dust3DError* error)
{
    return createFishIdleFbx(archive, archiveSize, bones, boneCount, ids, clipCount, options, nullptr, maximumFbxBytes, error);
}

Dust3DExperimentalFishIdleFbx* dust3d_experimental_fish_idle_fbx_create_baked(
    const uint8_t* archive, size_t archiveSize, const Dust3DGeneratedRigBoneInput* bones, size_t boneCount,
    const Dust3DExperimentalFishIdleFbxClipID* ids, size_t clipCount, const Dust3DGeneratedRigOptions* options,
    const Dust3DBakeOptions* bake, size_t maximumFbxBytes, Dust3DError* error)
{
    if (!bake) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Textured FishIdle FBX requires bake options.");
        return nullptr;
    }
    return createFishIdleFbx(archive, archiveSize, bones, boneCount, ids, clipCount, options, bake, maximumFbxBytes, error);
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

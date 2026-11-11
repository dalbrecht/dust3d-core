// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include "GeneratedRigDrawOwner.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include "WorkerUtf8.h"
#include <algorithm>
#include <dust3d/rig/rig_generator.h>
#include <string_view>
namespace {
using namespace dust3d::native;
[[noreturn]] void invalid(const char* message) { throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, message); }
[[noreturn]] void limit(const char* message) { throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, message); }
std::string text(const char* bytes, size_t size, bool empty)
{
    if (size > 1024)
        limit("Generated rig identity exceeds 1024 bytes.");
    if ((!bytes && size) || (!empty && !size))
        invalid("Invalid generated rig identity span.");
    const auto view = bytes ? std::string_view(bytes, size) : std::string_view {};
    if (!dust3d::worker::detail::isIdentityUtf8(view))
        invalid("Generated rig identity is not non-NUL UTF8.");
    for (unsigned char byte : view)
        if (byte < 32 || byte == 127)
            invalid("Generated rig identity contains a control.");
    return std::string(view);
}
void charge(size_t& total, size_t count, size_t unit, size_t maximum)
{
    if (total > maximum || count > (maximum - total) / unit)
        limit("Generated rig output/staging exceeds its quota.");
    total += count * unit;
}
}
std::unique_ptr<Dust3DGeneratedRig> dust3d::native::makeGeneratedRigDrawOwner(
    const FinalRigGeneration& final, std::unique_ptr<Dust3DMesh> metadata,
    size_t maximumDrawVertices, size_t maximumOutputBytes, const Dust3DCancellationToken* cancellation,
    const Snapshot* generated, const GeneratedRigDrawBakeInput* bake)
{
    if (!metadata)
        invalid("Generated rig draw preparation requires mesh metadata.");
    auto owner = std::make_unique<Dust3DGeneratedRig>();
    owner->mesh = std::move(metadata);
    owner->eyelidsGenerated = final.eyelidsGenerated;
    if (final.restTransforms.size() != final.data.joints.size())
        invalid("Generated rig rest/joint counts differ.");
    size_t baseBytes = 0;
    charge(baseBytes, final.data.joints.size(), sizeof(Dust3DGeneratedRigJoint), maximumOutputBytes);
    for (const auto& joint : final.data.joints)
        charge(baseBytes, joint.name.size() + 1, 1, maximumOutputBytes);
    charge(baseBytes, owner->mesh->warning.size() + 1, 1, maximumOutputBytes);
    if (final.object->triangles.size() > maximumDrawVertices / 3)
        limit("Generated rig draw count exceeds its limit.");
    const auto maximumDraw = final.object->triangles.size() * 3;
    size_t stagingBytes = baseBytes;
    charge(stagingBytes, maximumDraw, sizeof(Dust3DVertex) + sizeof(Dust3DGeneratedRigWeights) + sizeof(size_t), maximumOutputBytes);
    std::vector<size_t> originalIndices;
    {
        std::lock_guard<std::mutex> lock(engineMutex());
        ImportedModels imports;
        GenerationOutputs outputs;
        outputs.previewVertexIndices = &originalIndices;
        outputs.cancellation = cancellation;
        if (bake) {
            const auto& options = bake->options;
            if (!generated || options.legacy.cancellation != cancellation || options.model_image_count)
                invalid("Grounded rig source baking requires a matching procedural snapshot and cancellation token.");
            if (!options.maximum_output_payload_bytes || !options.maximum_sampler_payload_bytes
                || !options.maximum_atlas_payload_bytes || !options.legacy.maximum_uv_payload_bytes
                || !options.legacy.maximum_bake_payload_bytes)
                invalid("Grounded rig source baking requires explicit payload budgets.");
            for (const auto bytes : { options.maximum_output_payload_bytes, options.maximum_sampler_payload_bytes,
                     options.maximum_atlas_payload_bytes, options.legacy.maximum_uv_payload_bytes,
                     options.legacy.maximum_bake_payload_bytes })
                charge(stagingBytes, bytes, 1, maximumOutputBytes);
            owner->sourceBake = std::make_unique<SourceBakedData>(options, imports);
            owner->sourceBake->validateVisuals(bake->authored);
            owner->sourceSurfaces = std::make_unique<ImportedSurfaceData>(
                bake->authored, imports, owner->sourceBake->maximumOutputBytes());
            outputs.sourceBaked = owner->sourceBake.get();
            outputs.surfaces = owner->sourceSurfaces.get();
        }
        prepareGeneratedObject(*final.object, generated, *owner->mesh, outputs);
    }
    if (originalIndices.size() != owner->mesh->vertices.size())
        invalid("Generated rig draw mapping count differs.");
    if (owner->sourceBake
        && (owner->sourceBake->triangles.size() != originalIndices.size() / 3
            || owner->sourceSurfaces->triangles().size() != originalIndices.size() / 3))
        invalid("Grounded rig source bake does not match the retained draw triangles.");
    owner->weights.reserve(originalIndices.size());
    for (size_t i = 0; i < originalIndices.size(); ++i) {
        if (i % 256 == 0)
            checkCancellation(cancellation);
        if (originalIndices[i] >= final.data.vertices.size())
            invalid("Generated rig draw mapping references a missing original vertex.");
        const auto& source = final.data.vertices[originalIndices[i]];
        Dust3DGeneratedRigWeights value {};
        std::copy(source.joints.begin(), source.joints.end(), value.joints);
        std::copy(source.weights.begin(), source.weights.end(), value.weights);
        owner->weights.push_back(value);
    }
    owner->joints.reserve(final.data.joints.size());
    owner->names.reserve(final.data.joints.size());
    for (size_t i = 0; i < final.data.joints.size(); ++i) {
        checkCancellation(cancellation);
        const auto& source = final.data.joints[i];
        Dust3DGeneratedRigJoint value {};
        value.parent_index = source.parentIndex;
        value.capsule_radius = source.capsuleRadius;
        value.closing_angle = source.closingAngle;
        std::copy(source.headPosition.begin(), source.headPosition.end(), value.head_position);
        std::copy(source.tailPosition.begin(), source.tailPosition.end(), value.tail_position);
        std::copy(final.restTransforms[i].world.begin(), final.restTransforms[i].world.end(), value.rest_world);
        std::copy(final.restTransforms[i].inverseBind.begin(), final.restTransforms[i].inverseBind.end(), value.inverse_bind);
        owner->joints.push_back(value);
        owner->names.push_back(source.name);
    }
    owner->outputBytes = baseBytes;
    charge(owner->outputBytes, owner->mesh->vertices.capacity(), sizeof(Dust3DVertex), maximumOutputBytes);
    charge(owner->outputBytes, owner->weights.size(), sizeof(Dust3DGeneratedRigWeights), maximumOutputBytes);
    if (owner->sourceBake)
        charge(owner->outputBytes, owner->sourceBake->payloadBytes, 1, maximumOutputBytes);
    checkCancellation(cancellation);
    return owner;
}
PreparedGeneratedRigSource dust3d::native::prepareGeneratedRigSource(const uint8_t* bytes, size_t size,
    const char* type, size_t typeSize, const Dust3DGeneratedRigBoneInput* input, size_t count,
    const Dust3DGeneratedRigOptions& config)
{
    if (config.version != 1 || config.struct_size != sizeof(config))
        invalid("Unsupported generated rig options version/size.");
    if (config.head_has_eyelids > 1 || config.reserved || !config.maximum_bones || config.maximum_bones > 256
        || !config.maximum_original_vertices || config.maximum_original_vertices > 1000000
        || !config.maximum_draw_vertices || config.maximum_draw_vertices > 15000000
        || !config.maximum_output_bytes || config.maximum_output_bytes > 256 * 1024 * 1024
        || !config.maximum_retained_input_bytes || config.maximum_retained_input_bytes > 256 * 1024 * 1024)
        invalid("Invalid generated rig options/limits.");
    if (!count || !input)
        invalid("Generated rig requires a nonempty template span.");
    if (count > config.maximum_bones)
        limit("Generated rig template exceeds its bone limit.");
    checkCancellation(config.cancellation);
    RigStructure templ;
    templ.type = text(type, typeSize, false);
    templ.headHasEyelids = config.head_has_eyelids != 0;
    templ.bones.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        checkCancellation(config.cancellation);
        RigNode bone;
        bone.name = text(input[i].name, input[i].name_length, false);
        bone.parent = text(input[i].parent, input[i].parent_length, true);
        bone.closingAngle = input[i].closing_angle;
        templ.bones.push_back(std::move(bone));
    }
    RigFinalDataOptions native;
    native.maximumBones = config.maximum_bones;
    native.maximumVertices = config.maximum_original_vertices;
    native.maximumPayloadBytes = config.maximum_output_bytes;
    native.cancellation = config.cancellation ? &config.cancellation->cancelled : nullptr;
    (void)copyFinalRigData(templ, Object {}, native);
    std::unique_ptr<GeneratedRigInput> retained;
    std::unique_ptr<Dust3DMesh> metadata;
    {
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(config.cancellation);
        std::string warning;
        auto snapshot = readDocument(bytes, size, warning);
        GenerationOutputs outputs;
        outputs.purpose = GenerationPurpose::rigInputOnly;
        outputs.rigInput = &retained;
        outputs.maximumRigInputBytes = config.maximum_retained_input_bytes;
        outputs.cancellation = config.cancellation;
        metadata = generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, outputs);
    }
    return { std::move(templ), native, std::move(retained), std::move(metadata) };
}

Dust3DGeneratedRigOptions dust3d_generated_rig_default_options(void)
{
    Dust3DGeneratedRigOptions options {};
    options.version = 1;
    options.struct_size = sizeof(options);
    options.maximum_bones = 256;
    options.maximum_original_vertices = 1000000;
    options.maximum_draw_vertices = 15000000;
    options.maximum_output_bytes = 64 * 1024 * 1024;
    options.maximum_retained_input_bytes = 64 * 1024 * 1024;
    return options;
}
Dust3DGeneratedRig* dust3d_generated_rig_load(const uint8_t* bytes, size_t size, const char* type, size_t typeSize,
    const Dust3DGeneratedRigBoneInput* input, size_t count, const Dust3DGeneratedRigOptions* options, Dust3DError* error)
{
    using namespace dust3d;
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (options && (options->version != 1 || options->struct_size != sizeof(*options)))
            invalid("Unsupported generated rig options version/size.");
        const auto config = options ? *options : dust3d_generated_rig_default_options();
        auto prepared = prepareGeneratedRigSource(bytes, size, type, typeSize, input, count, config);
        auto final = generateFinalRig(*prepared.input, prepared.rig, prepared.options);
        return makeGeneratedRigDrawOwner(final, std::move(prepared.metadata), config.maximum_draw_vertices,
            config.maximum_output_bytes, config.cancellation).release();
    } catch (const RigFinalDataError& e) {
        const auto status = e.failure() == RigFinalDataFailure::invalidData ? DUST3D_STATUS_INVALID_DOCUMENT : e.failure() == RigFinalDataFailure::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
                                                                                                                                                                 : DUST3D_STATUS_CANCELLED;
        setError(error, status, e.what());
    } catch (const DocumentError& e) {
        setError(error, e.status(), e.what());
    } catch (const dust3d::PositionQuantizationError& e) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, e.what());
    } catch (const std::bad_alloc& e) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, e.what());
    } catch (const std::exception& e) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, e.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown generated rig failure.");
    }
    return nullptr;
}
void dust3d_generated_rig_destroy(Dust3DGeneratedRig* rig) { delete rig; }
const Dust3DMesh* dust3d_generated_rig_mesh(const Dust3DGeneratedRig* rig) { return rig ? rig->mesh.get() : nullptr; }
size_t dust3d_generated_rig_joint_count(const Dust3DGeneratedRig* rig) { return rig ? rig->joints.size() : 0; }
const Dust3DGeneratedRigJoint* dust3d_generated_rig_joints(const Dust3DGeneratedRig* rig) { return rig && !rig->joints.empty() ? rig->joints.data() : nullptr; }
const char* dust3d_generated_rig_joint_name(const Dust3DGeneratedRig* rig, size_t index, size_t* count)
{
    if (count)
        *count = 0;
    if (!rig || index >= rig->names.size())
        return nullptr;
    if (count)
        *count = rig->names[index].size();
    return rig->names[index].c_str();
}
const Dust3DGeneratedRigWeights* dust3d_generated_rig_weights(const Dust3DGeneratedRig* rig) { return rig && !rig->weights.empty() ? rig->weights.data() : nullptr; }
uint32_t dust3d_generated_rig_eyelids_generated(const Dust3DGeneratedRig* rig) { return rig && rig->eyelidsGenerated ? 1 : 0; }
size_t dust3d_generated_rig_output_bytes(const Dust3DGeneratedRig* rig) { return rig ? rig->outputBytes : 0; }

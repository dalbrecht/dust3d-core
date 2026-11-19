// SPDX-License-Identifier: MIT
#include "EngineSupport.h"
#include "RigTemplateMesh.h"
#include "WorkerUtf8.h"
#include <cmath>
#include <limits>
#include <memory>
#include <string_view>
struct Dust3DRigTemplateMesh {
    std::vector<std::string> names;
    std::vector<Dust3DRigTemplateDrawVertex> vertices;
    size_t outputBytes = 0;
};
namespace {
class BridgeFailure : public std::runtime_error {
public:
    BridgeFailure(Dust3DStatus value, const char* message)
        : std::runtime_error(message)
        , status(value)
    {
    }
    Dust3DStatus status;
};
void reject(Dust3DStatus status, const char* cause) { throw BridgeFailure(status, cause); }
void cancelled(const Dust3DCancellationToken* token)
{
    if (dust3d_cancellation_token_is_cancelled(token))
        reject(DUST3D_STATUS_CANCELLED, "Rig template visualization was cancelled.");
}
std::string nameCopy(const char* data, size_t size, bool allowEmpty)
{
    if (size > 1024)
        reject(DUST3D_STATUS_LIMIT_EXCEEDED, "Rig template identity exceeds 1024 bytes.");
    if ((!data && size) || (!allowEmpty && !size))
        reject(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid rig template identity span.");
    const auto view = data ? std::string_view(data, size) : std::string_view();
    // Reuse the allocation-free primitive; no worker/runtime product is linked.
    if (!dust3d::worker::detail::isIdentityUtf8(view))
        reject(DUST3D_STATUS_INVALID_DOCUMENT, "Rig template identity is not non-NUL UTF8.");
    return size ? std::string(data, size) : std::string();
}
void charge(size_t& total, size_t count, size_t unit, size_t maximum)
{
    if (total > maximum || count > (maximum - total) / unit)
        reject(DUST3D_STATUS_LIMIT_EXCEEDED, "Rig template retained output exceeds its quota.");
    total += count * unit;
}
float checkedFloat(double value)
{
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        reject(DUST3D_STATUS_GENERATION_FAILED, "Rig template draw value cannot be represented as a finite float.");
    return static_cast<float>(value);
}
}
Dust3DRigTemplateOptions dust3d_rig_template_default_options(void)
{
    Dust3DRigTemplateOptions value {};
    value.version = 1;
    value.struct_size = sizeof(value);
    value.start_radius = .02;
    value.normalize = 1;
    value.maximum_bones = 256;
    value.maximum_vertices = 500000;
    value.maximum_triangles = 200000;
    value.maximum_output_bytes = 32 * 1024 * 1024;
    return value;
}
Dust3DRigTemplateMesh* dust3d_rig_template_mesh_create(const Dust3DRigTemplateBoneInput* input, size_t count, const Dust3DRigTemplateOptions* options, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (options && (options->version != 1 || options->struct_size != sizeof(Dust3DRigTemplateOptions)))
            reject(DUST3D_STATUS_INVALID_DOCUMENT, "Unsupported rig template options version or size.");
        const auto config = options ? *options : dust3d_rig_template_default_options();
        if (config.normalize > 1 || config.reserved || !config.maximum_bones || config.maximum_bones > 256 || !config.maximum_vertices || !config.maximum_triangles || !config.maximum_output_bytes)
            reject(DUST3D_STATUS_INVALID_DOCUMENT, "Invalid rig template options or output limits.");
        if (count > config.maximum_bones)
            reject(DUST3D_STATUS_LIMIT_EXCEEDED, "Rig template bone count exceeds its limit.");
        if (!input && count)
            reject(DUST3D_STATUS_INVALID_DOCUMENT, "Rig template bone span is NULL.");
        cancelled(config.cancellation);
        std::lock_guard<std::mutex> lock(engineMutex());
        cancelled(config.cancellation);
        RigTemplateMeshOptions native;
        native.startRadius = config.start_radius;
        native.normalize = config.normalize != 0;
        native.maximumBones = config.maximum_bones;
        native.maximumVertices = config.maximum_vertices;
        native.maximumTriangles = config.maximum_triangles;
        native.maximumOutputBytes = config.maximum_output_bytes;
        native.selectedBoneName = nameCopy(config.selected_bone_name, config.selected_bone_name_length, true);
        auto owner = std::make_unique<Dust3DRigTemplateMesh>();
        std::vector<RigTemplateBone> bones;
        bones.reserve(count);
        owner->names.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            cancelled(config.cancellation);
            auto name = nameCopy(input[i].name, input[i].name_length, false);
            charge(owner->outputBytes, name.size() + 1, 1, config.maximum_output_bytes);
            bones.push_back({ name, { input[i].from_position[0], input[i].from_position[1], input[i].from_position[2] },
                { input[i].to_position[0], input[i].to_position[1], input[i].to_position[2] }, input[i].capsule_radius });
            owner->names.push_back(std::move(name));
        }
        const auto mesh = buildRigTemplateMesh(bones, native);
        cancelled(config.cancellation);
        if (mesh.triangles.size() > config.maximum_vertices / 3)
            reject(DUST3D_STATUS_LIMIT_EXCEEDED, "Rig template expanded draw count exceeds its limit.");
        const auto drawCount = mesh.triangles.size() * 3;
        charge(owner->outputBytes, drawCount, sizeof(Dust3DRigTemplateDrawVertex), config.maximum_output_bytes);
        owner->vertices.reserve(drawCount);
        for (const auto& triangle : mesh.triangles) {
            cancelled(config.cancellation);
            for (auto index : triangle)
                if (index >= mesh.vertices.size())
                    reject(DUST3D_STATUS_GENERATION_FAILED, "Rig template triangle contains an invalid index.");
            const auto normal = dust3d::Vector3::normal(mesh.vertices[triangle[0]].position, mesh.vertices[triangle[1]].position, mesh.vertices[triangle[2]].position);
            for (auto index : triangle) {
                const auto& vertex = mesh.vertices[index];
                if (vertex.boneIndex >= owner->names.size())
                    reject(DUST3D_STATUS_GENERATION_FAILED, "Rig template draw record has an invalid bone index.");
                Dust3DRigTemplateDrawVertex draw {};
                for (size_t axis = 0; axis < 3; ++axis) {
                    draw.position[axis] = checkedFloat(vertex.position[axis]);
                    draw.normal[axis] = checkedFloat(normal[axis]);
                }
                draw.bone_index = vertex.boneIndex;
                draw.role = static_cast<uint32_t>(vertex.role);
                owner->vertices.push_back(draw);
            }
        }
        cancelled(config.cancellation);
        return owner.release();
    } catch (const BridgeFailure& cause) {
        setError(error, cause.status, cause.what());
    } catch (const RigTemplateMeshError& cause) {
        Dust3DStatus status = DUST3D_STATUS_GENERATION_FAILED;
        if (cause.failure() == RigTemplateMeshFailure::invalidInput)
            status = DUST3D_STATUS_INVALID_DOCUMENT;
        if (cause.failure() == RigTemplateMeshFailure::limitExceeded)
            status = DUST3D_STATUS_LIMIT_EXCEEDED;
        setError(error, status, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown rig template visualization failure.");
    }
    return nullptr;
}
void dust3d_rig_template_mesh_destroy(Dust3DRigTemplateMesh* mesh) { delete mesh; }
size_t dust3d_rig_template_mesh_vertex_count(const Dust3DRigTemplateMesh* mesh) { return mesh ? mesh->vertices.size() : 0; }
const Dust3DRigTemplateDrawVertex* dust3d_rig_template_mesh_vertices(const Dust3DRigTemplateMesh* mesh) { return mesh && !mesh->vertices.empty() ? mesh->vertices.data() : nullptr; }
size_t dust3d_rig_template_mesh_bone_count(const Dust3DRigTemplateMesh* mesh) { return mesh ? mesh->names.size() : 0; }
const char* dust3d_rig_template_mesh_bone_name(const Dust3DRigTemplateMesh* mesh, size_t index, size_t* byteCount)
{
    if (byteCount)
        *byteCount = 0;
    if (!mesh || index >= mesh->names.size())
        return nullptr;
    if (byteCount)
        *byteCount = mesh->names[index].size();
    return mesh->names[index].c_str();
}
size_t dust3d_rig_template_mesh_output_bytes(const Dust3DRigTemplateMesh* mesh) { return mesh ? mesh->outputBytes : 0; }

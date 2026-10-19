// SPDX-License-Identifier: MIT
#include "ImportedSurface.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include "UvMetadata.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace dust3d {
namespace native {
    void ImportedSurfaceData::charge(size_t count, size_t width)
    {
        if (!m_budget.tryAdd(count, width))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Imported surface output exceeds its logical ownership budget.");
    }
    std::string ImportedSurfaceData::copyIdentity(const std::string& value)
    {
        charge(value.size() + 1);
        std::string copy(value);
        charge(copy.capacity() - copy.size());
        return copy;
    }
    void ImportedSurfaceData::addComponent(const std::string& value, Dust3DSurfaceComponentKind kind)
    {
        if (m_components.size() == m_components.capacity()) {
            // The 256 MiB quota bounds record capacity well below size_t/2, even on 32-bit targets.
            reserve(m_components, std::max(m_components.size() + 1, m_components.capacity() * 2));
        }
        const size_t index = m_components.size();
        m_components.push_back({ copyIdentity(value), kind });
        m_componentIndices.emplace(value, index);
    }
    ImportedSurfaceData::ImportedSurfaceData(const Snapshot& snapshot, const ImportedModels& imports, size_t maximumBytes)
        : m_maximumBytes(maximumBytes)
        , m_budget(maximumBytes)
    {
        charge(1, sizeof(Dust3DImportedSurfaceMesh) + sizeof(ImportedSurfaceData) + sizeof(Dust3DMesh));
        std::set<std::string> models;
        for (const auto& [key, part] : snapshot.parts)
            if (String::valueOrEmpty(part, "target") == "ImportedModel")
                models.insert(String::valueOrEmpty(part, "importedModelId"));
        reserve(m_models, models.size());
        for (const auto& identity : models) {
            m_modelIndices.emplace(identity, m_models.size());
            m_models.push_back(copyIdentity(identity));
            if (imports.hasVertexColors(identity))
                m_linearModels.insert(identity);
        }
        reserve(m_components, snapshot.components.size());
        for (const auto& [key, attributes] : snapshot.components) {
            (void)normalizedUvColor(String::valueOrEmpty(attributes, "color"));
            addComponent(key, DUST3D_SURFACE_COMPONENT_ORIGINAL);
        }
    }
    void ImportedSurfaceData::prepare(const Object& object, const std::string& warning)
    {
        const size_t count = object.triangles.size();
        for (size_t size : { object.triangleCornerUvs.size(), object.triangleModelKeys.size(), object.triangleComponentKeys.size() })
            if (size && size != count)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Generated surface channels are not triangle-aligned.");
        charge(count, 3 * sizeof(Dust3DVertex));
        charge(warning.capacity() + 1);
        reserve(m_triangles, count);
        for (const auto& key : object.triangleComponentKeys)
            if (!key.empty() && !m_componentIndices.count(key))
                addComponent(key, DUST3D_SURFACE_COMPONENT_GENERATED);
        m_object = &object;
    }
    void ImportedSurfaceData::appendTriangle(size_t index)
    {
        Dust3DImportedSurfaceTriangle result {};
        result.model_index = SIZE_MAX;
        result.component_index = SIZE_MAX;
        const auto& object = *m_object;
        if (!object.triangleModelKeys.empty() && !object.triangleModelKeys.at(index).empty()) {
            const auto found = m_modelIndices.find(object.triangleModelKeys[index]);
            if (found == m_modelIndices.end())
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Generated surface refers to an unknown resolved model.");
            result.model_index = found->second;
        }
        if (!object.triangleComponentKeys.empty() && !object.triangleComponentKeys.at(index).empty())
            result.component_index = m_componentIndices.at(object.triangleComponentKeys[index]);
        if (!object.triangleCornerUvs.empty() && object.triangleCornerUvs.at(index)) {
            if (result.model_index == SIZE_MAX)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Authored imported UVs have no resolved model owner.");
            result.uv_known = 1;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis) {
                    const double value = object.triangleCornerUvs[index]->at(corner)[axis];
                    if (!std::isfinite(value))
                        throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Generated source UVs are nonfinite.");
                    result.uv[corner][axis] = value;
                }
        }
        result.color_encoding = result.model_index != SIZE_MAX && m_linearModels.count(m_models.at(result.model_index))
            ? DUST3D_SURFACE_COLOR_LINEAR
            : DUST3D_SURFACE_COLOR_SRGB;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto color = sourceCornerColor(object, index, corner);
            for (size_t channel = 0; channel < 4; ++channel) {
                const double value = color[channel];
                if (!std::isfinite(value) || value < -1e-12 || value > 1 + 1e-12)
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Generated source RGBA is outside its finite normalized range.");
                result.color[corner][channel] = std::clamp(value, 0.0, 1.0);
            }
        }
        m_triangles.push_back(result);
    }
    void ImportedSurfaceData::finish(const Dust3DMesh& geometry)
    {
        m_object = nullptr;
        m_modelIndices.clear();
        m_componentIndices.clear();
        m_linearModels.clear();
        RetainedPayloadBudget actual(m_maximumBytes);
        actual.tryAdd(1, sizeof(Dust3DImportedSurfaceMesh) + sizeof(ImportedSurfaceData));
        actual.accountObject(geometry);
        actual.accountDynamicFields(m_triangles, m_models, m_components);
        if (actual.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Retained imported surface ownership exceeds its configured limit.");
        m_payloadBytes = actual.usedBytes();
    }
}
}

Dust3DImportedSurfaceMesh* dust3d_imported_surface_mesh_load(const uint8_t* bytes, size_t size,
    const Dust3DImportedSurfaceOptions* options, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!options || options->struct_size != sizeof(*options))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Imported surfaces require correctly sized options.");
        if (options->maximum_payload_bytes > DUST3D_MAX_IMPORTED_SURFACE_PAYLOAD_BYTES)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Imported surface ownership budget exceeds 256 MiB.");
        const size_t maximum = options->maximum_payload_bytes ? options->maximum_payload_bytes : DUST3D_DEFAULT_IMPORTED_SURFACE_PAYLOAD_BYTES;
        std::lock_guard<std::mutex> lock(engineMutex());
        std::string warning;
        ImportedModels imports;
        auto snapshot = readDocument(bytes, size, warning, nullptr, &imports);
        auto owner = std::make_unique<Dust3DImportedSurfaceMesh>();
        owner->data = std::make_unique<ImportedSurfaceData>(*snapshot, imports, maximum);
        GenerationOutputs outputs;
        outputs.surfaces = owner->data.get();
        owner->geometry = generateMesh(std::move(snapshot), warning, &imports, nullptr, nullptr, outputs);
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown imported surface generation failure.");
    }
    return nullptr;
}
void dust3d_imported_surface_mesh_destroy(Dust3DImportedSurfaceMesh* mesh) { delete mesh; }
const Dust3DMesh* dust3d_imported_surface_mesh_geometry(const Dust3DImportedSurfaceMesh* mesh) { return mesh ? mesh->geometry.get() : nullptr; }
const Dust3DImportedSurfaceTriangle* dust3d_imported_surface_mesh_triangles(const Dust3DImportedSurfaceMesh* mesh)
{
    return mesh && !mesh->data->triangles().empty() ? mesh->data->triangles().data() : nullptr;
}
size_t dust3d_imported_surface_mesh_triangle_count(const Dust3DImportedSurfaceMesh* mesh) { return mesh ? mesh->data->triangles().size() : 0; }
size_t dust3d_imported_surface_mesh_model_count(const Dust3DImportedSurfaceMesh* mesh) { return mesh ? mesh->data->models().size() : 0; }
const char* dust3d_imported_surface_mesh_model_id(const Dust3DImportedSurfaceMesh* mesh, size_t index)
{
    return mesh && index < mesh->data->models().size() ? mesh->data->models()[index].c_str() : nullptr;
}
size_t dust3d_imported_surface_mesh_component_count(const Dust3DImportedSurfaceMesh* mesh) { return mesh ? mesh->data->components().size() : 0; }
const char* dust3d_imported_surface_mesh_component_id(const Dust3DImportedSurfaceMesh* mesh, size_t index)
{
    return mesh && index < mesh->data->components().size() ? mesh->data->components()[index].identity.c_str() : nullptr;
}
Dust3DSurfaceComponentKind dust3d_imported_surface_mesh_component_kind(const Dust3DImportedSurfaceMesh* mesh, size_t index)
{
    return mesh && index < mesh->data->components().size() ? mesh->data->components()[index].kind : DUST3D_SURFACE_COMPONENT_UNKNOWN;
}
size_t dust3d_imported_surface_mesh_payload_bytes(const Dust3DImportedSurfaceMesh* mesh) { return mesh ? mesh->data->payloadBytes() : 0; }

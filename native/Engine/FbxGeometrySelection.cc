// SPDX-License-Identifier: MIT
#include "FbxGeometrySelection.h"
#include "EngineInternal.h"
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>

namespace dust3d {
namespace native {
    FbxGeometrySelectionPlan planFbxGeometrySelection(size_t vertices, size_t sourceTriangles, size_t selectedTriangles, bool hasNormals, bool hasUvs, size_t maximumBytes)
    {
        const auto maximum = maximumBytes ? maximumBytes : 64 * 1024 * 1024;
        if (maximum > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX selection quota exceeds256MiB.");
        if (!vertices || !sourceTriangles || !selectedTriangles)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection requires nonempty geometry and faces.");
        if (vertices > 1000000 || sourceTriangles > 5000000 || selectedTriangles > sourceTriangles)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX selection exceeds geometry counts.");
        RetainedPayloadBudget budget(maximum);
        budget.tryAdd(sizeof(FbxGeometrySelection) + sizeof(Object));
        budget.tryAdd(vertices, sizeof(Vector3));
        budget.tryAdd(selectedTriangles, sizeof(std::vector<size_t>) + 4 * sizeof(size_t));
        // Object setters copy their inputs, so admit both temporary and retained corner arrays.
        if (hasNormals)
            budget.tryAdd(selectedTriangles, 2 * (sizeof(std::vector<Vector3>) + 3 * sizeof(Vector3)));
        if (hasUvs)
            budget.tryAdd(selectedTriangles, 2 * (sizeof(std::vector<Vector2>) + 3 * sizeof(Vector2)));
        if (budget.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX selection staging exceeds quota.");
        return { maximum, budget.usedBytes() };
    }
    namespace {
        template <typename Value>
        void validateValues(const Value& value, size_t dimensions, double maximum)
        {
            for (size_t axis = 0; axis < dimensions; ++axis) {
                if (!std::isfinite(value[axis]))
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection contains nonfinite coordinates.");
                if (std::abs(value[axis]) > maximum)
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX selection coordinate exceeds bounds.");
            }
        }
        template <typename Value>
        void validateStream(const std::vector<std::vector<Value>>* stream, size_t sourceCount, const std::vector<size_t>& indices, size_t dimensions)
        {
            if (!stream)
                return;
            if (stream->size() != sourceCount)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection corner stream face count differs.");
            for (const auto index : indices) {
                if ((*stream)[index].size() != 3)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection requires three corners.");
                for (const auto& value : (*stream)[index])
                    validateValues(value, dimensions, 1000000);
            }
        }
    }
    FbxGeometrySelection selectFbxGeometry(const Object& source, const std::vector<size_t>& indices, const std::vector<FbxTriangleUvs>* overrides, size_t maximumBytes)
    {
        const auto* normals = source.triangleVertexNormals();
        const auto* uvs = overrides ? nullptr : source.triangleVertexUvs();
        const auto plan = planFbxGeometrySelection(source.vertices.size(), source.triangles.size(), indices.size(), normals != nullptr, overrides || uvs, maximumBytes);
        for (size_t position = 0; position < indices.size(); ++position) {
            const auto index = indices[position];
            if (index >= source.triangles.size() || (position && index <= indices[position - 1]))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection indices must be ordered unique source faces.");
            const auto& triangle = source.triangles[index];
            if (triangle.size() != 3)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection requires triangles.");
            for (const auto vertex : triangle) {
                if (vertex >= source.vertices.size())
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection vertex index exceeds source.");
            }
        }
        for (const auto& vertex : source.vertices)
            validateValues(vertex, 3, 4000000);
        validateStream(normals, source.triangles.size(), indices, 3);
        validateStream(uvs, source.triangles.size(), indices, 2);
        if (overrides) {
            if (overrides->size() != indices.size())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX selection UV override count differs.");
            for (const auto& triangle : *overrides) {
                for (const auto& value : triangle)
                    validateValues(value, 2, 1000000);
            }
        }
        FbxGeometrySelection result;
        result.m_object = std::make_unique<Object>();
        result.m_object->vertices = source.vertices;
        result.m_sourceTriangles = indices;
        result.m_object->triangles.reserve(indices.size());
        std::vector<std::vector<Vector3>> selectedNormals;
        std::vector<std::vector<Vector2>> selectedUvs;
        if (normals)
            selectedNormals.reserve(indices.size());
        if (uvs || overrides)
            selectedUvs.reserve(indices.size());
        for (size_t position = 0; position < indices.size(); ++position) {
            const auto index = indices[position];
            result.m_object->triangles.push_back(source.triangles[index]);
            if (normals)
                selectedNormals.push_back((*normals)[index]);
            if (overrides)
                selectedUvs.emplace_back((*overrides)[position].begin(), (*overrides)[position].end());
            else if (uvs)
                selectedUvs.push_back((*uvs)[index]);
        }
        if (normals)
            result.m_object->setTriangleVertexNormals(selectedNormals);
        if (uvs || overrides)
            result.m_object->setTriangleVertexUvs(selectedUvs);
        RetainedPayloadBudget retained(plan.maximumPayloadBytes);
        retained.tryAdd(sizeof(FbxGeometrySelection) + sizeof(Object));
        retained.accountDynamicFields(result.m_sourceTriangles, result.m_object->vertices, result.m_object->triangles);
        if (normals)
            retained.accountDynamic(*result.m_object->triangleVertexNormals());
        if (uvs || overrides)
            retained.accountDynamic(*result.m_object->triangleVertexUvs());
        result.m_payloadBytes = retained.usedBytes();
        retained.accountDynamicFields(selectedNormals, selectedUvs);
        if (retained.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX selection container capacities exceed quota.");
        return result;
    }
}
}

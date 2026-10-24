// SPDX-License-Identifier: MIT
#include "FbxMeshScene.h"
#include "EngineInternal.h"
#include "FbxMeshScenePlan.h"
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
namespace dust3d {
namespace native {
    namespace {
        void invalid(const char* cause) { throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, cause); }
        void limit(const char* cause) { throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, cause); }
        template <typename... Nodes>
        std::vector<FbxNode> children(Nodes... nodes)
        {
            std::vector<FbxNode> result;
            result.reserve(sizeof...(nodes));
            (result.push_back(std::move(nodes)), ...);
            return result;
        }
        FbxNode text(const char* name, const char* value)
        {
            FbxPropertyBlock properties;
            properties.appendString(value);
            return FbxNode(name, std::move(properties));
        }
        FbxNode integer(const char* name, int32_t value)
        {
            FbxPropertyBlock properties;
            properties.appendInt32(value);
            return FbxNode(name, std::move(properties));
        }
        FbxNode doubles(const char* name, const std::vector<double>& values)
        {
            FbxPropertyBlock properties(256 * 1024 * 1024);
            properties.appendDoubleArray(values);
            return FbxNode(name, std::move(properties), {}, { 256 * 1024 * 1024 });
        }
        FbxNode parameter(const char* name, const char* type, const char* label, double value, bool isInteger = false)
        {
            FbxPropertyBlock properties;
            properties.appendString(name);
            properties.appendString(type);
            properties.appendString(label);
            properties.appendString("");
            if (isInteger)
                properties.appendInt32(static_cast<int32_t>(value));
            else
                properties.appendDouble(value);
            return FbxNode("P", std::move(properties));
        }
        FbxNode connection(int64_t source, int64_t destination)
        {
            FbxPropertyBlock properties;
            properties.appendString("OO");
            properties.appendInt64(source);
            properties.appendInt64(destination);
            return FbxNode("C", std::move(properties));
        }
        void finite(double value, double bound)
        {
            if (!std::isfinite(value))
                invalid("FBX mesh attributes must be finite.");
            if (std::abs(value) > bound)
                limit("FBX mesh attribute exceeds coordinate bounds.");
        }
        template <typename Vector>
        void validateCorners(const std::vector<std::vector<Vector>>* stream, size_t triangles, size_t width)
        {
            if (!stream)
                return;
            if (stream->size() != triangles)
                invalid("FBX corner stream must contain every triangle.");
            for (const auto& corners : *stream) {
                if (corners.size() != 3)
                    invalid("FBX corner stream must contain exactly three corners.");
                for (const auto& corner : corners)
                    for (size_t axis = 0; axis < width; ++axis)
                        finite(corner[axis], 1000000);
            }
        }
        void validateStagingCapacity(size_t positions, size_t indices, size_t corners = 0)
        {
            RetainedPayloadBudget actual(FbxMeshSceneGeometryPlan::maximumStagingBytes);
            actual.tryAdd(positions, sizeof(double));
            actual.tryAdd(indices, sizeof(int32_t));
            actual.tryAdd(corners, sizeof(double));
            if (actual.isExceeded())
                limit("FBX mesh scene actual staging capacity exceeds budget.");
        }
        FbxNode layerReference(const char* type)
        {
            return FbxNode("LayerElement", FbxPropertyBlock(), children(text("Type", type), integer("TypedIndex", 0)));
        }
    }
    OwnedFbxDocument encodeFbxMeshScene(const Object& source, FbxDocumentLimits limits)
    {
        constexpr size_t maximumBytes = 256 * 1024 * 1024;
        if ((limits.maximumPayloadBytes != 0 && limits.maximumPayloadBytes <= sizeof(OwnedFbxDocument))
            || limits.maximumPayloadBytes > maximumBytes || limits.maximumNodes == 0 || limits.maximumNodes > 65536)
            limit("FBX mesh scene has invalid document quotas.");
        const auto* normals = source.triangleVertexNormals();
        const auto* uvs = source.triangleVertexUvs();
        const auto plan = planFbxMeshSceneGeometry(source.vertices.size(), source.triangles.size(), normals != nullptr, uvs != nullptr);
        for (const auto& vertex : source.vertices)
            for (size_t axis = 0; axis < 3; ++axis)
                finite(vertex[axis], 4000000);
        for (const auto& triangle : source.triangles) {
            if (triangle.size() != 3)
                invalid("FBX mesh scene requires triangle topology.");
            for (size_t index : triangle)
                if (index >= source.vertices.size())
                    invalid("FBX mesh scene has an invalid triangle index.");
        }
        validateCorners(normals, source.triangles.size(), 3);
        validateCorners(uvs, source.triangles.size(), 2);
        std::vector<double> positions;
        positions.reserve(plan.positionValueCount);
        for (const auto& vertex : source.vertices)
            for (size_t axis = 0; axis < 3; ++axis)
                positions.push_back(vertex[axis]);
        std::vector<int32_t> indices;
        indices.reserve(plan.polygonIndexCount);
        for (const auto& triangle : source.triangles) {
            indices.push_back(static_cast<int32_t>(triangle[0]));
            indices.push_back(static_cast<int32_t>(triangle[1]));
            indices.push_back(-static_cast<int32_t>(triangle[2]) - 1);
        }
        validateStagingCapacity(positions.capacity(), indices.capacity());
        FbxPropertyBlock indexProperties(maximumBytes);
        indexProperties.appendInt32Array(indices);
        auto geometryChildren = children(doubles("Vertices", positions), FbxNode("PolygonVertexIndex", std::move(indexProperties), {}, { maximumBytes }), integer("GeometryVersion", 124));
        auto layerChildren = children(integer("Version", 100));
        if (normals) {
            std::vector<double> values;
            values.reserve(plan.normalValueCount);
            for (const auto& triangle : *normals)
                for (const auto& corner : triangle)
                    for (size_t axis = 0; axis < 3; ++axis)
                        values.push_back(corner[axis]);
            validateStagingCapacity(positions.capacity(), indices.capacity(), values.capacity());
            FbxPropertyBlock ordinal;
            ordinal.appendInt32(0);
            geometryChildren.emplace_back("LayerElementNormal", std::move(ordinal), children(integer("Version", 101), text("Name", ""), text("MappingInformationType", "ByPolygonVertex"), text("ReferenceInformationType", "Direct"), doubles("Normals", values)), FbxNodeLimits { maximumBytes });
            layerChildren.push_back(layerReference("LayerElementNormal"));
        }
        if (uvs) {
            std::vector<double> values;
            values.reserve(plan.textureCoordinateValueCount);
            for (const auto& triangle : *uvs)
                for (const auto& corner : triangle) {
                    values.push_back(corner.x());
                    values.push_back(1 - corner.y());
                }
            validateStagingCapacity(positions.capacity(), indices.capacity(), values.capacity());
            FbxPropertyBlock ordinal;
            ordinal.appendInt32(0);
            geometryChildren.emplace_back("LayerElementUV", std::move(ordinal), children(integer("Version", 101), text("Name", "default"), text("MappingInformationType", "ByPolygonVertex"), text("ReferenceInformationType", "Direct"), doubles("UV", values)), FbxNodeLimits { maximumBytes });
            layerChildren.push_back(layerReference("LayerElementUV"));
        }
        FbxPropertyBlock layerOrdinal;
        layerOrdinal.appendInt32(0);
        geometryChildren.emplace_back("Layer", std::move(layerOrdinal), std::move(layerChildren));
        FbxPropertyBlock geometryProperties;
        geometryProperties.appendInt64(612150000);
        geometryProperties.appendString(std::string("Dust3D\0\1Geometry", 16));
        geometryProperties.appendString("Mesh");
        FbxPropertyBlock modelProperties;
        modelProperties.appendInt64(612150001);
        modelProperties.appendString(std::string("Dust3D\0\1Model", 13));
        modelProperties.appendString("Mesh");
        auto objects = children(FbxNode("Geometry", std::move(geometryProperties), std::move(geometryChildren), { maximumBytes }), FbxNode("Model", std::move(modelProperties), children(integer("Version", 232), FbxNode("Properties70"), text("Culling", "CullingOff"))));
        auto settings = children(parameter("UpAxis", "int", "Integer", 1, true), parameter("UpAxisSign", "int", "Integer", 1, true), parameter("FrontAxis", "int", "Integer", 2, true), parameter("FrontAxisSign", "int", "Integer", 1, true), parameter("CoordAxis", "int", "Integer", 0, true), parameter("CoordAxisSign", "int", "Integer", 1, true), parameter("OriginalUpAxis", "int", "Integer", 1, true), parameter("OriginalUpAxisSign", "int", "Integer", 1, true), parameter("UnitScaleFactor", "double", "Number", 100), parameter("OriginalUnitScaleFactor", "double", "Number", 1));
        auto roots = children(FbxNode("FBXHeaderExtension", FbxPropertyBlock(), children(integer("FBXHeaderVersion", 1003), integer("FBXVersion", 7400), text("Creator", "Dust3D native"))), FbxNode("GlobalSettings", FbxPropertyBlock(), children(integer("Version", 1000), FbxNode("Properties70", FbxPropertyBlock(), std::move(settings)))), FbxNode("Objects", FbxPropertyBlock(), std::move(objects), { maximumBytes }), FbxNode("Connections", FbxPropertyBlock(), children(connection(612150001, 0), connection(612150000, 612150001))));
        return encodeFbxDocument(roots, limits);
    }
}
}

// SPDX-License-Identifier: MIT
#include "FbxMeshScene.h"
#include "EngineInternal.h"
#include "FbxMaterialImagePlan.h"
#include "FbxMeshScenePlan.h"
#include <algorithm>
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
        FbxNode text(const char* name, const std::string& value)
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
        FbxNode stringParameter(const char* name, const char* value)
        {
            FbxPropertyBlock properties;
            properties.appendString(name);
            properties.appendString("KString");
            properties.appendString("");
            properties.appendString("");
            properties.appendString(value);
            return FbxNode("P", std::move(properties));
        }
        FbxNode vectorParameter(const char* name, const std::array<double, 3>& value, const char* type = "Color")
        {
            FbxPropertyBlock properties;
            properties.appendString(name);
            properties.appendString(type);
            properties.appendString("");
            properties.appendString("A");
            for (double component : value)
                properties.appendDouble(component);
            return FbxNode("P", std::move(properties));
        }
        void validateMaterial(const FbxSceneMaterial& material, const Object& source)
        {
            const auto unit = [](double value) {
                if (!std::isfinite(value) || value < 0 || value > 1)
                    invalid("FBX material scalars must be finite and within zero to one.");
            };
            for (double value : material.baseColor)
                unit(value);
            unit(material.metallic);
            unit(material.roughness);
            std::array<size_t, 6> mapBytes {};
            for (size_t index = 0; index < mapBytes.size(); ++index)
                mapBytes[index] = material.pngMaps[index].size();
            const auto images = planFbxMaterialImages(mapBytes, source.triangleVertexUvs() != nullptr);
            if (images.imageCount == 0)
                return;
            const std::array<uint8_t, 8> signature { 137, 80, 78, 71, 13, 10, 26, 10 };
            for (const auto& map : material.pngMaps) {
                if (map.empty())
                    continue;
                // The pure length plan admits the signature span before these byte accesses.
                if (!std::equal(signature.begin(), signature.end(), map.begin()))
                    invalid("FBX embedded images require prevalidated encoded PNG bytes.");
            }
        }
        void appendMaterial(const FbxSceneMaterial& material, std::vector<FbxNode>& objects, std::vector<FbxNode>& connections)
        {
            const std::array<double, 3> color { material.baseColor[0], material.baseColor[1], material.baseColor[2] };
            auto properties = children(vectorParameter("DiffuseColor", color), parameter("DiffuseFactor", "Number", "", 1),
                parameter("Opacity", "Number", "", material.baseColor[3]), parameter("TransparencyFactor", "Number", "", 1 - material.baseColor[3]),
                vectorParameter("TransparentColor", { 1, 1, 1 }), vectorParameter("Maya|base_color", color, "Vector3D"),
                parameter("Maya|metallic", "double", "Number", material.metallic), parameter("Maya|roughness", "double", "Number", material.roughness),
                parameter("Metalness", "double", "Number", material.metallic), parameter("Roughness", "double", "Number", material.roughness));
            const double emission = material.pngMaps[4].empty() ? 0 : 1;
            properties.push_back(vectorParameter("EmissiveColor", { emission, emission, emission }));
            properties.push_back(parameter("EmissiveFactor", "Number", "", 1));
            properties.push_back(vectorParameter("Maya|emissive", { emission, emission, emission }, "Vector3D"));
            properties.push_back(parameter("Maya|emissive_intensity", "double", "Number", emission));
            constexpr std::array<const char*, 6> names { "color", "normal", "metallic", "roughness", "emissive", "ao" };
            constexpr std::array<const char*, 6> nativeProperties { "Maya|TEX_color_map", "Maya|TEX_normal_map", "Maya|TEX_metallic_map", "Maya|TEX_roughness_map", "Maya|TEX_emissive_map", "Maya|TEX_ao_map" };
            constexpr std::array<const char*, 6> standardProperties { "DiffuseColor", "NormalMap", "Metalness", "Roughness", "EmissiveColor", "AmbientColor" };
            for (size_t channel = 0; channel < material.pngMaps.size(); ++channel) {
                const auto enabledName = std::string("Maya|use_") + names[channel] + "_map";
                properties.push_back(parameter(enabledName.c_str(), "double", "Number", material.pngMaps[channel].empty() ? 0 : 1));
                properties.push_back(vectorParameter(nativeProperties[channel], { 0, 0, 0 }, "Vector3D"));
                if (material.pngMaps[channel].empty())
                    continue;
                const int64_t videoId = 612150003 + static_cast<int64_t>(channel) * 2;
                const int64_t textureId = videoId + 1;
                const auto filename = std::string("Dust3D_") + names[channel] + ".png";
                const auto videoName = std::string("Dust3D_") + names[channel] + std::string("\0\1Video", 7);
                const auto textureName = std::string("Dust3D_") + names[channel] + std::string("\0\1Texture", 9);
                FbxPropertyBlock videoProperties;
                videoProperties.appendInt64(videoId);
                videoProperties.appendString(videoName);
                videoProperties.appendString("Clip");
                FbxPropertyBlock content(256 * 1024 * 1024);
                content.appendRaw(material.pngMaps[channel]);
                objects.emplace_back("Video", std::move(videoProperties), children(text("Type", "Clip"), text("FileName", filename.c_str()), text("RelativeFilename", filename.c_str()), FbxNode("Content", std::move(content), {}, { 256 * 1024 * 1024 })), FbxNodeLimits { 256 * 1024 * 1024 });
                FbxPropertyBlock textureProperties;
                textureProperties.appendInt64(textureId);
                textureProperties.appendString(textureName);
                textureProperties.appendString("");
                objects.emplace_back("Texture", std::move(textureProperties), children(text("Type", "TextureVideoClip"), integer("Version", 202), text("TextureName", textureName), text("Media", videoName), text("FileName", filename.c_str()), text("RelativeFilename", filename.c_str()), FbxNode("Properties70", FbxPropertyBlock(), children(stringParameter("UVSet", "default"))), text("Texture_Alpha_Source", "None")));
                connections.push_back(connection(videoId, textureId));
                const auto connectProperty = [&](const char* name) {
                    FbxPropertyBlock link;
                    link.appendString("OP");
                    link.appendInt64(textureId);
                    link.appendInt64(612150002);
                    link.appendString(name);
                    connections.emplace_back("C", std::move(link));
                };
                connectProperty(nativeProperties[channel]);
                connectProperty(standardProperties[channel]);
            }
            FbxPropertyBlock materialProperties;
            materialProperties.appendInt64(612150002);
            materialProperties.appendString(std::string("Dust3D\0\1Material", 16));
            materialProperties.appendString("");
            objects.emplace_back("Material", std::move(materialProperties), children(integer("Version", 102), text("ShadingModel", "Phong"), integer("MultiLayer", 0), FbxNode("Properties70", FbxPropertyBlock(), std::move(properties))));
            connections.push_back(connection(612150002, 612150001));
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
    static OwnedFbxDocument encodeMeshScene(const Object& source, const FbxSceneMaterial* material, FbxDocumentLimits limits)
    {
        constexpr size_t maximumBytes = 256 * 1024 * 1024;
        if ((limits.maximumPayloadBytes != 0 && limits.maximumPayloadBytes <= sizeof(OwnedFbxDocument))
            || limits.maximumPayloadBytes > maximumBytes || limits.maximumNodes == 0 || limits.maximumNodes > 65536)
            limit("FBX mesh scene has invalid document quotas.");
        const auto* normals = source.triangleVertexNormals();
        const auto* uvs = source.triangleVertexUvs();
        const auto plan = planFbxMeshSceneGeometry(source.vertices.size(), source.triangles.size(), normals != nullptr, uvs != nullptr);
        if (material)
            validateMaterial(*material, source);
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
        if (material) {
            FbxPropertyBlock ordinal;
            ordinal.appendInt32(0);
            FbxPropertyBlock indices;
            indices.appendInt32Array({ 0 });
            geometryChildren.emplace_back("LayerElementMaterial", std::move(ordinal), children(integer("Version", 101), text("Name", ""), text("MappingInformationType", "AllSame"), text("ReferenceInformationType", "IndexToDirect"), FbxNode("Materials", std::move(indices))));
            layerChildren.push_back(layerReference("LayerElementMaterial"));
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
        auto connections = children(connection(612150001, 0), connection(612150000, 612150001));
        if (material)
            appendMaterial(*material, objects, connections);
        auto roots = children(FbxNode("FBXHeaderExtension", FbxPropertyBlock(), children(integer("FBXHeaderVersion", 1003), integer("FBXVersion", 7400), text("Creator", "Dust3D native"))), FbxNode("GlobalSettings", FbxPropertyBlock(), children(integer("Version", 1000), FbxNode("Properties70", FbxPropertyBlock(), std::move(settings)))), FbxNode("Objects", FbxPropertyBlock(), std::move(objects), { maximumBytes }), FbxNode("Connections", FbxPropertyBlock(), std::move(connections)));
        return encodeFbxDocument(roots, limits);
    }
    OwnedFbxDocument encodeFbxMeshScene(const Object& source, FbxDocumentLimits limits)
    {
        return encodeMeshScene(source, nullptr, limits);
    }
    OwnedFbxDocument encodeFbxTexturedMeshScene(const Object& source, const FbxSceneMaterial& material, FbxDocumentLimits limits)
    {
        return encodeMeshScene(source, &material, limits);
    }
}
}

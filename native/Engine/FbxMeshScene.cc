// SPDX-License-Identifier: MIT
#include "FbxMeshScene.h"
#include "EngineInternal.h"
#include "FbxAnimationMath.h"
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
            if (!material.pngMaps[1].empty()) {
                properties.push_back(vectorParameter("Bump", { 0, 0, 0 }, "Vector3D"));
                properties.push_back(parameter("BumpFactor", "double", "Number", 1));
            }
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
                if (channel == 1)
                    connectProperty("Bump");
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
    constexpr int64_t fbxSkinId = 612151000;
    constexpr int64_t fbxSkeletonAttributeBaseId = 612154000;
    static int64_t fbxLimbId(size_t index) { return fbxSkinId + 2 + 2 * static_cast<int64_t>(index); }

    static std::map<std::string, Matrix4x4> appendRestSkin(const RigStructure& rig, const std::vector<GlbSkinWeights>& bindings,
        size_t vertexCount, std::vector<FbxNode>& objects, std::vector<FbxNode>& connections)
    {
        constexpr int64_t geometryId = 612150000;
        constexpr int64_t modelId = 612150001;
        constexpr int64_t skinId = fbxSkinId;
        constexpr int64_t poseId = skinId + 1;
        if (bindings.size() != vertexCount || rig.bones.empty() || rig.bones.size() > 1024)
            invalid("FBX skin requires a bounded rig and one binding per control point.");
        std::vector<std::string> names;
        std::vector<std::string> parents;
        for (const auto& bone : rig.bones) {
            if (bone.name.size() > 1024 || bone.name.find('\0') != std::string::npos || bone.parent.size() > 1024)
                invalid("FBX skin joint names exceed their admitted bounds.");
            names.push_back(bone.name);
            parents.push_back(bone.parent);
        }
        std::map<std::string, uint16_t> indices;
        try {
            indices = glbJointIndices(names);
            glbJointParents(parents, indices);
        } catch (const std::invalid_argument&) {
            invalid("FBX skin requires valid unique joint names and an acyclic hierarchy.");
        }
        RigGenerator generator;
        std::map<std::string, Matrix4x4> world;
        if (!generator.computeBoneWorldTransforms(rig, world))
            invalid("FBX could not compute rest transforms.");

        struct Cluster {
            std::vector<int32_t> controlPoints;
            std::vector<double> weights;
        };
        std::vector<Cluster> clusters(rig.bones.size());
        for (size_t vertex = 0; vertex < bindings.size(); ++vertex) {
            const auto& binding = bindings[vertex];
            double totalWeight = 0;
            for (size_t slot = 0; slot < binding.weights.size(); ++slot) {
                const double weight = binding.weights[slot];
                if (weight == 0)
                    continue;
                const auto joint = binding.joints[slot];
                if (joint >= clusters.size() || !std::isfinite(weight) || weight < 0 || weight > 1)
                    invalid("FBX skin contains an invalid joint influence.");
                clusters[joint].controlPoints.push_back(static_cast<int32_t>(vertex));
                clusters[joint].weights.push_back(weight);
                totalWeight += weight;
            }
            if (std::abs(totalWeight - 1.0) > 1e-4)
                invalid("FBX skin requires normalized control-point influences.");
        }
        const auto objectProperties = [](int64_t id, const std::string& name, const char* type) {
            FbxPropertyBlock block;
            block.appendInt64(id);
            block.appendString(name);
            block.appendString(type);
            return block;
        };
        const auto matrixValues = [](const Matrix4x4& matrix) {
            const auto* values = matrix.constData();
            return std::vector<double>(values, values + 16);
        };
        const auto poseNode = [&](int64_t id, const Matrix4x4& matrix) {
            FbxPropertyBlock identifier;
            identifier.appendInt64(id);
            return FbxNode("PoseNode", FbxPropertyBlock(), children(FbxNode("Node", std::move(identifier)), doubles("Matrix", matrixValues(matrix))));
        };
        Matrix4x4 identity;
        auto pose = children(text("Type", "BindPose"), integer("Version", 100),
            integer("NbPoseNodes", static_cast<int32_t>(rig.bones.size() + 1)), poseNode(modelId, identity));
        objects.emplace_back("Deformer", objectProperties(skinId, std::string("Dust3D\0\1Deformer", 16), "Skin"),
            children(integer("Version", 101)));
        connections.push_back(connection(skinId, geometryId));
        for (size_t index = 0; index < rig.bones.size(); ++index) {
            const auto& bone = rig.bones[index];
            const auto& bind = world.at(bone.name);
            const auto inverse = bind.inverted();
            auto local = bind;
            int64_t parent = 0;
            if (!bone.parent.empty()) {
                local = world.at(bone.parent).inverted();
                local *= bind;
                parent = fbxLimbId(indices.at(bone.parent));
            }
            RigMatrix localMatrix;
            std::copy_n(local.constData(), localMatrix.size(), localMatrix.begin());
            const auto angles = fbxEulerAngles(localMatrix);
            auto properties = children(
                vectorParameter("Lcl Translation", { localMatrix[12], localMatrix[13], localMatrix[14] }, "Lcl Translation"),
                vectorParameter("Lcl Rotation", { angles.pitch, angles.yaw, angles.roll }, "Lcl Rotation"),
                vectorParameter("Lcl Scaling", { 1, 1, 1 }, "Lcl Scaling"));
            const auto limb = fbxLimbId(index);
            const auto cluster = limb + 1;
            objects.emplace_back("Model", objectProperties(limb, bone.name + std::string("\0\1Model", 7), "LimbNode"),
                children(integer("Version", 232), FbxNode("Properties70", FbxPropertyBlock(), std::move(properties))));
            const auto attribute = fbxSkeletonAttributeBaseId + static_cast<int64_t>(index);
            objects.emplace_back("NodeAttribute",
                objectProperties(attribute, bone.name + std::string("\0\1NodeAttribute", 15), "LimbNode"),
                children(text("TypeFlags", "Skeleton")));
            connections.push_back(connection(attribute, limb));
            FbxPropertyBlock points(256 * 1024 * 1024);
            points.appendInt32Array(clusters[index].controlPoints);
            objects.emplace_back("Deformer",
                objectProperties(cluster, bone.name + std::string("\0\1SubDeformer", 13), "Cluster"),
                children(integer("Version", 100), FbxNode("Indexes", std::move(points), {}, { 256 * 1024 * 1024 }),
                    doubles("Weights", clusters[index].weights), doubles("Transform", matrixValues(inverse)),
                    doubles("TransformLink", matrixValues(bind))),
                FbxNodeLimits { 256 * 1024 * 1024 });
            connections.push_back(connection(limb, parent));
            connections.push_back(connection(limb, cluster));
            connections.push_back(connection(cluster, skinId));
            pose.push_back(poseNode(limb, bind));
        }
        objects.emplace_back("Pose", objectProperties(poseId, std::string("Dust3D\0\1Pose", 12), "BindPose"), std::move(pose));
        return world;
    }

    static void appendAnimationClips(const RigStructure& rig, const std::vector<RigAnimationClip>& clips,
        const std::map<std::string, Matrix4x4>& bindWorld,
        std::vector<FbxNode>& objects, std::vector<FbxNode>& connections)
    {
        int64_t nextId = 612160000;
        size_t remainingJointKeys = 1000000;
        if (clips.size() > 128)
            limit("FBX animation clip count exceeds its bound.");
        const auto namedObject = [](int64_t id, const std::string& name, const char* type) {
            FbxPropertyBlock properties;
            properties.appendInt64(id);
            properties.appendString(name + std::string("\0\1", 2) + type);
            properties.appendString("");
            return properties;
        };
        const auto propertyLink = [](int64_t source, int64_t target, const char* property) {
            FbxPropertyBlock properties;
            properties.appendString("OP");
            properties.appendInt64(source);
            properties.appendInt64(target);
            properties.appendString(property);
            return FbxNode("C", std::move(properties));
        };
        const auto arrayNode = [](const char* name, const auto& values, auto append) {
            FbxPropertyBlock properties;
            (properties.*append)(values);
            return FbxNode(name, std::move(properties));
        };
        const auto durationProperty = [](const char* name, int64_t ticks) {
            FbxPropertyBlock properties;
            properties.appendString(name);
            properties.appendString("KTime");
            properties.appendString("Time");
            properties.appendString("");
            properties.appendInt64(ticks);
            return FbxNode("P", std::move(properties));
        };
        constexpr std::array<const char*, 3> axes { "d|X", "d|Y", "d|Z" };
        for (const auto& clip : clips) {
            if (clip.name.empty() || clip.name.size() > 1024 || clip.name.find('\0') != std::string::npos || clip.frames.empty())
                invalid("FBX animation requires a clip name and at least one key.");
            if (clip.frames.size() > 5000 || clip.frames.size() > remainingJointKeys / rig.bones.size())
                limit("FBX animation key count exceeds its bound.");
            remainingJointKeys -= clip.frames.size() * rig.bones.size();
            const auto duration = fbxAnimationTime(clip.durationSeconds);
            std::vector<int64_t> times;
            times.reserve(clip.frames.size());
            for (const auto& frame : clip.frames) {
                const auto time = fbxAnimationTime(frame.time);
                if (time > duration || (!times.empty() && time <= times.back()))
                    invalid("FBX animation keys must be strictly increasing within the clip duration.");
                times.push_back(time);
            }
            const auto stack = nextId++;
            const auto layer = nextId++;
            objects.emplace_back("AnimationStack", namedObject(stack, clip.name, "AnimStack"),
                children(FbxNode("Properties70", FbxPropertyBlock(), children(durationProperty("LocalStart", 0), durationProperty("LocalStop", duration), durationProperty("ReferenceStart", 0), durationProperty("ReferenceStop", duration)))));
            objects.emplace_back("AnimationLayer", namedObject(layer, "BaseLayer", "AnimLayer"));
            connections.push_back(connection(layer, stack));

            for (size_t joint = 0; joint < rig.bones.size(); ++joint) {
                const auto& bone = rig.bones[joint];
                auto bindLocal = bindWorld.at(bone.name);
                if (!bone.parent.empty()) {
                    bindLocal = bindWorld.at(bone.parent).inverted();
                    bindLocal *= bindWorld.at(bone.name);
                }
                RigMatrix bindMatrix;
                std::copy_n(bindLocal.constData(), bindMatrix.size(), bindMatrix.begin());
                const auto bindEuler = fbxEulerAngles(bindMatrix);
                const std::array<double, 3> bindAngles { bindEuler.pitch, bindEuler.yaw, bindEuler.roll };
                constexpr double bindTolerance = 1e-6;
                bool hasTranslation = false;
                bool hasRotation = false;
                std::array<std::vector<float>, 3> translation, rotation;
                FbxEulerAngles previous;
                bool first = true;
                for (const auto& frame : clip.frames) {
                    const auto world = frame.boneWorldTransforms.find(bone.name);
                    if (world == frame.boneWorldTransforms.end())
                        invalid("FBX animation key omits a rig joint.");
                    auto local = world->second;
                    if (!bone.parent.empty()) {
                        const auto parent = frame.boneWorldTransforms.find(bone.parent);
                        if (parent == frame.boneWorldTransforms.end())
                            invalid("FBX animation key omits a rig joint parent.");
                        local = parent->second.inverted();
                        local *= world->second;
                    }
                    RigMatrix matrix;
                    std::copy_n(local.constData(), matrix.size(), matrix.begin());
                    const auto euler = fbxEulerAngles(matrix);
                    const std::array<double, 3> keyAngles { euler.pitch, euler.yaw, euler.roll };
                    previous = first ? euler : continuousFbxEulerAngles(euler, previous);
                    first = false;
                    const std::array<double, 3> angles { previous.pitch, previous.yaw, previous.roll };
                    for (size_t axis = 0; axis < axes.size(); ++axis) {
                        hasTranslation |= std::abs(matrix[12 + axis] - bindMatrix[12 + axis]) > bindTolerance;
                        hasRotation |= std::abs(std::remainder(keyAngles[axis] - bindAngles[axis], 360.0)) > bindTolerance;
                        translation[axis].push_back(static_cast<float>(matrix[12 + axis]));
                        rotation[axis].push_back(static_cast<float>(angles[axis]));
                    }
                }
                const auto appendChannels = [&](const auto& values, const char* name, const char* property) {
                    const auto node = nextId++;
                    std::vector<FbxNode> defaults;
                    for (size_t axis = 0; axis < axes.size(); ++axis) {
                        FbxPropertyBlock properties;
                        properties.appendString(axes[axis]);
                        properties.appendString("Number");
                        properties.appendString("");
                        properties.appendString("A");
                        properties.appendDouble(values[axis].front());
                        defaults.emplace_back("P", std::move(properties));
                    }
                    objects.emplace_back("AnimationCurveNode", namedObject(node, name, "AnimCurveNode"),
                        children(FbxNode("Properties70", FbxPropertyBlock(), std::move(defaults))));
                    connections.push_back(connection(node, layer));
                    connections.push_back(propertyLink(node, fbxLimbId(joint), property));
                    for (size_t axis = 0; axis < axes.size(); ++axis) {
                        const auto curve = nextId++;
                        FbxPropertyBlock defaultValue;
                        defaultValue.appendDouble(0.0);
                        objects.emplace_back("AnimationCurve", namedObject(curve, axes[axis], "AnimCurve"),
                            children(FbxNode("Default", std::move(defaultValue)), integer("KeyVer", 4008),
                                arrayNode("KeyTime", times, &FbxPropertyBlock::appendInt64Array),
                                arrayNode("KeyValueFloat", values[axis], &FbxPropertyBlock::appendFloatArray),
                                arrayNode("KeyAttrFlags", std::vector<int32_t> { 24836 }, &FbxPropertyBlock::appendInt32Array),
                                arrayNode("KeyAttrDataFloat", std::vector<float>(4, 0), &FbxPropertyBlock::appendFloatArray),
                                arrayNode("KeyAttrRefCount", std::vector<int32_t> { static_cast<int32_t>(times.size()) }, &FbxPropertyBlock::appendInt32Array)));
                        connections.push_back(propertyLink(curve, node, axes[axis]));
                    }
                };
                if (hasTranslation)
                    appendChannels(translation, "T", "Lcl Translation");
                if (hasRotation)
                    appendChannels(rotation, "R", "Lcl Rotation");
            }
        }
    }

    static OwnedFbxDocument encodeMeshScene(const Object& source, const FbxSceneMaterial* material, FbxDocumentLimits limits,
        const RigStructure* rig = nullptr, const std::vector<GlbSkinWeights>* bindings = nullptr,
        const std::vector<RigAnimationClip>* clips = nullptr)
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
        auto settings = children(parameter("UpAxis", "int", "Integer", 1, true), parameter("UpAxisSign", "int", "Integer", 1, true), parameter("FrontAxis", "int", "Integer", 2, true), parameter("FrontAxisSign", "int", "Integer", 1, true), parameter("CoordAxis", "int", "Integer", 0, true), parameter("CoordAxisSign", "int", "Integer", 1, true), parameter("OriginalUpAxis", "int", "Integer", -1, true), parameter("OriginalUpAxisSign", "int", "Integer", 1, true), parameter("UnitScaleFactor", "double", "Number", 100), parameter("OriginalUnitScaleFactor", "double", "Number", 1));
        if (clips && !clips->empty()) {
            settings.push_back(parameter("TimeMode", "enum", "", 11, true)); // FBX 24 fps mode.
            settings.push_back(parameter("CustomFrameRate", "double", "Number", 24.0));
        }
        auto connections = children(connection(612150001, 0), connection(612150000, 612150001));
        if (material)
            appendMaterial(*material, objects, connections);
        if (rig) {
            const auto bindWorld = appendRestSkin(*rig, *bindings, source.vertices.size(), objects, connections);
            if (clips && !clips->empty())
                appendAnimationClips(*rig, *clips, bindWorld, objects, connections);
        }
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
    OwnedFbxDocument encodeFbxRestMeshScene(const Object& source, const FbxSceneMaterial* material,
        const RigStructure& rig, const std::vector<GlbSkinWeights>& bindings, FbxDocumentLimits limits,
        const std::vector<RigAnimationClip>& clips)
    {
        return encodeMeshScene(source, material, limits, &rig, &bindings, &clips);
    }

}
}

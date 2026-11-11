// SPDX-License-Identifier: MIT
// Derived from application/sources/rig_skeleton_mesh_generator.cc at b475c730,
// distributed under the repository's MIT license. Keep reference geometry/winding.
#include "RigTemplateMesh.h"
#include <algorithm>
#include <cmath>
#include <dust3d/mesh/rope_mesh.h>
#include <limits>
#include <set>
namespace dust3d {
namespace native {
    namespace {
        constexpr size_t ringVertices = 32 * 3 * 3;
        constexpr size_t ringTriangles = 32 * 3 * 3 * 2;
        constexpr double pi = 3.141592653589793238462643383279502884;
        void fail(RigTemplateMeshFailure failure, const char* message)
        {
            throw RigTemplateMeshError(failure, message);
        }
        void validateName(const std::string& name, bool allowEmpty)
        {
            if (name.size() > 1024)
                fail(RigTemplateMeshFailure::limitExceeded, "Rig template name exceeds 1024 bytes.");
            if ((!allowEmpty && name.empty()) || std::any_of(name.begin(), name.end(), [](unsigned char value) { return value < 32 || value == 127; }))
                fail(RigTemplateMeshFailure::invalidInput, "Rig template name is empty or contains controls.");
        }
        void validatePosition(const Vector3& position)
        {
            for (size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(position[axis]))
                    fail(RigTemplateMeshFailure::invalidInput, "Rig template position is nonfinite.");
                if (std::abs(position[axis]) > 1000000)
                    fail(RigTemplateMeshFailure::limitExceeded, "Rig template position exceeds coordinate bounds.");
            }
        }
        Vector3 startDirection(const Vector3& direction)
        {
            const std::array<Vector3, 3> axes { Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1) };
            double maximumDot = -1;
            size_t nearest = 0;
            bool reversed = false;
            for (size_t index = 0; index < axes.size(); ++index) {
                const auto dot = Vector3::dotProduct(axes[index], direction);
                if (std::abs(dot) >= maximumDot) {
                    reversed = dot < 0;
                    maximumDot = std::abs(dot);
                    nearest = index;
                }
            }
            const auto value = Vector3::crossProduct(direction, axes[(nearest + 1) % 3]).normalized();
            return reversed ? -value : value;
        }
        void normalize(RigTemplateMesh& mesh)
        {
            if (mesh.vertices.empty())
                return;
            auto minimum = mesh.vertices.front().position;
            auto maximum = minimum;
            for (const auto& vertex : mesh.vertices) {
                for (size_t axis = 0; axis < 3; ++axis) {
                    minimum[axis] = std::min(minimum[axis], vertex.position[axis]);
                    maximum[axis] = std::max(maximum[axis], vertex.position[axis]);
                }
            }
            const auto size = maximum - minimum;
            const auto maximumSize = std::max({ size.x(), size.y(), size.z() });
            if (maximumSize > 0.001) {
                const auto center = (minimum + maximum) * 0.5;
                const auto scale = 1.5 / maximumSize;
                for (auto& vertex : mesh.vertices)
                    vertex.position = (vertex.position - center) * scale;
            }
        }
    }
    RigTemplateMesh buildRigTemplateMesh(const std::vector<RigTemplateBone>& bones, const RigTemplateMeshOptions& options)
    {
        if (!options.maximumBones || options.maximumBones > 256 || !options.maximumVertices || !options.maximumTriangles || !options.maximumOutputBytes)
            fail(RigTemplateMeshFailure::invalidInput, "Rig template output limits are invalid.");
        if (bones.size() > options.maximumBones)
            fail(RigTemplateMeshFailure::limitExceeded, "Rig template bone count exceeds its limit.");
        if (!std::isfinite(options.startRadius) || options.startRadius <= 0)
            fail(RigTemplateMeshFailure::invalidInput, "Rig template start radius must be finite and positive.");
        if (options.startRadius > 1000000)
            fail(RigTemplateMeshFailure::limitExceeded, "Rig template start radius exceeds coordinate bounds.");
        validateName(options.selectedBoneName, true);
        std::set<std::string> names;
        size_t vertexCount = 0, triangleCount = 0;
        for (const auto& bone : bones) {
            validateName(bone.name, false);
            if (!names.insert(bone.name).second)
                fail(RigTemplateMeshFailure::invalidInput, "Rig template bone names must be unique.");
            validatePosition(bone.fromPosition);
            validatePosition(bone.toPosition);
            if (!std::isfinite(bone.capsuleRadius) || bone.capsuleRadius < 0 || (bone.toPosition - bone.fromPosition).length() == 0)
                fail(RigTemplateMeshFailure::invalidInput, "Rig template bone radius or length is invalid.");
            if (bone.capsuleRadius > 1000000)
                fail(RigTemplateMeshFailure::limitExceeded, "Rig template capsule radius exceeds coordinate bounds.");
            const bool ring = (bone.toPosition - bone.fromPosition).length() >= 1e-6;
            // At most256 bounded records; these fixed-count sums cannot overflow size_t.
            vertexCount += 8 + (ring ? ringVertices : 0);
            triangleCount += 12 + (ring ? ringTriangles : 0);
        }
        const auto bytes = vertexCount * sizeof(RigTemplateVertex) + triangleCount * sizeof(std::array<uint32_t, 3>);
        if (vertexCount > options.maximumVertices || triangleCount > options.maximumTriangles || bytes > options.maximumOutputBytes)
            fail(RigTemplateMeshFailure::limitExceeded, "Rig template geometry exceeds output limits.");
        RigTemplateMesh mesh;
        mesh.vertices.reserve(vertexCount);
        mesh.triangles.reserve(triangleCount);
        std::vector<std::array<uint32_t, 4>> quads;
        quads.reserve(bones.size() * 6);
        for (size_t index = 0; index < bones.size(); ++index) {
            const auto& bone = bones[index];
            const auto boneIndex = static_cast<uint32_t>(index);
            const auto role = bone.name == options.selectedBoneName ? RigTemplateVertexRole::selectedBone : RigTemplateVertexRole::bone;
            const auto normal = (bone.toPosition - bone.fromPosition).normalized();
            const auto direction = startDirection(-normal);
            const auto up = Vector3::crossProduct(direction, -normal);
            auto face = [&](const Vector3& origin, double radius) {
                const auto start = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back({ origin + direction * radius, boneIndex, role });
                mesh.vertices.push_back({ origin - up * radius, boneIndex, role });
                mesh.vertices.push_back({ origin - direction * radius, boneIndex, role });
                mesh.vertices.push_back({ origin + up * radius, boneIndex, role });
                return std::array<uint32_t, 4> { start, start + 1, start + 2, start + 3 };
            };
            const auto from = face(bone.fromPosition, options.startRadius);
            auto to = face(bone.toPosition, options.startRadius / 5.0);
            quads.push_back(from);
            for (size_t i = 0; i < 4; ++i) {
                const auto j = (i + 1) % 4;
                quads.push_back({ from[j], from[i], to[i], to[j] });
            }
            std::reverse(to.begin(), to.end());
            quads.push_back(to);
            const auto axisValue = bone.toPosition - bone.fromPosition;
            const auto length = axisValue.length();
            if (length < 1e-6)
                continue;
            const auto axis = axisValue / length;
            const auto arbitrary = std::abs(axis.x()) < 0.9 ? Vector3(1, 0, 0) : Vector3(0, 1, 0);
            const auto u = Vector3::crossProduct(axis, arbitrary).normalized();
            const auto v = Vector3::crossProduct(axis, u).normalized();
            const auto center = (bone.fromPosition + bone.toPosition) * 0.5;
            const auto radius = std::max(0.01, bone.capsuleRadius * 1.25);
            std::vector<Vector3> positions;
            positions.reserve(32);
            for (size_t i = 0; i < 32; ++i) {
                const auto theta = 2.0 * pi * static_cast<double>(i) / 32.0;
                positions.push_back(center + u * (radius * std::cos(theta)) + v * (radius * std::sin(theta)));
            }
            RopeMesh::BuildParameters parameters;
            parameters.defaultRadius = 0.001;
            parameters.sectionSegments = 3;
            RopeMesh rope(parameters);
            rope.addRope(positions, true);
            if (rope.resultVertices().size() != ringVertices || rope.resultTriangles().size() != ringTriangles)
                fail(RigTemplateMeshFailure::generationFailed, "Unexpected template ring topology.");
            const auto start = static_cast<uint32_t>(mesh.vertices.size());
            for (const auto& position : rope.resultVertices())
                mesh.vertices.push_back({ position, boneIndex, RigTemplateVertexRole::ring });
            for (const auto& triangle : rope.resultTriangles()) {
                if (triangle.size() != 3 || std::any_of(triangle.begin(), triangle.end(), [](size_t value) { return value >= ringVertices; }))
                    fail(RigTemplateMeshFailure::generationFailed, "Invalid template ring indices.");
                mesh.triangles.push_back({ start + static_cast<uint32_t>(triangle[0]), start + static_cast<uint32_t>(triangle[1]), start + static_cast<uint32_t>(triangle[2]) });
            }
        }
        if (options.normalize)
            normalize(mesh);
        for (const auto& quad : quads) {
            mesh.triangles.push_back({ quad[0], quad[1], quad[2] });
            mesh.triangles.push_back({ quad[2], quad[3], quad[0] });
        }
        if (mesh.vertices.size() != vertexCount || mesh.triangles.size() != triangleCount)
            fail(RigTemplateMeshFailure::generationFailed, "Unexpected template output topology.");
        for (const auto& vertex : mesh.vertices) {
            for (size_t axis = 0; axis < 3; ++axis)
                if (!std::isfinite(vertex.position[axis]))
                    fail(RigTemplateMeshFailure::generationFailed, "Nonfinite template output vertex.");
        }
        return mesh;
    }
}
}

// SPDX-License-Identifier: MIT
#include "WorkerMeshCopyInspection.h"
#include "WorkerMeshValidation.h"
namespace dust3d {
namespace worker {
    namespace detail {
        namespace {
            void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
            {
                if (total > maximum || count > (maximum - total) / unit)
                    throw MeshPayloadError(MeshPayloadErrorCode::limitExceeded, "Native worker geometry exceeds its copy/wire quota.");
                total += count * unit;
            }
            MeshVertex readVertex(const Dust3DVertex& value)
            {
                return { { value.position[0], value.position[1], value.position[2] }, { value.normal[0], value.normal[1], value.normal[2] }, { value.color[0], value.color[1], value.color[2], value.color[3] } };
            }
        }
        NativeMeshCopyInspection inspectNativeMeshCopy(const Dust3DMesh* owner, const MeshPayloadLimits& limits)
        {
            validateMeshPayloadLimits(limits);
            const auto count = dust3d_mesh_vertex_count(owner);
            const auto* vertices = dust3d_mesh_vertices(owner);
            if (!vertices || !count || count % 3)
                throw MeshPayloadError(MeshPayloadErrorCode::invalidGeometry, "Native worker returned invalid expanded geometry.");
            if (count > limits.maximumVertices || dust3d_mesh_node_count(owner) > limits.maximumNodeCount || dust3d_mesh_part_count(owner) > limits.maximumPartCount)
                throw MeshPayloadError(MeshPayloadErrorCode::limitExceeded, "Native worker geometry counts exceed configured limits.");
            const auto* warning = dust3d_mesh_warning(owner);
            if (!warning)
                throw MeshPayloadError(MeshPayloadErrorCode::invalidText, "Native worker warning storage is missing.");
            size_t text = 0;
            while (text <= limits.maximumWarningBytes && warning[text])
                ++text;
            if (text > limits.maximumWarningBytes)
                throw MeshPayloadError(MeshPayloadErrorCode::limitExceeded, "Native worker warning exceeds configured limits.");
            uint64_t wire = meshPrefixBytes, logical = 0;
            charge(wire, count, meshVertexBytes, limits.maximumWireBytes);
            charge(wire, text, 1, limits.maximumWireBytes);
            charge(logical, count, sizeof(MeshVertex), limits.maximumDecodedBytes);
            charge(logical, text, 1, limits.maximumDecodedBytes);
            validateMeshWarning(reinterpret_cast<const uint8_t*>(warning), text);
            for (size_t index = 0; index < count; ++index)
                validateMeshVertex(readVertex(vertices[index]));
            for (size_t index = 0; index < count; index += 3)
                validateMeshTriangle({ readVertex(vertices[index]), readVertex(vertices[index + 1]), readVertex(vertices[index + 2]) });
            return { count, wire, logical, text };
        }
    }
}
}

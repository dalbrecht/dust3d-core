// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_MESH_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_MESH_VALIDATION_H_
#include "WorkerMesh.h"
namespace dust3d {
namespace worker {
    namespace detail {
        /// Validate one borrowed vertex's finite position, unit normal and finite unit colors.
        /// Throws original MeshPayloadError(invalidGeometry); no output allocation on success.
        /// Example: validateMeshVertex(value);
        void validateMeshVertex(const MeshVertex& value);
        /// Validate three borrowed vertices and positive finite triangle area.
        /// Throws original MeshPayloadError; no output allocation on success.
        /// Example: validateMeshTriangle(triangle);
        void validateMeshTriangle(const std::array<MeshVertex, 3>& value);
        /// Validate a readable warning span as supported UTF8/control characters.
        /// NULL is allowed only for zero size; throws original MeshPayloadError.
        /// Caller separately checks text/aggregate quotas. Example: validateMeshWarning(bytes, size).
        void validateMeshWarning(const uint8_t* bytes, size_t size);
        /// Read exactly 40 already bounds-checked wire bytes; reject nonfinite bits before float use.
        /// Caller guarantees a readable span; throws MeshPayloadError on invalid bits.
        /// Example: auto vertex = readMeshVertex(validatedSpan);
        MeshVertex readMeshVertex(const uint8_t* bytes);
        /// Write exactly 40 already bounds-checked writable wire bytes, preserving original float bits.
        /// Caller validates geometry and readable/writable storage first. No allocation/exception.
        /// Example: writeMeshVertex(validatedOutput, vertex);
        void writeMeshVertex(uint8_t* bytes, const MeshVertex& value);
    }
}
}
#endif

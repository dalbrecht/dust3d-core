#ifndef DUST3D_NATIVE_COMPONENT_PREVIEW_OUTPUT_H_
#define DUST3D_NATIVE_COMPONENT_PREVIEW_OUTPUT_H_

#include "EngineInternal.h"
#include <dust3d/mesh/mesh_generator.h>

struct Dust3DComponentPreview {
    std::vector<Dust3DComponentPreviewVertex> vertices;
    std::vector<Dust3DComponentPreviewTriangle> triangles;
    std::vector<Dust3DComponentPreviewPoint> cutFace;
    std::string colorImageId;
    std::string warning;
    size_t retainedPayloadBytes = 0;
};

namespace dust3d::native {
Uuid resolveComponentPreviewId(const Snapshot& snapshot, std::string_view authoredId);
size_t componentPreviewOutputLimit(size_t requested);
std::unique_ptr<Dust3DComponentPreview> copyComponentPreview(const MeshGenerator::ComponentPreview& source,
    std::string_view colorImageId, std::string_view warning, size_t maximumBytes,
    const Dust3DCancellationToken* cancellation = nullptr);
}

#endif

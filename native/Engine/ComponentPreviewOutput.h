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

struct Dust3DComponentPreviewBatch {
    std::vector<std::unique_ptr<Dust3DComponentPreview>> previews;
};

namespace dust3d::native {
struct ComponentPreviewRequest {
    Uuid id;
    std::string colorImageId;
};
void validateComponentPreviewHierarchy(const Snapshot& snapshot);
Uuid resolveComponentPreviewId(const Snapshot& snapshot, std::string_view authoredId);
size_t componentPreviewOutputLimit(size_t requested);
std::unique_ptr<Dust3DComponentPreview> copyComponentPreview(const MeshGenerator::ComponentPreview& source,
    std::string_view colorImageId, std::string_view warning, size_t maximumBytes,
    const Dust3DCancellationToken* cancellation = nullptr);
std::unique_ptr<Dust3DComponentPreviewBatch> copyComponentPreviewBatch(
    const std::map<Uuid, MeshGenerator::ComponentPreview>& source,
    const std::vector<ComponentPreviewRequest>& requests, std::string_view warning, size_t maximumBytes,
    const Dust3DCancellationToken* cancellation = nullptr);
}

#endif

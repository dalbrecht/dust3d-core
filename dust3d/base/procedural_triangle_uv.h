// SPDX-License-Identifier: MIT
#ifndef DUST3D_BASE_PROCEDURAL_TRIANGLE_UV_H_
#define DUST3D_BASE_PROCEDURAL_TRIANGLE_UV_H_

#include <array>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/uuid.h>
#include <dust3d/base/vector2.h>
#include <optional>

namespace dust3d {
/// A generated seam chart with topology-derived endpoint owners, not one surface owner.
struct ProceduralUvGradient {
    Uuid chartId;
    Uuid largeComponentId;
    Uuid smallComponentId;
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        budget.accountDynamicFields(chartId, largeComponentId, smallComponentId);
    }
};

/// Local chart corners and their owning component, aligned to one actual triangle.
/// This record is separate from imported authored UVs. Copies retain corner order;
/// a missing optional record denotes a generated or otherwise unmapped surface.
struct ProceduralTriangleUv {
    Uuid componentId;
    std::array<Vector2, 3> uv;
    std::optional<ProceduralUvGradient> gradient;
    /// Include the component UUID's owned text when this record is cached.
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        budget.accountDynamicFields(componentId, gradient);
    }
};
}
#endif

#include "EngineInternal.h"
#include <dust3d/base/retained_payload_budget.h>

void Dust3DMesh::accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept
{
    const auto& [vertices, nodes, parts, warning] = *this;
    budget.accountDynamicFields(vertices, nodes, parts, warning);
}

void Dust3DIndexedMesh::accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept
{
    const auto& [vertices, indices, nodes, parts, warning] = *this;
    budget.accountDynamicFields(vertices, indices, nodes, parts, warning);
}

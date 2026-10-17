#include <dust3d/base/axis_aligned_bounding_box_tree.h>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/snapshot.h>
#include <dust3d/base/uuid.h>

namespace dust3d {
namespace {
    void accountNode(const AxisAlignedBoudingBoxTree::Node* node, RetainedPayloadBudget& budget) noexcept
    {
        if (!node || !budget.tryAdd(1, sizeof(*node)))
            return;
        const auto& [box, center, indices, left, right] = *node;
        budget.accountDynamicFields(box, center, indices);
        accountNode(left, budget);
        accountNode(right, budget);
    }
}

void Uuid::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [uuid] = *this;
    budget.accountDynamic(uuid);
}

void Snapshot::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [canvas, nodes, edges, parts, components, root, animations] = *this;
    budget.accountDynamicFields(canvas, nodes, edges, parts, components, root, animations);
}

void AxisAlignedBoudingBoxTree::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [boxes, root, order, spans] = *this;
    budget.accountBorrowedPointer(boxes);
    budget.accountDynamicFields(order, spans);
    accountNode(root, budget);
}
}

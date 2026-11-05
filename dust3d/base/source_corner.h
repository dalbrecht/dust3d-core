// SPDX-License-Identifier: MIT
#ifndef DUST3D_BASE_SOURCE_CORNER_H_
#define DUST3D_BASE_SOURCE_CORNER_H_

#include <array>
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/uuid.h>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace dust3d {
/// Owned node hints for one actual triangle corner. Empty influences mean unknown.
/// Weighted values preserve their original scale; they are not normalized here.
struct SourceCorner {
    bool weighted = false;
    using Influences = std::vector<std::pair<Uuid, float>>;
    /// Immutable distributions are shared by copies; replacement never mutates aliases.
    /// The borrowed reference lives while at least one corner retains that distribution.
    const Influences& influences() const noexcept
    {
        static const Influences empty;
        return m_influences ? *m_influences : empty;
    }
    /// Replace the complete distribution, preserving order, duplicates and scale.
    /// The by-value argument owns a snapshot independent of caller mutations.
    void setInfluences(Influences values)
    {
        m_influences = values.empty() ? nullptr : std::make_shared<const Influences>(std::move(values));
    }
    /// Dominant bind-sample heuristic: aggregate duplicate node hints first,
    /// preserving original UUID occurrence order for exact aggregate ties.
    std::optional<Uuid> dominantNodeHint() const
    {
        const auto& influences = this->influences();
        if (!weighted && (influences.size() != 1 || influences[0].second != 1))
            return std::nullopt;
        if (influences.size() == 1) {
            const auto& hint = influences[0];
            return !hint.first.isNull() && std::isfinite(hint.second) && hint.second > 0
                ? std::optional<Uuid>(hint.first)
                : std::nullopt;
        }
        std::map<Uuid, double> totals;
        std::vector<Uuid> order;
        for (const auto& influence : influences) {
            if (influence.first.isNull() || !std::isfinite(influence.second) || influence.second < 0)
                return std::nullopt;
            const auto inserted = totals.emplace(influence.first, 0);
            if (inserted.second)
                order.push_back(influence.first);
            inserted.first->second += influence.second;
            if (!std::isfinite(inserted.first->second))
                return std::nullopt;
        }
        std::optional<Uuid> result;
        double greatest = 0;
        for (const auto& node : order) {
            if (totals.at(node) > greatest) {
                result = node;
                greatest = totals.at(node);
            }
        }
        return result;
    }
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        if (m_influences) {
            // Charge the complete distribution per logical owner, even when shared.
            // A shared_ptr-sized policy allowance covers shared ownership metadata;
            // this is conservative logical accounting, not allocator/RSS measurement.
            budget.tryAdd(1, sizeof(std::shared_ptr<const Influences>));
            budget.accountObject(*m_influences);
        }
    }

private:
    std::shared_ptr<const Influences> m_influences;
};

/// Explicit owning-array visitor keeps dynamic node hints in retained cache budgets.
struct SourceTriangleCorners {
    std::array<SourceCorner, 3> corners;
    SourceCorner& operator[](size_t index) { return corners.at(index); }
    const SourceCorner& operator[](size_t index) const { return corners.at(index); }
    void accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
    {
        for (const auto& corner : corners)
            budget.accountDynamic(corner);
    }
};
}
#endif

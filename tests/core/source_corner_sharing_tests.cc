// SPDX-License-Identifier: MIT
#include <dust3d/base/source_corner.h>
#include <limits>
#include <stdexcept>
#include <type_traits>
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Immutable source distribution contract failed");
}
int main()
{
    using dust3d::SourceCorner;
    static_assert(std::is_same_v<decltype(std::declval<SourceCorner&>().influences()), const SourceCorner::Influences&>);
    const dust3d::Uuid node("{00000000-0000-4000-8000-000000000001}");
    SourceCorner original;
    original.weighted = true;
    SourceCorner::Influences input { { node, .25f }, { node, .75f } };
    original.setInfluences(input);
    input[0].second = 99;
    std::vector<SourceCorner> copies(96, original);
    const auto* distribution = &original.influences();
    for (const auto& copy : copies) {
        require(&copy.influences() == distribution);
        require(copy.weighted && copy.influences().size() == 2);
        require(copy.influences()[0].second == .25f && copy.influences()[1].second == .75f);
    }
    dust3d::RetainedPayloadBudget dynamic(std::numeric_limits<size_t>::max());
    dynamic.accountDynamic(original);
    dust3d::RetainedPayloadBudget expected(std::numeric_limits<size_t>::max());
    expected.tryAdd(1, sizeof(SourceCorner::Influences));
    expected.tryAdd(1, sizeof(std::shared_ptr<const SourceCorner::Influences>));
    expected.accountDynamic(original.influences());
    require(dynamic.usedBytes() == expected.usedBytes());
    dust3d::RetainedPayloadBudget exact(dynamic.usedBytes());
    exact.accountDynamic(original);
    require(!exact.isExceeded());
    dust3d::RetainedPayloadBudget shortBudget(dynamic.usedBytes() - 1);
    shortBudget.accountDynamic(original);
    require(shortBudget.isExceeded());
    dust3d::RetainedPayloadBudget all(std::numeric_limits<size_t>::max());
    all.accountObject(copies);
    require(all.usedBytes() == sizeof(copies) + copies.capacity() * sizeof(SourceCorner) + copies.size() * dynamic.usedBytes());
    original.setInfluences({ { node, 99 } });
    require(&original.influences() != distribution);
    original = {};
    require(&copies.back().influences() == distribution);
    require(copies.back().influences()[1].second == .75f);
    copies.erase(copies.begin(), copies.end() - 1);
    require(copies.front().dominantNodeHint() == node);
    copies.front().setInfluences({});
    require(copies.front().influences().empty());
    dust3d::RetainedPayloadBudget empty(0);
    empty.accountDynamic(copies.front());
    require(!empty.isExceeded() && empty.usedBytes() == 0);
}

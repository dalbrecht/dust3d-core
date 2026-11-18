// SPDX-License-Identifier: MIT
#include <dust3d/base/object.h>
#include <dust3d/base/retained_payload_budget.h>
#include <iostream>
#include <stdexcept>

int main()
{
    try {
        dust3d::Object source;
        source.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
        source.triangles = { { 0, 1, 2 } };
        source.triangleProceduralUvs = { dust3d::ProceduralTriangleUv { {},
            { dust3d::Vector2(0.2, 0.3), dust3d::Vector2(0.4, 0.5), dust3d::Vector2(0.6, 0.7) } } };
        dust3d::RetainedPayloadBudget budget(1024 * 1024);
        budget.accountDynamic(source.triangleProceduralUvs);
        if (budget.isExceeded() || budget.usedBytes() <= source.triangleProceduralUvs.capacity() * sizeof(decltype(source.triangleProceduralUvs)::value_type))
            throw std::runtime_error("Procedural owner strings must be included beyond inline optional storage");
        dust3d::RetainedPayloadBudget exact(budget.usedBytes());
        exact.accountDynamic(source.triangleProceduralUvs);
        if (exact.isExceeded())
            throw std::runtime_error("Exact procedural retained payload budget must succeed");
        dust3d::RetainedPayloadBudget shortBudget(budget.usedBytes() - 1);
        shortBudget.accountDynamic(source.triangleProceduralUvs);
        if (!shortBudget.isExceeded())
            throw std::runtime_error("Procedural retained payload budget must include owned UUID storage");
        auto target = source;
        source.triangleProceduralUvs[0]->uv[0][0] = 0.9;
        if (target.triangleProceduralUvs[0]->uv[0].x() != 0.2)
            throw std::runtime_error("Object copy must retain independent procedural corners");
        target.triangleProceduralUvs.clear();
        target.copyUvFrom(source);
        if (target.triangleProceduralUvs.size() != 1 || !target.triangleProceduralUvs[0]
            || target.triangleProceduralUvs[0]->uv[0].x() != 0.9)
            throw std::runtime_error("copyUvFrom must retain triangle-indexed procedural charts");
        source.triangleProceduralUvs[0]->uv[0][0] = 0.1;
        if (target.triangleProceduralUvs[0]->uv[0].x() != 0.9)
            throw std::runtime_error("copyUvFrom must own copied procedural corners");
        target.triangles[0] = { 2, 1, 0 };
        target.copyUvFrom(source);
        if (target.triangleProceduralUvs.size() != 1 || target.triangleProceduralUvs[0])
            throw std::runtime_error("Different topology must not copy procedural corner ordinals");
        source.triangleProceduralUvs.clear();
        target.copyUvFrom(source);
        if (!target.triangleProceduralUvs.empty())
            throw std::runtime_error("Legacy UV copy must clear stale indexed mappings");
        std::cout << "Procedural UV copy ownership contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

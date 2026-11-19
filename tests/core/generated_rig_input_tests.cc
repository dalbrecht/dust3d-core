// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "GeneratedRigInput.h"
#include <dust3d/base/retained_payload_budget.h>
#include <iostream>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <class F>
void rejects(F run, Dust3DStatus status)
{
    try {
        run();
    } catch (const DocumentError& e) {
        require(e.status() == status, "Wrong failure cause");
        return;
    }
    throw std::runtime_error("Expected rejection");
}
template <class F>
void accountsOwnedGrowth(F populate)
{
    Object empty;
    RetainedPayloadBudget baseline(256 * 1024 * 1024);
    baseline.accountObject(empty);
    Object populated;
    populate(populated);
    RetainedPayloadBudget measured(256 * 1024 * 1024);
    measured.accountObject(populated);
    require(!measured.isExceeded() && measured.usedBytes() > baseline.usedBytes(), "Owned field was omitted from payload accounting");
}
int main()
{
    try {
        accountsOwnedGrowth([](Object& o) { o.setTriangleSourceNodes({ { Uuid {}, Uuid {} } }); });
        accountsOwnedGrowth([](Object& o) { o.setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } } }); });
        accountsOwnedGrowth([](Object& o) { o.setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } } }); });
        accountsOwnedGrowth([](Object& o) { o.setTriangleTangents({ { 1, 0, 0 } }); });
        accountsOwnedGrowth([](Object& o) { o.setPartUvRects({ { Uuid {}, { Rectangle {} } } }); });
        accountsOwnedGrowth([](Object& o) { o.setTriangleLinks({ { { 0, 1 }, { 1, 2 } } }); });
        accountsOwnedGrowth([](Object& o) { o.triangleSourceCorners.resize(1); o.triangleSourceCorners[0][0].setInfluences({{Uuid {}, 1}}); });
        accountsOwnedGrowth([](Object& o) { o.triangleProceduralUvs.resize(1); o.triangleProceduralUvs[0].emplace(); });
        accountsOwnedGrowth([](Object& o) { o.componentPositionToNodeIdMap["processed component"] = {}; });
        accountsOwnedGrowth([](Object& o) { o.vertexBone1 = { { "First", 1 } }; });
        accountsOwnedGrowth([](Object& o) { o.vertexBone2 = { { "Second", 1 } }; });
        accountsOwnedGrowth([](Object& o) { o.vertexBone3 = { { "Third", 1 } }; });
        accountsOwnedGrowth([](Object& o) { o.vertexBone4 = { { "Fourth", 1 } }; });
        auto snapshot = std::make_unique<Snapshot>();
        snapshot->nodes["processed mirror"]["mirrorFrom"] = "original";
        auto object = std::make_unique<Object>();
        object->vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
        object->triangles = { { 0, 1, 2 } };
        object->triangleModelKeys = { "owned model" };
        object->vertexBone4 = { { "Fourth", 1 }, { "", 0 }, { "", 0 } };
        object->setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } } });
        object->setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } } });
        const auto* snapshotPointer = snapshot.get();
        const auto* objectPointer = object.get();
        RetainedPayloadBudget expected(256 * 1024 * 1024);
        expected.accountObject(*snapshot);
        expected.accountObject(*object);
        require(!expected.isExceeded(), "Ordinary accounting exceeded");
        const auto bytes = expected.usedBytes();
        auto input = retainGeneratedRigInput(std::move(snapshot), std::move(object), bytes);
        require(!snapshot && !object, "Ownership not transferred");
        require(&input->snapshot() == snapshotPointer && &input->object() == objectPointer, "Owner copied rather than moved inputs");
        require(input->payloadBytes() == bytes, "Aggregate accounting differs");
        require(input->snapshot().nodes.at("processed mirror").at("mirrorFrom") == "original", "Processed identity lost");
        require(input->object().triangleModelKeys.at(0) == "owned model" && input->object().vertexBone4.at(0).first == "Fourth", "Metadata lost");
        require(input->object().triangleVertexUvs()->at(0).at(1).x() == 1, "UV cache lost");
        rejects([&] { retainGeneratedRigInput(std::make_unique<Snapshot>(input->snapshot()), std::make_unique<Object>(input->object()), 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { retainGeneratedRigInput(nullptr, std::make_unique<Object>()); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { retainGeneratedRigInput(std::make_unique<Snapshot>(), nullptr); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { retainGeneratedRigInput(std::make_unique<Snapshot>(), std::make_unique<Object>(), 256 * 1024 * 1024 + 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        auto moved = std::move(input);
        require(!input && &moved->object() == objectPointer, "Move lost owner");
        std::cout << "Generated rig input ownership passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

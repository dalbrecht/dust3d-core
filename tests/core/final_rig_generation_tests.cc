// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FinalRigGeneration.h"
#include "GeneratedRigInput.h"
#include <dust3d/base/position_key.h>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <class F>
void rejectsValue(F run, RigFinalDataFailure failure)
{
    try {
        run();
    } catch (const RigFinalDataError& error) {
        require(error.failure() == failure, "Wrong final value cause");
        return;
    }
    throw std::runtime_error("Expected final value rejection");
}
template <class F>
void rejectsDocument(F run, Dust3DStatus status)
{
    try {
        run();
    } catch (const DocumentError& error) {
        require(error.status() == status, "Wrong document cause");
        return;
    }
    throw std::runtime_error("Expected document rejection");
}
int main()
{
    try {
        const Uuid first("{00000000-0000-0000-0000-000000000001}");
        const Uuid second("{00000000-0000-0000-0000-000000000002}");
        auto snapshot = std::make_unique<Snapshot>();
        snapshot->canvas = { { "originX", "0" }, { "originY", "0" }, { "originZ", "0" } };
        snapshot->parts["body"] = { { "target", "Model" } };
        snapshot->nodes[first.toString()] = { { "x", "0" }, { "y", "-1" }, { "z", "0" }, { "radius", "0.2" }, { "partId", "body" } };
        snapshot->nodes[second.toString()] = { { "x", "0" }, { "y", "-3" }, { "z", "0" }, { "radius", "0.4" }, { "partId", "body" } };
        snapshot->edges["edge"] = { { "from", first.toString() }, { "to", second.toString() }, { "partId", "body" }, { "boneName", "Body" } };
        auto object = std::make_unique<Object>();
        object->vertices = { { 0, 1, 0 }, { 0, 3, 0 }, { 1, 1, 0 } };
        object->triangles = { { 0, 1, 2 } };
        object->positionToNodeIdMap = { { PositionKey(object->vertices[0]), first }, { PositionKey(object->vertices[1]), second }, { PositionKey(object->vertices[2]), first } };
        object->setTriangleVertexUvs({ { { 0, 0 }, { 0, 1 }, { 1, 0 } } });
        auto input = retainGeneratedRigInput(std::move(snapshot), std::move(object));
        RigStructure templ;
        templ.type = "Test";
        RigNode body;
        body.name = "Body";
        body.posY = 99;
        body.endY = 100;
        templ.bones = { body };
        const auto result = generateFinalRig(*input, templ);
        require(result.data.joints.size() == 1 && result.data.vertices.size() == 3, "Final owned counts");
        require(result.data.joints[0].headPosition[1] == 0 && result.data.joints[0].tailPosition[1] == 2, "Actual positions/grounding differ");
        require(result.object->vertices[0].y() == 0 && result.object->vertices[1].y() == 2, "Body and bones do not share grounding");
        for (const auto& weight : result.data.vertices)
            require(weight.joints[0] == 0 && weight.weights[0] == 1, "Assigned source weight lost");
        require(input->object().vertices[0].y() == 1 && templ.bones[0].posY == 99, "Input mutated");
        require(result.object->triangleVertexUvs()->at(0).at(2).x() == 1 && result.object->triangles == input->object().triangles, "UV/topology lost");
        require(!result.eyelidsGenerated, "Unrequested eyelids generated");
        templ.headHasEyelids = true;
        const auto absent = generateFinalRig(*input, templ);
        require(!absent.eyelidsGenerated && absent.data.joints.size() == 1, "Optional absent eyelids discarded base rig");
        std::atomic_bool cancelled { true };
        RigFinalDataOptions options;
        options.cancellation = &cancelled;
        rejectsValue([&] { generateFinalRig(*input, templ, options); }, RigFinalDataFailure::cancelled);
        options.cancellation = nullptr;
        options.maximumPayloadBytes = 1;
        rejectsValue([&] { generateFinalRig(*input, templ, options); }, RigFinalDataFailure::limitExceeded);
        options.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
        rejectsDocument([&] { generateFinalRig(*input, templ, options); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        auto cyclic = templ;
        cyclic.bones[0].parent = "Body";
        rejectsValue([&] { generateFinalRig(*input, cyclic); }, RigFinalDataFailure::invalidData);
        auto duplicate = templ;
        duplicate.bones.push_back(body);
        rejectsValue([&] { generateFinalRig(*input, duplicate); }, RigFinalDataFailure::invalidData);
        RigStructure empty;
        rejectsDocument([&] { generateFinalRig(*input, empty); }, DUST3D_STATUS_INVALID_DOCUMENT);
        std::cout << "Final rig generation contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

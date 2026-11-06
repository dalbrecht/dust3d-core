// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FinalRigGeneration.h"
#include "GeneratedRigInput.h"
#include <cmath>
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
        // Two ordinary open annuli. Outer boundaries exceed the source detector's
        // head-relative radius filter; the two inner holes form the symmetric pair.
        auto eyeSnapshot = std::make_unique<Snapshot>(input->snapshot());
        eyeSnapshot->nodes[second.toString()]["y"] = "-1.5";
        eyeSnapshot->edges["edge"]["boneName"] = "Head";
        auto eyeObject = std::make_unique<Object>();
        const std::array<std::array<double, 2>, 4> directions { { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } } };
        for (double center : { -0.7, 0.7 }) {
            const auto base = eyeObject->vertices.size();
            for (double radius : { 0.1, 0.5 })
                for (const auto& direction : directions) {
                    const Vector3 vertex(center + radius * direction[0], 1.25 + radius * direction[1], 0);
                    eyeObject->vertices.push_back(vertex);
                    eyeObject->positionToNodeIdMap.emplace(PositionKey(vertex), first);
                }
            for (size_t corner = 0; corner < 4; ++corner) {
                const auto next = (corner + 1) % 4;
                eyeObject->triangles.push_back({ base + corner, base + 4 + corner, base + 4 + next });
                eyeObject->triangles.push_back({ base + corner, base + 4 + next, base + next });
            }
        }
        auto eyes = retainGeneratedRigInput(std::move(eyeSnapshot), std::move(eyeObject));
        auto headTemplate = templ;
        headTemplate.bones[0].name = "Head";
        const auto lids = generateFinalRig(*eyes, headTemplate);
        require(lids.eyelidsGenerated && lids.data.joints.size() == 5, "Symmetric eye holes did not generate four lids");
        const std::array<std::string, 4> names { "LeftUpperEyelid", "LeftLowerEyelid", "RightUpperEyelid", "RightLowerEyelid" };
        for (size_t i = 0; i < names.size(); ++i) {
            require(lids.data.joints[i + 1].name == names[i] && lids.data.joints[i + 1].parentIndex == 0, "Eyelid names/parents lost");
            require(std::isfinite(lids.data.joints[i + 1].closingAngle), "Eyelid closing angle is not finite");
        }
        require(lids.data.vertices[9].joints[0] == 1 && lids.data.vertices[11].joints[0] == 2
                && lids.data.vertices[1].joints[0] == 3 && lids.data.vertices[3].joints[0] == 4,
            "Eyelid loop vertex binding differs");
        for (const auto& weights : lids.data.vertices)
            require(weights.weights[0] == 1 && weights.weights[1] == 0 && weights.weights[2] == 0 && weights.weights[3] == 0, "Eyelid final weights not normalized");
        require(eyes->object().vertices[0].y() == 1.25 && lids.object->triangles == eyes->object().triangles, "Eyelid generation changed input/topology");
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

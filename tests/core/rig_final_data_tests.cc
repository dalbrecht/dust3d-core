// SPDX-License-Identifier: MIT
#include "RigFinalData.h"
#include <cmath>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Operation>
void rejects(Operation operation, RigFinalDataFailure failure)
{
    try {
        operation();
    } catch (const RigFinalDataError& cause) {
        require(cause.failure() == failure && cause.what()[0], "Typed original failure");
        return;
    }
    throw std::runtime_error("Expected atomic rejection");
}
RigNode bone(std::string name, std::string parent = "")
{
    RigNode value;
    value.name = std::move(name);
    value.parent = std::move(parent);
    value.endY = 1;
    return value;
}
RigStructure rig()
{
    RigStructure value;
    value.bones = { bone("Child", "Root"), bone("Root"), bone("Third", "Root"), bone("Fourth", "Third") };
    return value;
}
Object object()
{
    Object value;
    value.vertices = { Vector3(0, 0, 0) };
    value.vertexBone1 = { { "Child", .1f } };
    value.vertexBone2 = { { "Root", .2f } };
    value.vertexBone3 = { { "Third", .3f } };
    value.vertexBone4 = { { "Fourth", .4f } };
    return value;
}
void sourceOrderFourWeightsAndOwnership()
{
    auto source = rig();
    auto geometry = object();
    const auto copied = copyFinalRigData(source, geometry);
    require(copied.joints.size() == 4 && copied.vertices.size() == 1, "Complete owned records");
    require(copied.joints[0].name == "Child" && copied.joints[0].parentIndex == 1, "Source order and forward parent");
    require(copied.joints[1].parentIndex == rigFinalNoJoint && copied.joints[3].parentIndex == 2, "Resolved parent forest");
    for (size_t slot = 0; slot < 4; ++slot) {
        require(copied.vertices[0].joints[slot] == slot, "Retain all four source influence slots");
        require(copied.vertices[0].weights[slot] == (.1f * static_cast<float>(slot + 1)), "Do not silently renormalize source weights");
    }
    source.bones[0].name = "Changed";
    geometry.vertexBone1[0] = { "Changed", 1 };
    require(copied.joints[0].name == "Child" && copied.vertices[0].weights[0] == .1f, "No input storage retained");
    auto afterLifetime = [] { return copyFinalRigData(rig(), object()); }();
    require(afterLifetime.joints[3].name == "Fourth", "Caller lifetime independent");
}
void exactNamesParentsAndZeroLength()
{
    RigStructure source;
    Object empty;
    source.bones = { bone("Caf\xc3\xa9"), bone("Cafe\xcc\x81", "Caf\xc3\xa9") };
    source.bones[0].endY = 0; // Actual unassigned bones may have zero length.
    require(copyFinalRigData(source, empty).joints[1].parentIndex == 0, "Exact Unicode identity and valid zero-length bone");
    source.bones[1].name = source.bones[0].name;
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone("A", "B"), bone("B", "A") };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone("A", "missing") };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone(std::string("A\0B", 3)) };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone("") };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone(std::string("A\x7f", 2)) };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
    source.bones = { bone(std::string(1025, 'a')) };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::limitExceeded);
    source.bones = { bone(std::string("\xc0\xaf", 2)) };
    rejects([&] { copyFinalRigData(source, empty); }, RigFinalDataFailure::invalidData);
}
void unboundAndSparseSlots()
{
    auto source = rig();
    Object geometry;
    geometry.vertices = { Vector3(), Vector3(1, 0, 0) };
    geometry.vertexBone4 = { { "Root", 1 }, { "future unused name", 0 } };
    const auto copied = copyFinalRigData(source, geometry);
    require(copied.vertices[0].joints[3] == 1 && copied.vertices[0].joints[0] == rigFinalNoJoint, "Preserve occupied channel");
    for (size_t slot = 0; slot < 4; ++slot)
        require(copied.vertices[1].joints[slot] == rigFinalNoJoint && copied.vertices[1].weights[slot] == 0, "Explicit unbound, no fabricated Root weight");
    require(copyFinalRigData(RigStructure(), Object()).logicalBytes == 0, "Empty owned data");
}
void malformedWeights()
{
    const auto source = rig();
    auto geometry = object();
    geometry.vertexBone1.push_back({ "Root", 1 });
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    for (const auto& entry : { std::pair<std::string, float> { "unknown", .1f }, { "", .1f }, { "Child", -.1f }, { "Child", 1.1f }, { "Child", std::numeric_limits<float>::quiet_NaN() } }) {
        geometry = object();
        geometry.vertexBone1[0] = entry;
        rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    }
    geometry = object();
    geometry.vertexBone4[0].second = .2f;
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
}
void finiteValuesAndQuotas()
{
    auto source = rig();
    auto geometry = object();
    source.bones[0].posX = std::numeric_limits<float>::infinity();
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    source = rig();
    source.bones[0].capsuleRadius = -1;
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    source = rig();
    source.bones[0].closingAngle = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    source = rig();
    geometry.vertices[0] = Vector3(0, std::numeric_limits<double>::infinity(), 0);
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::invalidData);
    geometry = object();
    RigFinalDataOptions options;
    options.maximumBones = 3;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::limitExceeded);
    options = {};
    options.maximumPayloadBytes = copyFinalRigData(source, geometry).logicalBytes;
    require(copyFinalRigData(source, geometry, options).logicalBytes == options.maximumPayloadBytes, "Exact retained quota");
    --options.maximumPayloadBytes;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::limitExceeded);
    options = {};
    options.maximumVertices = 0;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::invalidData);
    options = {};
    options.maximumBones = 257;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::invalidData);
    options = {};
    options.maximumPayloadBytes = 0;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::invalidData);
    options = {};
    options.maximumVertices = 1000001;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::invalidData);
    options = {};
    options.maximumVertices = 1;
    geometry.vertices.push_back(Vector3(1, 0, 0));
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::limitExceeded);
    geometry = object();
    source.bones[0].posX = 1000001;
    rejects([&] { copyFinalRigData(source, geometry); }, RigFinalDataFailure::limitExceeded);
    source = rig();
    std::atomic_bool cancelled { false };
    options = {};
    options.cancellation = &cancelled;
    const auto retained = copyFinalRigData(source, geometry, options);
    require(retained.joints.size() == 4 && !cancelled.load(), "Live cancellation flag does not alter a successful copy");
    cancelled.store(true);
    options = {};
    options.cancellation = &cancelled;
    rejects([&] { copyFinalRigData(source, geometry, options); }, RigFinalDataFailure::cancelled);
}
}
int main()
{
    try {
        sourceOrderFourWeightsAndOwnership();
        exactNamesParentsAndZeroLength();
        unboundAndSparseSlots();
        malformedWeights();
        finiteValuesAndQuotas();
        std::cout << "Final rig value ownership contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

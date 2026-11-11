// SPDX-License-Identifier: MIT
#include <atomic>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <stdexcept>
using namespace dust3d;
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
int main()
{
    try {
        const Uuid first("{00000000-0000-0000-0000-000000000001}");
        const Uuid second("{00000000-0000-0000-0000-000000000002}");
        Object object;
        object.vertices = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, 0, 0 } };
        object.positionToNodeIdMap = { { PositionKey(object.vertices[0]), first }, { PositionKey(object.vertices[1]), second } };
        const std::map<Uuid, NodeBoneInfluence> influences { { first, NodeBoneInfluence("First") }, { second, NodeBoneInfluence("Second") } };
        const auto fresh = object;
        RigGenerator generator;
        require(generator.computeVertexBoneBindings(&object, influences), "Default binding failed");
        require(object.vertexBone1[2].first == "First", "Nearest tie did not choose first source vertex");
        const auto before = object.vertexBone1;
        const auto before2 = object.vertexBone2;
        const auto before3 = object.vertexBone3;
        const auto before4 = object.vertexBone4;
        std::atomic_bool cancelled { true };
        generator.setBindingCancellationFlag(&cancelled);
        require(!generator.computeVertexBoneBindings(&object, influences), "Precancelled binding accepted");
        require(generator.getErrorMessage() == "Vertex bone binding cancelled", "Cancellation cause lost");
        require(object.vertexBone1 == before && object.vertexBone2 == before2 && object.vertexBone3 == before3 && object.vertexBone4 == before4, "Precancel changed old weights");
        cancelled.store(false);
        object = fresh;
        require(generator.computeVertexBoneBindings(&object, influences), "Cleared flag did not allow new operation");
        require(object.vertexBone1 == before && generator.getErrorMessage().empty(), "False flag changed output/cause");
        generator.setBindingCancellationFlag(nullptr);
        cancelled.store(true);
        object = fresh;
        require(generator.computeVertexBoneBindings(&object, influences), "Detached flag still affected binding");
        require(object.vertexBone1 == before, "Detached flag changed binding order");
        std::cout << "Rig binding cancellation contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

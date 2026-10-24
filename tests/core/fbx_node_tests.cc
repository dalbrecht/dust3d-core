// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxNode.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
using namespace dust3d::native;
static_assert(!std::is_copy_constructible_v<FbxNode> && std::is_nothrow_move_constructible_v<FbxNode>);
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus expected)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == expected, "FBX node typed cause changed");
        return;
    }
    throw std::runtime_error("Invalid FBX node admitted");
}
FbxNode tree(FbxNodeLimits limits = {})
{
    FbxPropertyBlock properties;
    properties.appendInt32(1);
    properties.appendString(std::string("Mesh\0\1Geometry", 14));
    std::vector<FbxNode> children;
    children.reserve(4);
    children.emplace_back("A");
    return FbxNode("Root", std::move(properties), std::move(children), limits);
}
void metadataAndOwnership()
{
    auto root = tree();
    require(root.name() == std::vector<uint8_t>({ 'R', 'o', 'o', 't' }), "FBX node name changed");
    require(root.depth() == 2 && root.nodeCount() == 2, "FBX subtree metadata changed");
    require(root.properties().propertyCount() == 2 && root.properties().bytes().size() == 24, "FBX property ownership lost");
    require(root.encodedSize() == 68 && root.children().front().encodedSize() == 14, "FBX node record/terminator size changed");
    const size_t expected = sizeof(FbxNode) + root.name().capacity()
        + root.properties().payloadBytes() - sizeof(FbxPropertyBlock)
        + root.children().capacity() * sizeof(FbxNode)
        + root.children().front().payloadBytes() - sizeof(FbxNode);
    require(root.payloadBytes() == expected, "FBX subtree spare capacity was not charged");
    const auto payload = root.payloadBytes();
    auto moved = std::move(root);
    require(moved.encodedSize() == 68 && moved.payloadBytes() == payload && moved.properties().propertyCount() == 2,
        "FBX node move lost cached ownership metadata");
    FbxNodeLimits exact;
    exact.maximumPayloadBytes = payload;
    auto admitted = tree(exact);
    require(admitted.payloadBytes() == payload, "FBX exact subtree quota changed");
    exact.maximumPayloadBytes = payload - 1;
    rejects([&] { (void)tree(exact); }, DUST3D_STATUS_LIMIT_EXCEEDED);
}
void namesAndLimits()
{
    rejects([] { FbxNode invalid(""); }, DUST3D_STATUS_INVALID_DOCUMENT);
    rejects([] { FbxNode invalid(std::string("A\0B", 3)); }, DUST3D_STATUS_INVALID_DOCUMENT);
    FbxNode largest(std::string(255, 'x'));
    require(largest.name().size() == 255 && largest.encodedSize() == 268, "FBX name byte boundary changed");
    rejects([] { FbxNode invalid(std::string(256, 'x')); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    for (int kind = 0; kind < 6; ++kind) {
        FbxNodeLimits limits;
        if (kind == 0)
            limits.maximumPayloadBytes = sizeof(FbxNode);
        if (kind == 1)
            limits.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
        if (kind == 2)
            limits.maximumNodes = 0;
        if (kind == 3)
            limits.maximumNodes = 65537;
        if (kind == 4)
            limits.maximumDepth = 0;
        if (kind == 5)
            limits.maximumDepth = 65;
        rejects([&] { (void)tree(limits); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    }
    FbxNodeLimits count;
    count.maximumNodes = 2;
    auto two = tree(count);
    require(two.nodeCount() == 2, "FBX node count exact boundary rejected");
    count.maximumNodes = 1;
    rejects([&] { (void)tree(count); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    rejects([] {
        std::vector<FbxNode> children;
        children.push_back(tree());
        FbxNodeLimits limits;
        limits.maximumNodes = 2;
        FbxNode parent("A", FbxPropertyBlock(), std::move(children), limits);
    },
        DUST3D_STATUS_LIMIT_EXCEEDED);
    rejects([] {
        std::vector<FbxNode> children;
        children.reserve(1);
        children.push_back(tree());
        FbxNodeLimits limits;
        limits.maximumPayloadBytes = 2 * sizeof(FbxNode) + 1;
        FbxNode parent("A", FbxPropertyBlock(), std::move(children), limits);
    },
        DUST3D_STATUS_LIMIT_EXCEEDED);
    FbxNodeLimits depth;
    depth.maximumDepth = 1;
    rejects([&] { (void)tree(depth); }, DUST3D_STATUS_LIMIT_EXCEEDED);
}
void boundedDepth()
{
    FbxNode node("Leaf");
    for (size_t depth = 2; depth <= 64; ++depth) {
        std::vector<FbxNode> children;
        children.push_back(std::move(node));
        node = FbxNode("Parent", FbxPropertyBlock(), std::move(children));
        require(node.depth() == depth && node.nodeCount() == depth, "FBX deep subtree metadata changed");
    }
    rejects([&] {
        std::vector<FbxNode> children;
        children.push_back(std::move(node));
        FbxNode tooDeep("Parent", FbxPropertyBlock(), std::move(children));
    },
        DUST3D_STATUS_LIMIT_EXCEEDED);
}
}
int main()
{
    try {
        metadataAndOwnership();
        namesAndLimits();
        boundedDepth();
        std::cout << "FBX immutable nodes passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxDocument.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
using namespace dust3d::native;
static_assert(!std::is_copy_constructible_v<OwnedFbxDocument> && std::is_nothrow_move_constructible_v<OwnedFbxDocument>);
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
uint32_t read32(const std::vector<uint8_t>& bytes, size_t offset)
{
    require(offset <= bytes.size() && bytes.size() - offset >= 4, "FBX fixture offset out of bounds");
    uint32_t value = 0;
    for (size_t byte = 0; byte < 4; ++byte)
        value |= uint32_t(bytes[offset + byte]) << (byte * 8);
    return value;
}
void zeros(const std::vector<uint8_t>& bytes, size_t offset)
{
    require(offset <= bytes.size() && bytes.size() - offset >= 13, "FBX null record out of bounds");
    for (size_t index = 0; index < 13; ++index)
        require(bytes[offset + index] == 0, "FBX null record changed");
}
std::vector<FbxNode> tree()
{
    FbxPropertyBlock properties;
    properties.appendInt32(1);
    properties.appendString(std::string("Mesh\0\1Geometry", 14));
    std::vector<FbxNode> children;
    children.emplace_back("A");
    std::vector<FbxNode> roots;
    roots.emplace_back("Root", std::move(properties), std::move(children));
    return roots;
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus expected)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == expected, "FBX document typed cause changed");
        return;
    }
    throw std::runtime_error("Invalid FBX document admitted");
}
void records()
{
    auto roots = tree();
    const auto sourceProperties = roots.front().properties().bytes();
    auto document = encodeFbxDocument(roots);
    const auto& bytes = document.bytes();
    const std::string magic("Kaydara FBX Binary  \0\x1a\0", 23);
    require(bytes.size() == 284 && std::equal(magic.begin(), magic.end(), bytes.begin()), "FBX7400 header/length changed");
    require(read32(bytes, 23) == 7400, "FBX version changed");
    require(read32(bytes, 27) == 95 && read32(bytes, 31) == 2 && read32(bytes, 35) == 24 && bytes[39] == 4, "FBX root absolute offset/count/list length changed");
    require(std::vector<uint8_t>(bytes.begin() + 40, bytes.begin() + 44) == roots.front().name(), "FBX root name changed");
    require(std::vector<uint8_t>(bytes.begin() + 44, bytes.begin() + 68) == sourceProperties, "FBX encoded property block changed");
    require(read32(bytes, 68) == 82 && read32(bytes, 72) == 0 && read32(bytes, 76) == 0 && bytes[80] == 1 && bytes[81] == 'A', "FBX child offsets became relative");
    zeros(bytes, 82);
    zeros(bytes, 95);
    const std::vector<uint8_t> footerPrefix { 0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66, 0xb1, 0x76, 0xfb, 0x83, 0x1c, 0xf7, 0x26, 0x7e };
    const std::vector<uint8_t> footerSuffix { 0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e, 0xec, 0xe9, 0x0c, 0xe3, 0x75, 0x8f, 0x29, 0x0b };
    require(std::equal(footerPrefix.begin(), footerPrefix.end(), bytes.begin() + 108) && std::equal(footerSuffix.begin(), footerSuffix.end(), bytes.end() - 16), "Reference FBX footer changed");
    require(read32(bytes, 108 + 36) == 7400, "Reference footer version changed");
    require(roots.front().properties().bytes() == sourceProperties && roots.front().encodedSize() == 68, "FBX encoding mutated source");
    auto ownedBytes = bytes;
    roots.clear();
    auto moved = std::move(document);
    require(moved.bytes() == ownedBytes, "FBX encoded owner borrowed its input tree");
    std::vector<FbxNode> siblings;
    siblings.emplace_back("A");
    siblings.emplace_back("B");
    const auto siblingDocument = encodeFbxDocument(siblings);
    require(read32(siblingDocument.bytes(), 27) == 41 && read32(siblingDocument.bytes(), 41) == 55, "FBX sibling absolute offset changed");
    zeros(siblingDocument.bytes(), 55);
}
void depthAndNameBoundaries()
{
    FbxNode node("A");
    for (size_t depth = 2; depth <= 64; ++depth) {
        std::vector<FbxNode> children;
        children.push_back(std::move(node));
        node = FbxNode("P", FbxPropertyBlock(), std::move(children));
    }
    std::vector<FbxNode> roots;
    roots.push_back(std::move(node));
    const auto output = encodeFbxDocument(roots);
    require(output.bytes().size() == 1931 && read32(output.bytes(), 27) == 1742, "FBX depth64 offset/terminators changed");
    zeros(output.bytes(), 1742);
    std::vector<FbxNode> named;
    named.emplace_back(std::string(255, 'x'));
    const auto largest = encodeFbxDocument(named);
    require(largest.bytes().size() == 484 && largest.bytes()[39] == 255, "FBX255-byte name narrowed incorrectly");
}
void limits()
{
    auto roots = tree();
    const auto document = encodeFbxDocument(roots);
    FbxDocumentLimits exact;
    exact.maximumPayloadBytes = document.payloadBytes();
    auto admitted = encodeFbxDocument(roots, exact);
    require(admitted.bytes() == document.bytes() && admitted.payloadBytes() == exact.maximumPayloadBytes, "FBX exact output quota rejected");
    exact.maximumPayloadBytes -= 1;
    rejects([&] { (void)encodeFbxDocument(roots, exact); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    for (int kind = 0; kind < 4; ++kind) {
        FbxDocumentLimits invalid;
        if (kind == 0)
            invalid.maximumPayloadBytes = sizeof(OwnedFbxDocument);
        if (kind == 1)
            invalid.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
        if (kind == 2)
            invalid.maximumNodes = 0;
        if (kind == 3)
            invalid.maximumNodes = 65537;
        rejects([&] { (void)encodeFbxDocument(roots, invalid); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    }
    FbxDocumentLimits count;
    count.maximumNodes = 1;
    rejects([&] { (void)encodeFbxDocument(roots, count); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    count.maximumNodes = 2;
    require(encodeFbxDocument(roots, count).bytes() == document.bytes(), "FBX document exact descendant count rejected");
    std::vector<FbxNode> empty;
    rejects([&] { (void)encodeFbxDocument(empty); }, DUST3D_STATUS_INVALID_DOCUMENT);
    require(roots.front().nodeCount() == 2 && roots.front().encodedSize() == 68, "Failed FBX encoding mutated its source");
}
}
int main()
{
    try {
        records();
        depthAndNameBoundaries();
        limits();
        std::cout << "FBX document encoding passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxPropertyBlock.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d::native;
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
        require(cause.status() == expected, "FBX property typed cause changed");
        return;
    }
    throw std::runtime_error("Invalid FBX property admitted");
}
void scalarFixtures()
{
    FbxPropertyBlock block;
    block.appendInt16(-32768);
    block.appendBoolean(true);
    block.appendBoolean(false);
    block.appendInt32(-7);
    block.appendFloat(1.0F);
    block.appendDouble(-2.0);
    block.appendInt64(std::numeric_limits<int64_t>::min());
    const std::vector<uint8_t> expected {
        'Y', 0, 128, 'C', 1, 'C', 0, 'I', 249, 255, 255, 255,
        'F', 0, 0, 128, 63, 'D', 0, 0, 0, 0, 0, 0, 0, 192,
        'L', 0, 0, 0, 0, 0, 0, 0, 128
    };
    require(block.bytes() == expected && block.propertyCount() == 7, "FBX scalar little-endian fixture changed");
    FbxPropertyBlock typedName;
    typedName.appendString(std::string("Mesh\0\1Geometry", 14));
    std::vector<uint8_t> expectedName { 'S', 14, 0, 0, 0, 'M', 'e', 's', 'h', 0, 1, 'G', 'e', 'o', 'm', 'e', 't', 'r', 'y' };
    require(typedName.bytes() == expectedName, "FBX typed name lost embedded null/type separator");
    FbxPropertyBlock raw;
    raw.appendRaw({ 0, 255, 17 });
    require(raw.bytes() == std::vector<uint8_t>({ 'R', 3, 0, 0, 0, 0, 255, 17 }), "FBX raw bytes changed");
    FbxPropertyBlock emptyRaw;
    emptyRaw.appendRaw({});
    require(emptyRaw.bytes() == std::vector<uint8_t>({ 'R', 0, 0, 0, 0 }), "Empty FBX raw value changed");
    FbxPropertyBlock growing;
    growing.appendRaw(std::vector<uint8_t>(100, 7));
    const auto growingSource = growing.bytes();
    growing.appendRaw(growing.bytes());
    require(std::vector<uint8_t>(growing.bytes().end() - growingSource.size(), growing.bytes().end()) == growingSource,
        "FBX self-alias growth invalidated input");
    const auto old = raw.bytes();
    raw.appendRaw(raw.bytes());
    require(raw.propertyCount() == 2, "FBX self-alias raw append lost count");
    require(std::vector<uint8_t>(raw.bytes().end() - old.size(), raw.bytes().end()) == old, "FBX self-alias raw append changed source bytes");
}
void arrayFixtures()
{
    auto checks = [](const FbxPropertyBlock& block, uint8_t tag, const std::vector<uint8_t>& data, uint32_t count) {
        std::vector<uint8_t> expected { tag };
        for (uint32_t value : { count, uint32_t(0), uint32_t(data.size()) })
            for (size_t byte = 0; byte < 4; ++byte)
                expected.push_back(static_cast<uint8_t>(value >> (byte * 8)));
        expected.insert(expected.end(), data.begin(), data.end());
        require(block.bytes() == expected && block.propertyCount() == 1, "FBX uncompressed array fixture changed");
    };
    FbxPropertyBlock ints;
    ints.appendInt32Array({ 1, -2 });
    checks(ints, 'i', { 1, 0, 0, 0, 254, 255, 255, 255 }, 2);
    FbxPropertyBlock floats;
    floats.appendFloatArray({ 1, -0.0F });
    checks(floats, 'f', { 0, 0, 128, 63, 0, 0, 0, 128 }, 2);
    FbxPropertyBlock doubles;
    doubles.appendDoubleArray({ -2 });
    checks(doubles, 'd', { 0, 0, 0, 0, 0, 0, 0, 192 }, 1);
    FbxPropertyBlock longs;
    longs.appendInt64Array({ -1 });
    checks(longs, 'l', { 255, 255, 255, 255, 255, 255, 255, 255 }, 1);
    FbxPropertyBlock booleans;
    booleans.appendBooleanArray({ true, false, true });
    checks(booleans, 'b', { 1, 0, 1 }, 3);
    FbxPropertyBlock empty;
    empty.appendDoubleArray({});
    checks(empty, 'd', {}, 0);
}
void boundaries()
{
    rejects([] { FbxPropertyBlock invalid(sizeof(FbxPropertyBlock)); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    rejects([] { FbxPropertyBlock invalid(256 * 1024 * 1024 + 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    FbxPropertyBlock exact(sizeof(FbxPropertyBlock) + 5);
    exact.appendString("");
    require(exact.payloadBytes() == sizeof(FbxPropertyBlock) + 5, "FBX exact quota accounting changed");
    const auto bytes = exact.bytes();
    rejects([&] { exact.appendBoolean(true); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    require(exact.bytes() == bytes && exact.propertyCount() == 1, "Failed FBX quota append changed output");
    FbxPropertyBlock arrayQuota(sizeof(FbxPropertyBlock) + 21);
    rejects([&] { arrayQuota.appendDoubleArray({ 1, 2 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    require(arrayQuota.bytes().empty() && arrayQuota.propertyCount() == 0, "Oversized FBX array changed empty owner");
    arrayQuota.appendDoubleArray({ 1 });
    require(arrayQuota.payloadBytes() == sizeof(FbxPropertyBlock) + 21, "FBX array exact quota changed");
    FbxPropertyBlock block;
    block.appendInt32(19);
    const auto original = block.bytes();
    for (double value : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
        rejects([&] { block.appendDouble(value); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { block.appendDoubleArray({ 1, value }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { block.appendFloat(static_cast<float>(value)); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { block.appendFloatArray({ 1, static_cast<float>(value) }); }, DUST3D_STATUS_INVALID_DOCUMENT);
    }
    require(block.bytes() == original && block.propertyCount() == 1, "Nonfinite FBX append changed prior output");
    for (size_t index = 1; index < FbxPropertyBlock::maximumPropertyCount; ++index)
        block.appendBoolean(false);
    const auto full = block.bytes();
    rejects([&] { block.appendString(""); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    require(block.bytes() == full && block.propertyCount() == FbxPropertyBlock::maximumPropertyCount, "FBX count limit changed prior output");
    auto moved = std::move(block);
    require(moved.bytes() == full && moved.propertyCount() == FbxPropertyBlock::maximumPropertyCount, "FBX property move lost ownership");
}
}
int main()
{
    try {
        scalarFixtures();
        arrayFixtures();
        boundaries();
        std::cout << "FBX property blocks passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

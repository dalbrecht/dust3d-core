// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerIndexedMesh.h>
#include <native/Engine/WorkerMeshValidation.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(IndexedMeshPayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const IndexedMeshPayloadError& cause) {
        require(cause.code() == code, "Indexed rejection retains original code");
        return;
    }
    throw std::runtime_error("Invalid indexed geometry accepted");
}
template <typename Action>
void rejectMesh(MeshPayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const MeshPayloadError& cause) {
        require(cause.code() == code, "Shared mesh validation retains original cause");
        return;
    }
    throw std::runtime_error("Invalid shared mesh value accepted");
}
IndexedMeshPayload sample()
{
    IndexedMeshPayload value;
    value.vertices = { { { -0.0F, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 0.5F } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } },
        { { 1, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.indices = { 0, 1, 2, 0, 2, 3 };
    value.nodeCount = 2;
    value.partCount = 1;
    value.warning = "original κ\t\n";
    return value;
}
void setField(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Test mutation fits readable storage");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
void ownershipAndQuotas()
{
    auto value = sample();
    auto bytes = encodeIndexedMeshPayload(value, {});
    const auto saved = bytes;
    const auto summary = inspectIndexedMeshPayload(bytes.data(), bytes.size(), {});
    require(summary.vertexCount == 4 && summary.indexCount == 6 && summary.nodeCount == 2 && summary.partCount == 1, "Indexed summary preserves unique table and corner dimensions");
    require(summary.logicalBytes == 4 * sizeof(MeshVertex) + 6 * sizeof(uint32_t) + value.warning.size(), "Logical retained charge is exact");
    IndexedMeshPayloadLimits exact;
    exact.maximumWireBytes = bytes.size();
    exact.maximumDecodedBytes = summary.logicalBytes;
    exact.maximumVertices = 4;
    exact.maximumIndices = 6;
    exact.maximumNodeCount = 2;
    exact.maximumPartCount = 1;
    exact.maximumWarningBytes = value.warning.size();
    auto copy = decodeIndexedMeshPayload(bytes.data(), bytes.size(), exact);
    require(encodeIndexedMeshPayload(copy, exact) == saved && copy.indices == value.indices && std::signbit(copy.vertices.at(0).position[0]), "Index order, attribute bits and negative zero survive exact quotas");
    value = {};
    bytes.assign(bytes.size(), 0);
    require(encodeIndexedMeshPayload(copy, {}) == saved, "Decoded arrays/text own storage independently");
    MeshPayload expanded;
    expanded.nodeCount = copy.nodeCount;
    expanded.partCount = copy.partCount;
    expanded.warning = copy.warning;
    for (const auto index : copy.indices)
        expanded.vertices.push_back(copy.vertices.at(index));
    const auto expandedWire = encodeMeshPayload(expanded, {});
    const auto expandedCopy = decodeMeshPayload(expandedWire.data(), expandedWire.size(), {});
    require(expandedCopy.vertices.size() == 6 && expandedCopy.vertices.at(3).position[0] == 0
            && expandedCopy.vertices.at(5).position[1] == 1 && expandedCopy.warning == copy.warning,
        "Shared validation preserves existing expanded triangle semantics");
    for (int field = 0; field < 6; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        if (field == 1)
            --limits.maximumDecodedBytes;
        if (field == 2)
            --limits.maximumVertices;
        if (field == 3)
            --limits.maximumIndices;
        if (field == 4)
            --limits.maximumNodeCount;
        if (field == 5)
            --limits.maximumWarningBytes;
        reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { encodeIndexedMeshPayload(copy, limits); });
        reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { decodeIndexedMeshPayload(saved.data(), saved.size(), limits); });
    }
    for (int field = 0; field < 14; ++field) {
        IndexedMeshPayloadLimits limits;
        if (field == 0)
            limits.maximumWireBytes = 0;
        if (field == 1)
            limits.maximumWireBytes = maximumPayloadBytes + 1;
        if (field == 2)
            limits.maximumDecodedBytes = 0;
        if (field == 3)
            limits.maximumDecodedBytes = maximumPayloadBytes + 1;
        if (field == 4)
            limits.maximumVertices = 0;
        if (field == 5)
            limits.maximumVertices = maximumMeshVertices + 1;
        if (field == 6)
            limits.maximumIndices = 0;
        if (field == 7)
            limits.maximumIndices = maximumPayloadBytes / 4 + 1;
        if (field == 8)
            limits.maximumNodeCount = 0;
        if (field == 9)
            limits.maximumNodeCount = maximumSourceCount + 1;
        if (field == 10)
            limits.maximumPartCount = 0;
        if (field == 11)
            limits.maximumPartCount = maximumSourceCount + 1;
        if (field == 12)
            limits.maximumWarningBytes = 0;
        if (field == 13)
            limits.maximumWarningBytes = maximumDiagnosticBytes + 1;
        reject(IndexedMeshPayloadErrorCode::invalidLimits, [&] { encodeIndexedMeshPayload(copy, limits); });
    }
}
void framingAndGeometry()
{
    const auto source = sample();
    const auto bytes = encodeIndexedMeshPayload(source, {});
    reject(IndexedMeshPayloadErrorCode::invalidInput, [&] { decodeIndexedMeshPayload(nullptr, bytes.size(), {}); });
    for (size_t size = 0; size < bytes.size(); ++size)
        reject(IndexedMeshPayloadErrorCode::truncated, [&] { decodeIndexedMeshPayload(bytes.data(), size, {}); });
    auto changed = bytes;
    changed.push_back(0);
    reject(IndexedMeshPayloadErrorCode::trailingData, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    for (const auto offset : { size_t { 0 }, size_t { 2 }, size_t { 4 }, size_t { 40 } }) {
        changed = bytes;
        setField(changed, offset, 2, offset < 4 ? 2 : 4);
        reject(offset == 0 ? IndexedMeshPayloadErrorCode::unsupportedVersion : IndexedMeshPayloadErrorCode::invalidLayout,
            [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    }
    for (const auto offset : { size_t { 8 }, size_t { 16 } }) {
        changed = bytes;
        setField(changed, offset, 0, 8);
        reject(IndexedMeshPayloadErrorCode::invalidGeometry, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
        changed = bytes;
        setField(changed, offset, UINT64_MAX, 8);
        reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    }
    changed = bytes;
    setField(changed, 16, 5, 8);
    reject(IndexedMeshPayloadErrorCode::invalidGeometry, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    const auto indicesOffset = indexedMeshPrefixBytes + source.warning.size() + source.vertices.size() * meshVertexBytes;
    changed = bytes;
    setField(changed, indicesOffset, 4, 4);
    reject(IndexedMeshPayloadErrorCode::invalidIndex, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    changed = bytes;
    setField(changed, indicesOffset + 5 * 4, UINT32_MAX, 4);
    reject(IndexedMeshPayloadErrorCode::invalidIndex, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    for (const auto offset : { size_t { 24 }, size_t { 32 } }) {
        changed = bytes;
        setField(changed, offset, maximumSourceCount + 1, 8);
        reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    }
    changed = bytes;
    setField(changed, 44, maximumDiagnosticBytes + 1, 4);
    reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    auto shortWire = IndexedMeshPayloadLimits {};
    shortWire.maximumWireBytes = 1;
    reject(IndexedMeshPayloadErrorCode::limitExceeded, [&] { encodeIndexedMeshPayload(source, shortWire); });
    auto invalid = source;
    invalid.indices.at(0) = UINT32_MAX;
    reject(IndexedMeshPayloadErrorCode::invalidIndex, [&] { encodeIndexedMeshPayload(invalid, {}); });
    invalid = source;
    invalid.indices.at(1) = 0;
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { encodeIndexedMeshPayload(invalid, {}); });
    changed = bytes;
    setField(changed, indicesOffset + 4, 0, 4);
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    // Even an unused vertex must satisfy the same geometry invariants.
    invalid = source;
    invalid.vertices.push_back(source.vertices.at(0));
    invalid.vertices.back().normal[2] = 0;
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { encodeIndexedMeshPayload(invalid, {}); });
    invalid.vertices.back().normal[2] = 1;
    auto unused = encodeIndexedMeshPayload(invalid, {});
    setField(unused, indexedMeshPrefixBytes + source.warning.size() + 4 * meshVertexBytes, 0x7fc00000, 4);
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { decodeIndexedMeshPayload(unused.data(), unused.size(), {}); });
    invalid = source;
    invalid.vertices.at(0).color[0] = 2;
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { encodeIndexedMeshPayload(invalid, {}); });
    invalid = source;
    invalid.vertices.at(0).position[0] = std::numeric_limits<float>::infinity();
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { encodeIndexedMeshPayload(invalid, {}); });
    changed = bytes;
    setField(changed, indexedMeshPrefixBytes + source.warning.size(), 0x7fc00000, 4);
    rejectMesh(MeshPayloadErrorCode::invalidGeometry, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    invalid = source;
    invalid.warning = std::string(1, '\0');
    rejectMesh(MeshPayloadErrorCode::invalidText, [&] { encodeIndexedMeshPayload(invalid, {}); });
    changed = bytes;
    changed.at(indexedMeshPrefixBytes) = 0;
    rejectMesh(MeshPayloadErrorCode::invalidText, [&] { decodeIndexedMeshPayload(changed.data(), changed.size(), {}); });
    detail::validateMeshWarning(nullptr, 0);
    rejectMesh(MeshPayloadErrorCode::invalidInput, [&] { detail::validateMeshWarning(nullptr, 1); });
}
}
int main()
{
    try {
        ownershipAndQuotas();
        framingAndGeometry();
        std::cout << "Indexed worker geometry contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerIndexedCopyBudget.h>
#include <native/Engine/WorkerIndexedSources.h>
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
void reject(IndexedSourcesPayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const IndexedSourcesPayloadError& cause) {
        require(cause.code() == code, "Indexed/source pairing preserves original cause");
        return;
    }
    throw std::runtime_error("Invalid indexed/source pair accepted");
}
IndexedSourcesPayload sample()
{
    IndexedSourcesPayload value;
    value.geometry.vertices = { { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } }, { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } },
        { { 1, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } }, { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 1, 1, 1 } } };
    value.geometry.indices = { 0, 1, 2, 0, 2, 3 };
    value.geometry.nodeCount = 1;
    value.geometry.partCount = 1;
    value.geometry.warning = "owned pair";
    value.sources.identities = { { "original-κ", "part" } };
    value.sources.influences = { { SourceHintKind::nodeHint, 0, 2.5F } };
    value.sources.corners.assign(6, { SourceHintKind::weightedHint, 0, 1 });
    return value;
}
void setField(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t width)
{
    require(offset <= bytes.size() && width <= bytes.size() - offset, "Test field fits storage");
    detail::writeUnsigned(bytes.data() + offset, value, width);
}
std::vector<uint8_t> independentFrame(const IndexedSourcesPayload& value)
{
    const auto mesh = encodeIndexedMeshPayload(value.geometry, {});
    const auto hints = encodeSourceHintPayload(value.sources, {});
    std::vector<uint8_t> wire(indexedSourcesPrefixBytes, 0);
    setField(wire, 0, 2, 2);
    setField(wire, 2, indexedSourcesPrefixBytes, 2);
    setField(wire, 8, mesh.size(), 8);
    setField(wire, 16, hints.size(), 8);
    wire.insert(wire.end(), mesh.begin(), mesh.end());
    wire.insert(wire.end(), hints.begin(), hints.end());
    return wire;
}
void ownershipAndLimits()
{
    auto value = sample();
    auto wire = encodeIndexedSourcesPayload(value, {});
    const auto saved = wire;
    const auto geometry = encodeIndexedMeshPayload(value.geometry, {});
    const auto mesh = inspectIndexedMeshPayload(geometry.data(), geometry.size(), {});
    const auto sources = measureSourceHintPayload(value.sources, {});
    IndexedSourcesPayloadLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = mesh.logicalBytes + sources.decodedBytes;
    auto copy = decodeIndexedSourcesPayload(wire.data(), wire.size(), exact);
    require(copy.geometry.vertices.size() == 4 && copy.geometry.indices.size() == 6 && copy.sources.corners.size() == 6, "Source corners match index positions, not unique vertices");
    require(encodeIndexedSourcesPayload(copy, exact) == saved, "Exact aggregate quotas retain full geometry/hints");
    value = {};
    wire.assign(wire.size(), 0);
    require(encodeIndexedSourcesPayload(copy, {}) == saved && copy.sources.influences.at(0).weight == 2.5F, "Complete independent pair owns all values after input release");
    for (int field = 0; field < 5; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        if (field == 1)
            --limits.maximumDecodedBytes;
        if (field == 2)
            limits.maximumWireBytes = 1;
        if (field == 3)
            limits.maximumWireBytes = indexedSourcesPrefixBytes;
        if (field == 4)
            limits.maximumDecodedBytes = mesh.logicalBytes - 1;
        reject(IndexedSourcesPayloadErrorCode::limitExceeded, [&] { encodeIndexedSourcesPayload(copy, limits); });
        reject(IndexedSourcesPayloadErrorCode::limitExceeded, [&] { decodeIndexedSourcesPayload(saved.data(), saved.size(), limits); });
    }
    for (int field = 0; field < 4; ++field) {
        auto limits = exact;
        if (field == 0)
            limits.maximumWireBytes = 0;
        if (field == 1)
            limits.maximumWireBytes = maximumPayloadBytes + 1;
        if (field == 2)
            limits.maximumDecodedBytes = 0;
        if (field == 3)
            limits.maximumDecodedBytes = maximumPayloadBytes + 1;
        reject(IndexedSourcesPayloadErrorCode::invalidLimits, [&] { encodeIndexedSourcesPayload(copy, limits); });
    }
}
void mismatchesAndFraming()
{
    auto value = sample();
    value.sources.corners.resize(3);
    reject(IndexedSourcesPayloadErrorCode::countMismatch, [&] { encodeIndexedSourcesPayload(value, {}); });
    auto bytes = independentFrame(value);
    reject(IndexedSourcesPayloadErrorCode::countMismatch, [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
    value = sample();
    value.geometry.nodeCount = 2;
    reject(IndexedSourcesPayloadErrorCode::countMismatch, [&] { encodeIndexedSourcesPayload(value, {}); });
    bytes = independentFrame(value);
    reject(IndexedSourcesPayloadErrorCode::countMismatch, [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
    const auto wire = encodeIndexedSourcesPayload(sample(), {});
    reject(IndexedSourcesPayloadErrorCode::invalidInput, [&] { decodeIndexedSourcesPayload(nullptr, wire.size(), {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(IndexedSourcesPayloadErrorCode::truncated, [&] { decodeIndexedSourcesPayload(wire.data(), size, {}); });
    bytes = wire;
    bytes.push_back(0);
    reject(IndexedSourcesPayloadErrorCode::trailingData, [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
    for (const auto offset : { size_t { 0 }, size_t { 2 }, size_t { 4 } }) {
        bytes = wire;
        setField(bytes, offset, 1, offset < 4 ? 2 : 4);
        reject(offset == 0 ? IndexedSourcesPayloadErrorCode::unsupportedVersion : IndexedSourcesPayloadErrorCode::invalidLayout,
            [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
    }
    for (const auto offset : { size_t { 8 }, size_t { 16 } }) {
        bytes = wire;
        setField(bytes, offset, 1, 8);
        reject(IndexedSourcesPayloadErrorCode::invalidLayout, [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
        bytes = wire;
        setField(bytes, offset, UINT64_MAX, 8);
        reject(IndexedSourcesPayloadErrorCode::limitExceeded, [&] { decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}); });
    }
    bytes = wire;
    setField(bytes, indexedSourcesPrefixBytes, 2, 2);
    try {
        decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {});
        throw std::runtime_error("Invalid inner indexed version accepted");
    } catch (const IndexedMeshPayloadError& cause) {
        require(cause.code() == IndexedMeshPayloadErrorCode::unsupportedVersion, "Original indexed codec cause survives pairing");
    }
    bytes = wire;
    const auto meshBytes = encodeIndexedMeshPayload(sample().geometry, {});
    setField(bytes, indexedSourcesPrefixBytes + meshBytes.size(), 2, 4);
    try {
        decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {});
        throw std::runtime_error("Invalid inner source version accepted");
    } catch (const SourceHintPayloadError& cause) {
        require(cause.code() == SourceHintPayloadErrorCode::unsupportedVersion, "Original source codec cause survives pairing");
    }
    bytes = wire;
    setField(bytes, indexedSourcesPrefixBytes + indexedMeshPrefixBytes + sample().geometry.warning.size() + 20, 0, 4);
    try {
        decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {});
        throw std::runtime_error("Invalid inner geometry accepted");
    } catch (const MeshPayloadError& cause) {
        require(cause.code() == MeshPayloadErrorCode::invalidGeometry, "Original shared geometry cause survives pairing");
    }
}
void pureIndexBudgets()
{
    IndexedMeshPayloadLimits exact;
    exact.maximumWireBytes = indexedMeshPrefixBytes + 10 + 4 * sizeof(MeshVertex) + 6 * sizeof(uint32_t);
    exact.maximumDecodedBytes = exact.maximumWireBytes - indexedMeshPrefixBytes;
    exact.maximumVertices = 4;
    require(calculateIndexedCopyBudget(6, 10, 2, 1, exact) == 4 * sizeof(MeshVertex) + 6 * sizeof(uint32_t), "Exact record budget excludes warning/prefix and retains vertex ceiling");
    auto less = exact;
    --less.maximumDecodedBytes;
    require(calculateIndexedCopyBudget(6, 10, 2, 1, less) == 3 * sizeof(MeshVertex) + 6 * sizeof(uint32_t), "Native allowance rounds down complete vertex records");
    for (int field = 0; field < 10; ++field) {
        auto limits = exact;
        uint64_t indices = 6, warning = 10, nodes = 2, parts = 1;
        if (field == 0)
            limits.maximumDecodedBytes = warning;
        if (field == 1)
            limits.maximumWireBytes = indexedMeshPrefixBytes + warning;
        if (field == 2)
            limits.maximumDecodedBytes = warning + indices * 4;
        if (field == 3)
            limits.maximumDecodedBytes = warning + indices * 4 + 1;
        if (field == 4)
            limits.maximumIndices = 3;
        if (field == 5)
            limits.maximumNodeCount = 1;
        if (field == 6)
            parts = maximumSourceCount + 1;
        if (field == 7)
            warning = maximumDiagnosticBytes + 1;
        if (field == 8)
            indices = 0;
        if (field == 9)
            indices = 1;
        bool rejected = false;
        try {
            calculateIndexedCopyBudget(indices, warning, nodes, parts, limits);
        } catch (const IndexedMeshPayloadError& cause) {
            rejected = cause.code() == (field >= 8 ? IndexedMeshPayloadErrorCode::invalidGeometry : IndexedMeshPayloadErrorCode::limitExceeded);
        }
        require(rejected, "Pure scalar export policy rejects before native indexing or allocation");
    }
}
}
int main()
{
    try {
        ownershipAndLimits();
        mismatchesAndFraming();
        pureIndexBudgets();
        std::cout << "Atomic indexed/source pairing passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

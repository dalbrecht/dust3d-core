// SPDX-License-Identifier: MIT
#include "WorkerIndexedMesh.h"
#include "WorkerMeshValidation.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Indexed worker spans must fit size_t.");
        [[noreturn]] void reject(IndexedMeshPayloadErrorCode code, const char* cause)
        {
            throw IndexedMeshPayloadError(code, cause);
        }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(IndexedMeshPayloadErrorCode::limitExceeded, "Indexed worker geometry exceeds its aggregate quota.");
            total += count * unit;
        }
        struct Layout {
            size_t bytes, vertices, indices;
            uint64_t logical;
        };
        Layout plan(uint64_t vertices, uint64_t indices, uint64_t nodes, uint64_t parts, uint64_t warning, const IndexedMeshPayloadLimits& limits)
        {
            validateIndexedMeshPayloadLimits(limits);
            if (!vertices || !indices || indices % 3)
                reject(IndexedMeshPayloadErrorCode::invalidGeometry, "Indexed worker geometry requires vertices and complete nonempty triangles.");
            if (vertices > limits.maximumVertices || indices > limits.maximumIndices
                || nodes > limits.maximumNodeCount || parts > limits.maximumPartCount || warning > limits.maximumWarningBytes)
                reject(IndexedMeshPayloadErrorCode::limitExceeded, "Indexed worker counts exceed configured limits.");
            uint64_t wire = indexedMeshPrefixBytes;
            charge(wire, warning, 1, limits.maximumWireBytes);
            const auto verticesOffset = wire;
            charge(wire, vertices, meshVertexBytes, limits.maximumWireBytes);
            const auto indicesOffset = wire;
            charge(wire, indices, 4, limits.maximumWireBytes);
            uint64_t logical = 0;
            charge(logical, warning, 1, limits.maximumDecodedBytes);
            charge(logical, vertices, sizeof(MeshVertex), limits.maximumDecodedBytes);
            charge(logical, indices, sizeof(uint32_t), limits.maximumDecodedBytes);
            return { static_cast<size_t>(wire), static_cast<size_t>(verticesOffset), static_cast<size_t>(indicesOffset), logical };
        }
        void validateIndex(uint64_t index, uint64_t vertices)
        {
            if (index >= vertices)
                reject(IndexedMeshPayloadErrorCode::invalidIndex, "Indexed worker corner references a missing vertex.");
        }
        struct Description {
            Layout layout;
            uint64_t vertices, indices, nodes, parts, warning;
        };
        Description describeIndexedMesh(const uint8_t* bytes, size_t size, const IndexedMeshPayloadLimits& limits)
        {
            validateIndexedMeshPayloadLimits(limits);
            if (!bytes)
                reject(IndexedMeshPayloadErrorCode::invalidInput, "Indexed worker geometry bytes are missing.");
            if (size < indexedMeshPrefixBytes)
                reject(IndexedMeshPayloadErrorCode::truncated, "Indexed worker geometry prefix is incomplete.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(IndexedMeshPayloadErrorCode::unsupportedVersion, "Indexed worker geometry version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != indexedMeshPrefixBytes || detail::readUnsigned(bytes + 4, 4)
                || detail::readUnsigned(bytes + 40, 4) != meshVertexBytes)
                reject(IndexedMeshPayloadErrorCode::invalidLayout, "Indexed worker prefix, flags or vertex layout is invalid.");
            const auto vertices = detail::readUnsigned(bytes + 8, 8);
            const auto indices = detail::readUnsigned(bytes + 16, 8);
            const auto nodes = detail::readUnsigned(bytes + 24, 8);
            const auto parts = detail::readUnsigned(bytes + 32, 8);
            const auto warning = detail::readUnsigned(bytes + 44, 4);
            const auto layout = plan(vertices, indices, nodes, parts, warning, limits);
            if (size < layout.bytes)
                reject(IndexedMeshPayloadErrorCode::truncated, "Indexed worker geometry is incomplete.");
            if (size > layout.bytes)
                reject(IndexedMeshPayloadErrorCode::trailingData, "Indexed worker geometry has trailing bytes.");
            detail::validateMeshWarning(bytes + indexedMeshPrefixBytes, static_cast<size_t>(warning));
            for (uint64_t index = 0; index < vertices; ++index)
                detail::validateMeshVertex(detail::readMeshVertex(bytes + layout.vertices + index * meshVertexBytes));
            for (uint64_t index = 0; index < indices; index += 3) {
                std::array<MeshVertex, 3> triangle;
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto vertex = detail::readUnsigned(bytes + layout.indices + (index + corner) * 4, 4);
                    validateIndex(vertex, vertices);
                    triangle[corner] = detail::readMeshVertex(bytes + layout.vertices + vertex * meshVertexBytes);
                }
                detail::validateMeshTriangle(triangle);
            }
            return { layout, vertices, indices, nodes, parts, warning };
        }
    }
    IndexedMeshPayloadError::IndexedMeshPayloadError(IndexedMeshPayloadErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }
    void validateIndexedMeshPayloadLimits(const IndexedMeshPayloadLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumVertices || limits.maximumVertices > maximumMeshVertices
            || !limits.maximumIndices || limits.maximumIndices > maximumMeshIndices
            || !limits.maximumNodeCount || limits.maximumNodeCount > maximumSourceCount
            || !limits.maximumPartCount || limits.maximumPartCount > maximumSourceCount
            || !limits.maximumWarningBytes || limits.maximumWarningBytes > maximumDiagnosticBytes)
            reject(IndexedMeshPayloadErrorCode::invalidLimits, "Indexed worker policy is outside supported positive bounds.");
    }
    std::vector<uint8_t> encodeIndexedMeshPayload(const IndexedMeshPayload& value, const IndexedMeshPayloadLimits& limits)
    {
        const auto layout = plan(value.vertices.size(), value.indices.size(), value.nodeCount, value.partCount, value.warning.size(), limits);
        detail::validateMeshWarning(reinterpret_cast<const uint8_t*>(value.warning.data()), value.warning.size());
        for (const auto& vertex : value.vertices)
            detail::validateMeshVertex(vertex);
        for (const auto index : value.indices)
            validateIndex(index, value.vertices.size());
        for (size_t index = 0; index < value.indices.size(); index += 3) {
            const std::array<MeshVertex, 3> triangle { value.vertices[value.indices[index]], value.vertices[value.indices[index + 1]], value.vertices[value.indices[index + 2]] };
            detail::validateMeshTriangle(triangle);
        }
        std::vector<uint8_t> bytes(layout.bytes, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, indexedMeshPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, value.vertices.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, value.indices.size(), 8);
        detail::writeUnsigned(bytes.data() + 24, value.nodeCount, 8);
        detail::writeUnsigned(bytes.data() + 32, value.partCount, 8);
        detail::writeUnsigned(bytes.data() + 40, meshVertexBytes, 4);
        detail::writeUnsigned(bytes.data() + 44, value.warning.size(), 4);
        std::copy(value.warning.begin(), value.warning.end(), bytes.begin() + indexedMeshPrefixBytes);
        size_t offset = layout.vertices;
        for (const auto& vertex : value.vertices) {
            detail::writeMeshVertex(bytes.data() + offset, vertex);
            offset += meshVertexBytes;
        }
        offset = layout.indices;
        for (const auto index : value.indices) {
            detail::writeUnsigned(bytes.data() + offset, index, 4);
            offset += 4;
        }
        return bytes;
    }
    IndexedMeshPayloadInspection inspectIndexedMeshPayload(const uint8_t* bytes, size_t size, const IndexedMeshPayloadLimits& limits)
    {
        const auto value = describeIndexedMesh(bytes, size, limits);
        return { value.vertices, value.indices, value.nodes, value.parts, value.layout.logical };
    }
    IndexedMeshPayload decodeIndexedMeshPayload(const uint8_t* bytes, size_t size, const IndexedMeshPayloadLimits& limits)
    {
        const auto value = describeIndexedMesh(bytes, size, limits);
        IndexedMeshPayload result;
        result.nodeCount = value.nodes;
        result.partCount = value.parts;
        result.warning.assign(reinterpret_cast<const char*>(bytes + indexedMeshPrefixBytes), static_cast<size_t>(value.warning));
        result.vertices.reserve(static_cast<size_t>(value.vertices));
        for (uint64_t index = 0; index < value.vertices; ++index)
            result.vertices.push_back(detail::readMeshVertex(bytes + value.layout.vertices + index * meshVertexBytes));
        result.indices.reserve(static_cast<size_t>(value.indices));
        for (uint64_t index = 0; index < value.indices; ++index)
            result.indices.push_back(static_cast<uint32_t>(detail::readUnsigned(bytes + value.layout.indices + index * 4, 4)));
        return result;
    }
}
}

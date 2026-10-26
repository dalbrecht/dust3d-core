// SPDX-License-Identifier: MIT
#include "WorkerIndexedSources.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Paired source spans must fit size_t.");
        [[noreturn]] void reject(IndexedSourcesPayloadErrorCode code, const char* cause)
        {
            throw IndexedSourcesPayloadError(code, cause);
        }
        size_t calculateWireSize(uint64_t mesh, uint64_t sources, const IndexedSourcesPayloadLimits& limits)
        {
            if (mesh < indexedMeshPrefixBytes || sources < sourceHintPrefixBytes)
                reject(IndexedSourcesPayloadErrorCode::invalidLayout, "Indexed/source pair subspans lack complete prefixes.");
            if (indexedSourcesPrefixBytes > limits.maximumWireBytes
                || mesh > limits.maximumWireBytes - indexedSourcesPrefixBytes
                || sources > limits.maximumWireBytes - indexedSourcesPrefixBytes - mesh)
                reject(IndexedSourcesPayloadErrorCode::limitExceeded, "Indexed/source pair exceeds its aggregate wire quota.");
            return static_cast<size_t>(indexedSourcesPrefixBytes + mesh + sources);
        }
        void validateDecodedSize(uint64_t mesh, uint64_t sources, const IndexedSourcesPayloadLimits& limits)
        {
            if (mesh > limits.maximumDecodedBytes || sources > limits.maximumDecodedBytes - mesh)
                reject(IndexedSourcesPayloadErrorCode::limitExceeded, "Indexed/source pair exceeds its aggregate logical quota.");
        }
        void validateCorrespondence(uint64_t indices, uint64_t nodes, uint64_t corners, uint64_t identities)
        {
            if (indices != corners || nodes != identities)
                reject(IndexedSourcesPayloadErrorCode::countMismatch, "Indexed/source pair geometry and source counts disagree.");
        }
    }

    IndexedSourcesPayloadError::IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }

    void validateIndexedSourcesPayloadLimits(const IndexedSourcesPayloadLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
            reject(IndexedSourcesPayloadErrorCode::invalidLimits, "Indexed/source pair limits are outside supported positive bounds.");
        validateIndexedMeshPayloadLimits(limits.geometry);
        validateSourceHintPayloadLimits(limits.sources);
    }

    std::vector<uint8_t> encodeIndexedSourcesPayload(const IndexedSourcesPayload& value, const IndexedSourcesPayloadLimits& limits)
    {
        validateIndexedSourcesPayloadLimits(limits);
        (void)calculateWireSize(indexedMeshPrefixBytes, sourceHintPrefixBytes, limits);
        const auto mesh = encodeIndexedMeshPayload(value.geometry, limits.geometry);
        const auto geometry = inspectIndexedMeshPayload(mesh.data(), mesh.size(), limits.geometry);
        const auto measuredSources = measureSourceHintPayload(value.sources, limits.sources);
        const auto size = calculateWireSize(mesh.size(), measuredSources.wireBytes, limits);
        validateDecodedSize(geometry.logicalBytes, measuredSources.decodedBytes, limits);
        validateCorrespondence(geometry.indexCount, geometry.nodeCount, value.sources.corners.size(), value.sources.identities.size());
        const auto sources = encodeSourceHintPayload(value.sources, limits.sources);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 2, 2);
        detail::writeUnsigned(bytes.data() + 2, indexedSourcesPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, mesh.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, sources.size(), 8);
        std::copy(mesh.begin(), mesh.end(), bytes.begin() + indexedSourcesPrefixBytes);
        std::copy(sources.begin(), sources.end(), bytes.begin() + indexedSourcesPrefixBytes + mesh.size());
        return bytes;
    }

    IndexedSourcesPayload decodeIndexedSourcesPayload(const uint8_t* bytes, size_t size, const IndexedSourcesPayloadLimits& limits)
    {
        validateIndexedSourcesPayloadLimits(limits);
        if (!bytes)
            reject(IndexedSourcesPayloadErrorCode::invalidInput, "Indexed/source pair bytes are missing.");
        if (size < indexedSourcesPrefixBytes)
            reject(IndexedSourcesPayloadErrorCode::truncated, "Indexed/source pair prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 2)
            reject(IndexedSourcesPayloadErrorCode::unsupportedVersion, "Indexed/source pair version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != indexedSourcesPrefixBytes || detail::readUnsigned(bytes + 4, 4))
            reject(IndexedSourcesPayloadErrorCode::invalidLayout, "Indexed/source pair prefix or reserved field is invalid.");
        const auto meshSize = detail::readUnsigned(bytes + 8, 8), sourceSize = detail::readUnsigned(bytes + 16, 8);
        const auto total = calculateWireSize(meshSize, sourceSize, limits);
        if (size < total)
            reject(IndexedSourcesPayloadErrorCode::truncated, "Indexed/source pair subspans are incomplete.");
        if (size != total)
            reject(IndexedSourcesPayloadErrorCode::trailingData, "Indexed/source pair has trailing bytes.");
        const auto* mesh = bytes + indexedSourcesPrefixBytes;
        const auto* source = mesh + static_cast<size_t>(meshSize);
        const auto geometry = inspectIndexedMeshPayload(mesh, static_cast<size_t>(meshSize), limits.geometry);
        const auto sources = inspectSourceHintPayload(source, static_cast<size_t>(sourceSize), limits.sources);
        validateDecodedSize(geometry.logicalBytes, sources.logicalBytes, limits);
        validateCorrespondence(geometry.indexCount, geometry.nodeCount, sources.cornerCount, sources.identityCount);
        return { decodeIndexedMeshPayload(mesh, static_cast<size_t>(meshSize), limits.geometry),
            decodeSourceHintPayload(source, static_cast<size_t>(sourceSize), limits.sources) };
    }
}
}

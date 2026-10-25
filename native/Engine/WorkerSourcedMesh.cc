// SPDX-License-Identifier: MIT
#include "WorkerSourcedMesh.h"
#include "WorkerWire.h"
#include <algorithm>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Paired source spans must fit size_t.");
        [[noreturn]] void reject(SourcedMeshPayloadErrorCode code, const char* cause)
        {
            throw SourcedMeshPayloadError(code, cause);
        }
        size_t calculateWireSize(uint64_t mesh, uint64_t sources, const SourcedMeshPayloadLimits& limits)
        {
            if (mesh < meshPrefixBytes || sources < sourceHintPrefixBytes)
                reject(SourcedMeshPayloadErrorCode::invalidLayout, "Sourced mesh subspans lack complete prefixes.");
            if (sourcedMeshPrefixBytes > limits.maximumWireBytes
                || mesh > limits.maximumWireBytes - sourcedMeshPrefixBytes
                || sources > limits.maximumWireBytes - sourcedMeshPrefixBytes - mesh)
                reject(SourcedMeshPayloadErrorCode::limitExceeded, "Sourced mesh exceeds its aggregate wire quota.");
            return static_cast<size_t>(sourcedMeshPrefixBytes + mesh + sources);
        }
        void validateDecodedSize(uint64_t mesh, uint64_t sources, const SourcedMeshPayloadLimits& limits)
        {
            if (mesh > limits.maximumDecodedBytes || sources > limits.maximumDecodedBytes - mesh)
                reject(SourcedMeshPayloadErrorCode::limitExceeded, "Sourced mesh exceeds its aggregate logical quota.");
        }
        void validateCorrespondence(uint64_t vertices, uint64_t nodes, uint64_t corners, uint64_t identities)
        {
            if (vertices != corners || nodes != identities)
                reject(SourcedMeshPayloadErrorCode::countMismatch, "Sourced mesh geometry and source counts disagree.");
        }
    }

    SourcedMeshPayloadError::SourcedMeshPayloadError(SourcedMeshPayloadErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }

    void validateSourcedMeshPayloadLimits(const SourcedMeshPayloadLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes)
            reject(SourcedMeshPayloadErrorCode::invalidLimits, "Sourced mesh limits are outside supported positive bounds.");
        validateMeshPayloadLimits(limits.geometry);
        validateSourceHintPayloadLimits(limits.sources);
    }

    std::vector<uint8_t> encodeSourcedMeshPayload(const SourcedMeshPayload& value, const SourcedMeshPayloadLimits& limits)
    {
        validateSourcedMeshPayloadLimits(limits);
        (void)calculateWireSize(meshPrefixBytes, sourceHintPrefixBytes, limits);
        const auto mesh = encodeMeshPayload(value.geometry, limits.geometry);
        const auto geometry = inspectMeshPayload(mesh.data(), mesh.size(), limits.geometry);
        const auto measuredSources = measureSourceHintPayload(value.sources, limits.sources);
        const auto size = calculateWireSize(mesh.size(), measuredSources.wireBytes, limits);
        validateDecodedSize(geometry.logicalBytes, measuredSources.decodedBytes, limits);
        validateCorrespondence(geometry.vertexCount, geometry.nodeCount, value.sources.corners.size(), value.sources.identities.size());
        const auto sources = encodeSourceHintPayload(value.sources, limits.sources);
        std::vector<uint8_t> bytes(size, 0);
        detail::writeUnsigned(bytes.data(), 1, 2);
        detail::writeUnsigned(bytes.data() + 2, sourcedMeshPrefixBytes, 2);
        detail::writeUnsigned(bytes.data() + 8, mesh.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, sources.size(), 8);
        std::copy(mesh.begin(), mesh.end(), bytes.begin() + sourcedMeshPrefixBytes);
        std::copy(sources.begin(), sources.end(), bytes.begin() + sourcedMeshPrefixBytes + mesh.size());
        return bytes;
    }

    SourcedMeshPayload decodeSourcedMeshPayload(const uint8_t* bytes, size_t size, const SourcedMeshPayloadLimits& limits)
    {
        validateSourcedMeshPayloadLimits(limits);
        if (!bytes)
            reject(SourcedMeshPayloadErrorCode::invalidInput, "Sourced mesh bytes are missing.");
        if (size < sourcedMeshPrefixBytes)
            reject(SourcedMeshPayloadErrorCode::truncated, "Sourced mesh prefix is incomplete.");
        if (detail::readUnsigned(bytes, 2) != 1)
            reject(SourcedMeshPayloadErrorCode::unsupportedVersion, "Sourced mesh version is unsupported.");
        if (detail::readUnsigned(bytes + 2, 2) != sourcedMeshPrefixBytes || detail::readUnsigned(bytes + 4, 4))
            reject(SourcedMeshPayloadErrorCode::invalidLayout, "Sourced mesh prefix or reserved field is invalid.");
        const auto meshSize = detail::readUnsigned(bytes + 8, 8), sourceSize = detail::readUnsigned(bytes + 16, 8);
        const auto total = calculateWireSize(meshSize, sourceSize, limits);
        if (size < total)
            reject(SourcedMeshPayloadErrorCode::truncated, "Sourced mesh subspans are incomplete.");
        if (size != total)
            reject(SourcedMeshPayloadErrorCode::trailingData, "Sourced mesh has trailing bytes.");
        const auto* mesh = bytes + sourcedMeshPrefixBytes;
        const auto* source = mesh + static_cast<size_t>(meshSize);
        const auto geometry = inspectMeshPayload(mesh, static_cast<size_t>(meshSize), limits.geometry);
        const auto sources = inspectSourceHintPayload(source, static_cast<size_t>(sourceSize), limits.sources);
        validateDecodedSize(geometry.logicalBytes, sources.logicalBytes, limits);
        validateCorrespondence(geometry.vertexCount, geometry.nodeCount, sources.cornerCount, sources.identityCount);
        return { decodeMeshPayload(mesh, static_cast<size_t>(meshSize), limits.geometry),
            decodeSourceHintPayload(source, static_cast<size_t>(sourceSize), limits.sources) };
    }
}
}

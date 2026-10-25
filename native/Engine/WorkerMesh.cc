// SPDX-License-Identifier: MIT
#include "WorkerMesh.h"
#include "WorkerWire.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "Worker mesh wire format requires IEEE-754 binary32.");
        [[noreturn]] void reject(MeshPayloadErrorCode code, const char* message)
        {
            throw MeshPayloadError(code, message);
        }
        void validateLimits(const MeshPayloadLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
                || !limits.maximumVertices || limits.maximumVertices > maximumMeshVertices
                || !limits.maximumNodeCount || limits.maximumNodeCount > maximumSourceCount
                || !limits.maximumPartCount || limits.maximumPartCount > maximumSourceCount
                || !limits.maximumWarningBytes || limits.maximumWarningBytes > maximumDiagnosticBytes)
                reject(MeshPayloadErrorCode::invalidLimits, "Worker mesh limits are outside their supported range.");
        }
        struct Layout {
            size_t bytes;
            size_t vertices;
        };
        Layout plan(uint64_t vertices, uint64_t nodes, uint64_t parts, uint64_t warning, const MeshPayloadLimits& limits)
        {
            validateLimits(limits);
            if (!vertices || vertices % 3)
                reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh requires complete nonempty triangles.");
            if (vertices > limits.maximumVertices || nodes > limits.maximumNodeCount || parts > limits.maximumPartCount || warning > limits.maximumWarningBytes)
                reject(MeshPayloadErrorCode::limitExceeded, "Worker mesh counts exceed their configured limits.");
            if (warning > limits.maximumDecodedBytes || vertices > (limits.maximumDecodedBytes - warning) / sizeof(MeshVertex))
                reject(MeshPayloadErrorCode::limitExceeded, "Worker decoded mesh exceeds its logical output quota.");
            if (meshPrefixBytes + warning > limits.maximumWireBytes || vertices > (limits.maximumWireBytes - meshPrefixBytes - warning) / meshVertexBytes)
                reject(MeshPayloadErrorCode::limitExceeded, "Worker mesh exceeds its wire payload quota.");
            const auto bytes = meshPrefixBytes + warning + vertices * meshVertexBytes;
            if (bytes > std::numeric_limits<size_t>::max())
                reject(MeshPayloadErrorCode::limitExceeded, "Worker mesh cannot fit the address space.");
            return { static_cast<size_t>(bytes), static_cast<size_t>(meshPrefixBytes + warning) };
        }
        void validateText(const uint8_t* text, size_t size)
        {
            size_t index = 0;
            while (index < size) {
                const auto first = text[index];
                if (first < 128) {
                    if ((first < 32 && first != 9 && first != 10 && first != 13) || first == 127)
                        reject(MeshPayloadErrorCode::invalidText, "Worker warning contains unsupported control characters.");
                    ++index;
                    continue;
                }
                const size_t width = first >= 0xc2 && first <= 0xdf ? 2 : first >= 0xe0 && first <= 0xef ? 3
                    : first >= 0xf0 && first <= 0xf4                                                     ? 4
                                                                                                         : 0;
                if (!width || width > size - index)
                    reject(MeshPayloadErrorCode::invalidText, "Worker warning has invalid UTF-8 storage.");
                uint32_t scalar = first & ((1U << (7 - width)) - 1);
                for (size_t byte = 1; byte < width; ++byte) {
                    const auto next = text[index + byte];
                    if ((next & 0xc0) != 0x80)
                        reject(MeshPayloadErrorCode::invalidText, "Worker warning has an invalid UTF-8 continuation.");
                    scalar = (scalar << 6) | (next & 0x3f);
                }
                const uint32_t minimum = width == 2 ? 0x80 : width == 3 ? 0x800
                                                                        : 0x10000;
                if (scalar < minimum || scalar > 0x10ffff || (scalar >= 0xd800 && scalar <= 0xdfff))
                    reject(MeshPayloadErrorCode::invalidText, "Worker warning has an invalid Unicode scalar.");
                index += width;
            }
        }
        bool finiteFloat(float value)
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return (bits & 0x7f800000U) != 0x7f800000U;
        }
        void validateTriangle(const MeshVertex* vertices)
        {
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& vertex = vertices[corner];
                double normalLengthSquared = 0;
                for (size_t axis = 0; axis < 3; ++axis) {
                    if (!finiteFloat(vertex.position[axis]) || !finiteFloat(vertex.normal[axis]))
                        reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh contains nonfinite geometry.");
                    normalLengthSquared += static_cast<double>(vertex.normal[axis]) * vertex.normal[axis];
                }
                if (std::abs(normalLengthSquared - 1) > 0.001)
                    reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh normals must have unit length.");
                for (const auto value : vertex.color)
                    if (!finiteFloat(value) || value < 0 || value > 1)
                        reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh colors must be finite unit values.");
            }
            std::array<double, 3> first;
            std::array<double, 3> second;
            for (size_t axis = 0; axis < 3; ++axis) {
                first[axis] = static_cast<double>(vertices[1].position[axis]) - vertices[0].position[axis];
                second[axis] = static_cast<double>(vertices[2].position[axis]) - vertices[0].position[axis];
            }
            const std::array<double, 3> cross { first[1] * second[2] - first[2] * second[1], first[2] * second[0] - first[0] * second[2], first[0] * second[1] - first[1] * second[0] };
            const double area = cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
            if (!std::isfinite(area) || area <= 0)
                reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh contains a degenerate triangle.");
        }
        void writeFloat(uint8_t* output, float value)
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            detail::writeUnsigned(output, bits, 4);
        }
        float readFloat(const uint8_t* input)
        {
            const auto bits = static_cast<uint32_t>(detail::readUnsigned(input, 4));
            // Reject nonfinite wire bits before constructing any floating-point operand.
            if ((bits & 0x7f800000U) == 0x7f800000U)
                reject(MeshPayloadErrorCode::invalidGeometry, "Worker mesh wire values must be finite.");
            float value;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }
        MeshVertex readVertex(const uint8_t* input)
        {
            MeshVertex result;
            for (auto* values : { &result.position, &result.normal })
                for (auto& value : *values) {
                    value = readFloat(input);
                    input += 4;
                }
            for (auto& value : result.color) {
                value = readFloat(input);
                input += 4;
            }
            return result;
        }
    }
    MeshPayloadError::MeshPayloadError(MeshPayloadErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    void validateMeshPayloadLimits(const MeshPayloadLimits& limits)
    {
        validateLimits(limits);
    }
    std::vector<uint8_t> encodeMeshPayload(const MeshPayload& mesh, const MeshPayloadLimits& limits)
    {
        const auto layout = plan(mesh.vertices.size(), mesh.nodeCount, mesh.partCount, mesh.warning.size(), limits);
        validateText(reinterpret_cast<const uint8_t*>(mesh.warning.data()), mesh.warning.size());
        for (size_t index = 0; index < mesh.vertices.size(); index += 3)
            validateTriangle(mesh.vertices.data() + index);
        std::vector<uint8_t> result(layout.bytes);
        detail::writeUnsigned(result.data(), 1, 2);
        detail::writeUnsigned(result.data() + 2, meshPrefixBytes, 2);
        detail::writeUnsigned(result.data() + 8, mesh.vertices.size(), 8);
        detail::writeUnsigned(result.data() + 16, mesh.nodeCount, 8);
        detail::writeUnsigned(result.data() + 24, mesh.partCount, 8);
        detail::writeUnsigned(result.data() + 32, meshVertexBytes, 4);
        detail::writeUnsigned(result.data() + 36, mesh.warning.size(), 4);
        std::copy(mesh.warning.begin(), mesh.warning.end(), result.begin() + meshPrefixBytes);
        size_t offset = layout.vertices;
        for (const auto& vertex : mesh.vertices) {
            for (const auto* values : { &vertex.position, &vertex.normal })
                for (const auto value : *values) {
                    writeFloat(result.data() + offset, value);
                    offset += 4;
                }
            for (const auto value : vertex.color) {
                writeFloat(result.data() + offset, value);
                offset += 4;
            }
        }
        return result;
    }
    namespace {
        struct ParsedMesh {
            Layout layout;
            uint64_t count, nodes, parts, warning;
        };
        ParsedMesh describeMesh(const uint8_t* bytes, size_t size, const MeshPayloadLimits& limits)
        {
            validateLimits(limits);
            if (size < meshPrefixBytes)
                reject(MeshPayloadErrorCode::truncated, "Worker mesh prefix is incomplete.");
            if (!bytes)
                reject(MeshPayloadErrorCode::invalidInput, "Worker mesh bytes are missing.");
            if (detail::readUnsigned(bytes, 2) != 1)
                reject(MeshPayloadErrorCode::unsupportedVersion, "Worker mesh schema version is unsupported.");
            if (detail::readUnsigned(bytes + 2, 2) != meshPrefixBytes || detail::readUnsigned(bytes + 4, 4) || detail::readUnsigned(bytes + 32, 4) != meshVertexBytes)
                reject(MeshPayloadErrorCode::invalidLayout, "Worker mesh prefix, flags or vertex layout is invalid.");
            const auto count = detail::readUnsigned(bytes + 8, 8);
            const auto nodes = detail::readUnsigned(bytes + 16, 8);
            const auto parts = detail::readUnsigned(bytes + 24, 8);
            const auto warning = detail::readUnsigned(bytes + 36, 4);
            const auto layout = plan(count, nodes, parts, warning, limits);
            if (size < layout.bytes)
                reject(MeshPayloadErrorCode::truncated, "Worker mesh payload is incomplete.");
            if (size != layout.bytes)
                reject(MeshPayloadErrorCode::trailingData, "Worker mesh payload contains trailing bytes.");
            validateText(bytes + meshPrefixBytes, static_cast<size_t>(warning));
            size_t offset = layout.vertices;
            for (uint64_t index = 0; index < count; index += 3) {
                std::array<MeshVertex, 3> triangle;
                for (auto& vertex : triangle) {
                    vertex = readVertex(bytes + offset);
                    offset += meshVertexBytes;
                }
                validateTriangle(triangle.data());
            }
            return { layout, count, nodes, parts, warning };
        }
    }
    MeshPayloadInspection inspectMeshPayload(const uint8_t* bytes, size_t size, const MeshPayloadLimits& limits)
    {
        const auto parsed = describeMesh(bytes, size, limits);
        return { parsed.count, parsed.count * sizeof(MeshVertex) + parsed.warning, parsed.nodes };
    }
    MeshPayload decodeMeshPayload(const uint8_t* bytes, size_t size, const MeshPayloadLimits& limits)
    {
        const auto parsed = describeMesh(bytes, size, limits);
        const auto& layout = parsed.layout;
        const auto count = parsed.count, nodes = parsed.nodes, parts = parsed.parts, warning = parsed.warning;
        MeshPayload result;
        result.nodeCount = nodes;
        result.partCount = parts;
        result.vertices.reserve(static_cast<size_t>(count));
        size_t offset = layout.vertices;
        for (uint64_t index = 0; index < count; index += 3) {
            std::array<MeshVertex, 3> triangle;
            for (auto& vertex : triangle) {
                vertex = readVertex(bytes + offset);
                offset += meshVertexBytes;
            }
            result.vertices.insert(result.vertices.end(), triangle.begin(), triangle.end());
        }
        result.warning.assign(reinterpret_cast<const char*>(bytes + meshPrefixBytes), static_cast<size_t>(warning));
        return result;
    }
}
}

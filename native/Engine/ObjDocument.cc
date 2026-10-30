// SPDX-License-Identifier: MIT
#include "ObjDocument.h"
#include "Cancellation.h"
#include "EngineInternal.h"
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
namespace dust3d {
namespace native {
    namespace {
        void validate(const Object& source)
        {
            for (const auto& vertex : source.vertices) {
                for (size_t axis = 0; axis < 3; ++axis) {
                    if (!std::isfinite(vertex[axis]))
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "OBJ coordinates must be finite.");
                    if (std::abs(vertex[axis]) > 4000000)
                        throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ coordinate exceeds bounds.");
                }
            }
            for (const auto& face : source.triangleAndQuads) {
                if (face.size() != 3 && face.size() != 4)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "OBJ source faces must be triangles or quads.");
                for (const auto index : face) {
                    if (index >= source.vertices.size())
                        throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "OBJ polygon index exceeds source vertices.");
                }
            }
        }
        template <typename Append>
        void encode(const Object& source, Append append)
        {
            append(std::string(objDocumentHeader));
            for (const auto& vertex : source.vertices) {
                std::ostringstream line;
                line.exceptions(std::ios::badbit | std::ios::failbit);
                line.imbue(std::locale::classic());
                line << std::setprecision(std::numeric_limits<double>::max_digits10)
                     << "v " << vertex.x() << ' ' << vertex.y() << ' ' << vertex.z() << '\n';
                append(line.str());
            }
            for (const auto& face : source.triangleAndQuads) {
                std::ostringstream line;
                line.exceptions(std::ios::badbit | std::ios::failbit);
                line.imbue(std::locale::classic());
                line << 'f';
                for (const auto index : face)
                    line << ' ' << static_cast<uint64_t>(index) + 1;
                line << '\n';
                append(line.str());
            }
        }
    }
    OwnedObjDocument encodeObjDocument(const Object& source, size_t maximumBytes)
    {
        const auto plan = planObjGeometry(source.vertices.size(), source.triangleAndQuads.size(), maximumBytes);
        validate(source);
        const auto available = plan.maximumPayloadBytes - sizeof(OwnedObjDocument);
        size_t count = 0;
        encode(source, [&](const std::string& line) {
            if (line.size() > available - count)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ text exceeds retained output quota.");
            count += line.size();
        });
        std::vector<uint8_t> bytes(count);
        if (bytes.capacity() > available)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ buffer capacity exceeds output quota.");
        size_t offset = 0;
        encode(source, [&](const std::string& line) {
            if (line.size() > bytes.size() - offset)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "OBJ source changed during encoding.");
            std::memcpy(bytes.data() + offset, line.data(), line.size());
            offset += line.size();
        });
        if (offset != count)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "OBJ encoded size changed between passes.");
        return OwnedObjDocument(std::move(bytes));
    }
    OwnedObjDocument encodeIndexedObjDocument(const Dust3DIndexedMesh& source, size_t maximumBytes, const Dust3DCancellationToken* cancellation)
    {
        checkCancellation(cancellation);
        if (source.indices.empty() || source.indices.size() % 3 != 0)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Indexed OBJ requires complete nonempty triangles.");
        const auto plan = planObjGeometry(source.vertices.size(), source.indices.size() / 3, maximumBytes);
        for (size_t index = 0; index < source.vertices.size(); ++index) {
            if (index % 256 == 0)
                checkCancellation(cancellation);
            const auto& vertex = source.vertices[index];
            for (size_t axis = 0; axis < 3; ++axis)
                if (!std::isfinite(vertex.position[axis]) || !std::isfinite(vertex.normal[axis]))
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Indexed OBJ positions and normals must be finite.");
        }
        for (size_t index = 0; index < source.indices.size(); ++index) {
            if (index % 768 == 0)
                checkCancellation(cancellation);
            if (source.indices[index] >= source.vertices.size())
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Indexed OBJ face index exceeds its vertices.");
        }
        const auto write = [&](auto append) {
            append(std::string(objDocumentHeader));
            for (size_t index = 0; index < source.vertices.size(); ++index) {
                if (index % 256 == 0)
                    checkCancellation(cancellation);
                const auto& vertex = source.vertices[index];
                std::ostringstream line;
                line.exceptions(std::ios::badbit | std::ios::failbit);
                line.imbue(std::locale::classic());
                line << std::setprecision(std::numeric_limits<float>::max_digits10)
                     << "v " << vertex.position[0] << ' ' << vertex.position[1] << ' ' << vertex.position[2] << '\n'
                     << "vn " << vertex.normal[0] << ' ' << vertex.normal[1] << ' ' << vertex.normal[2] << '\n';
                append(line.str());
            }
            for (size_t index = 0; index < source.indices.size(); index += 3) {
                if (index % 768 == 0)
                    checkCancellation(cancellation);
                std::ostringstream line;
                line.exceptions(std::ios::badbit | std::ios::failbit);
                line.imbue(std::locale::classic());
                line << 'f';
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto oneBased = uint64_t(source.indices[index + corner]) + 1;
                    line << ' ' << oneBased << "//" << oneBased;
                }
                line << '\n';
                append(line.str());
            }
        };
        const auto available = plan.maximumPayloadBytes - sizeof(OwnedObjDocument);
        size_t count = 0;
        write([&](const std::string& line) {
            if (line.size() > available - count)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ text exceeds its output quota.");
            count += line.size();
        });
        std::vector<uint8_t> bytes(count);
        if (bytes.capacity() > available)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ buffer capacity exceeds its output quota.");
        size_t offset = 0;
        write([&](const std::string& line) {
            if (line.size() > bytes.size() - offset)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Indexed OBJ source changed during encoding.");
            std::memcpy(bytes.data() + offset, line.data(), line.size());
            offset += line.size();
        });
        if (offset != count)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Indexed OBJ encoded size changed between passes.");
        checkCancellation(cancellation);
        return OwnedObjDocument(std::move(bytes));
    }

}
}

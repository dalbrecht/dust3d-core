// SPDX-License-Identifier: MIT
#include "ObjDocument.h"
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
}
}

// SPDX-License-Identifier: MIT
#include "GlbReader.h"
#include "EngineInternal.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <json/json.hpp>
#include <limits>
#include <map>
#include <set>

namespace dust3d {
namespace native {
    namespace {
        using Json = nlohmann::json;
        constexpr size_t maximumInput = 64 * 1024 * 1024;
        constexpr size_t maximumJson = 1024 * 1024;
        constexpr size_t maximumVertices = 250000;
        constexpr size_t maximumFaces = 500000;
        constexpr double maximumCoordinate = std::numeric_limits<long>::max() / 100000.0 < 1000000.0 ? std::numeric_limits<long>::max() / 100000.0 - 1 : 1000000.0;

        [[noreturn]] void invalid(const std::string& message)
        {
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "GLB: " + message);
        }
        [[noreturn]] void unsupported(const std::string& message)
        {
            throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "GLB: " + message);
        }
        [[noreturn]] void limit(const std::string& message)
        {
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "GLB: " + message);
        }
        uint32_t word(const uint8_t* bytes)
        {
            return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
        }
        size_t number(const Json& value)
        {
            if (value.is_number_unsigned()) {
                const auto result = value.get<uint64_t>();
                if (result > std::numeric_limits<size_t>::max())
                    limit("integer exceeds host size");
                return static_cast<size_t>(result);
            }
            if (!value.is_number_integer() || value.get<int64_t>() < 0)
                invalid("expected a nonnegative integer");
            return static_cast<size_t>(value.get<int64_t>());
        }
        size_t optionalNumber(const Json& object, const char* key, size_t fallback = 0)
        {
            return object.count(key) ? number(object.at(key)) : fallback;
        }
        const Json& indexed(const Json& array, size_t index)
        {
            if (!array.is_array() || index >= array.size())
                invalid("array reference is out of range");
            return array.at(index);
        }
        void object(const Json& value)
        {
            if (!value.is_object())
                invalid("expected an object");
        }
        double scalar(const Json& value, double magnitude = maximumCoordinate)
        {
            if (!value.is_number())
                invalid("expected a number");
            const double result = value.get<double>();
            if (!std::isfinite(result) || std::abs(result) > magnitude)
                invalid("nonfinite or out-of-range numeric value");
            return result;
        }
        void identity(const Json& node, const char* key, const std::vector<double>& expected)
        {
            if (!node.count(key))
                return;
            const auto& values = node.at(key);
            if (!values.is_array() || values.size() != expected.size())
                invalid("invalid node transform");
            for (size_t i = 0; i < expected.size(); ++i) {
                if (scalar(values.at(i)) != expected[i])
                    unsupported("nonidentity scene transforms are not supported yet");
            }
        }
        struct View {
            const uint8_t* bytes;
            size_t size;
            size_t stride;
        };
        class Decoder {
        public:
            Decoder(const Json& document, const uint8_t* binary, size_t binarySize)
                : m_document(document)
                , m_binary(binary)
            {
                object(document);
                const auto& buffers = document.at("buffers");
                if (!buffers.is_array() || buffers.size() != 1)
                    unsupported("exactly one embedded buffer is required");
                const auto& buffer = buffers.at(0);
                object(buffer);
                if (buffer.count("uri"))
                    unsupported("external and data URI buffers are not supported");
                m_bufferSize = number(buffer.at("byteLength"));
                if (!m_bufferSize || m_bufferSize > binarySize || binarySize - m_bufferSize > 3)
                    invalid("embedded buffer length does not match BIN chunk");
            }
            View view(size_t index) const
            {
                const auto& description = indexed(m_document.at("bufferViews"), index);
                object(description);
                if (number(description.at("buffer")) != 0)
                    invalid("buffer view references a missing buffer");
                const size_t offset = optionalNumber(description, "byteOffset");
                const size_t size = number(description.at("byteLength"));
                if (!size || offset > m_bufferSize || size > m_bufferSize - offset)
                    invalid("buffer view exceeds embedded buffer");
                return { m_binary + offset, size, optionalNumber(description, "byteStride") };
            }
            struct Accessor {
                const uint8_t* bytes;
                size_t count;
                size_t stride;
                size_t componentType;
                bool normalized;
            };
            Accessor accessor(size_t index, const std::string& type, size_t maximumCount) const
            {
                const auto& description = indexed(m_document.at("accessors"), index);
                object(description);
                if (description.count("sparse"))
                    unsupported("sparse accessors are not supported yet");
                if (description.at("type") != type)
                    invalid("accessor shape does not match its attribute");
                const size_t count = number(description.at("count"));
                if (!count)
                    invalid("empty accessor");
                if (count > maximumCount)
                    limit("accessor count exceeds geometry budget");
                const size_t component = number(description.at("componentType"));
                const size_t width = component == 5121 ? 1 : component == 5123 ? 2
                    : (component == 5125 || component == 5126)                 ? 4
                                                                               : 0;
                if (!width)
                    unsupported("unsupported accessor component type");
                const size_t dimensions = type == "SCALAR" ? 1 : type == "VEC2" ? 2
                    : type == "VEC3"                                            ? 3
                                                                                : 4;
                const size_t element = dimensions * width;
                const auto storage = view(number(description.at("bufferView")));
                const size_t offset = optionalNumber(description, "byteOffset");
                const size_t stride = storage.stride ? storage.stride : element;
                if (stride < element || stride > 252 || stride % width || offset % width)
                    invalid("invalid accessor stride or alignment");
                if (offset > storage.size || element > storage.size - offset || (count - 1) > (storage.size - offset - element) / stride)
                    invalid("accessor exceeds its buffer view");
                const size_t absoluteOffset = static_cast<size_t>(storage.bytes - m_binary) + offset;
                if (absoluteOffset % width)
                    invalid("buffer view and accessor component alignment differ");
                if (type == "SCALAR" && storage.stride)
                    invalid("index buffer views must not declare byteStride");
                if (type != "SCALAR" && (absoluteOffset % 4 || (count > 1 && stride % 4)))
                    invalid("vertex attributes require four-byte alignment");
                bool normalized = false;
                if (description.count("normalized")) {
                    if (!description.at("normalized").is_boolean())
                        invalid("accessor normalized flag must be boolean");
                    normalized = description.at("normalized").get<bool>();
                }
                return { storage.bytes + offset, count, stride, component, normalized };
            }
            double floating(const Accessor& values, size_t index, size_t axis) const
            {
                if (values.componentType != 5126 || values.normalized)
                    unsupported("this attribute requires unnormalized float components");
                const uint32_t bits = word(values.bytes + index * values.stride + axis * 4);
                float result;
                std::memcpy(&result, &bits, sizeof(result));
                if (!std::isfinite(result) || std::abs(result) > maximumCoordinate)
                    invalid("nonfinite or out-of-range attribute");
                return result;
            }
            uint32_t unsignedValue(const uint8_t* data, size_t component) const
            {
                if (component == 5121)
                    return data[0];
                if (component == 5123)
                    return uint32_t(data[0]) | (uint32_t(data[1]) << 8);
                return word(data);
            }
            double color(const Accessor& values, size_t index, size_t axis) const
            {
                double result;
                if (values.componentType == 5126) {
                    result = floating(values, index, axis);
                } else {
                    if (!values.normalized || (values.componentType != 5121 && values.componentType != 5123))
                        unsupported("vertex colors require float or normalized unsigned components");
                    const size_t width = values.componentType == 5121 ? 1 : 2;
                    result = double(unsignedValue(values.bytes + index * values.stride + axis * width, values.componentType)) / (width == 1 ? 255 : 65535);
                }
                if (result < 0 || result > 1)
                    invalid("vertex color is outside zero to one");
                return result;
            }

        private:
            const Json& m_document;
            const uint8_t* m_binary;
            size_t m_bufferSize = 0;
        };
        Json parseJson(const uint8_t* bytes, size_t size)
        {
            size_t events = 0;
            std::map<int, std::set<std::string>> keys;
            return Json::parse(bytes, bytes + size, [&](int depth, Json::parse_event_t event, Json& value) {
                if (depth > 64 || ++events > 100000)
                    limit("JSON nesting or value count exceeds budget");
                if (event == Json::parse_event_t::object_start)
                    keys[depth].clear();
                else if (event == Json::parse_event_t::object_end)
                    keys.erase(depth);
                else if (event == Json::parse_event_t::key) {
                    const auto key = value.get<std::string>();
                    if (!keys[depth - 1].insert(key).second)
                        invalid("duplicate JSON object key");
                }
                return true;
            });
        }
        void validateScene(const Json& document)
        {
            if (document.at("asset").at("version") != "2.0")
                unsupported("only glTF 2.0 assets are supported");
            if (document.at("asset").count("minVersion") && document.at("asset").at("minVersion") != "2.0")
                unsupported("asset requires an unsupported minimum version");
            for (const char* key : { "skins", "animations", "extensionsRequired" }) {
                if (document.count(key) && (!document.at(key).is_array() || !document.at(key).empty()))
                    unsupported(std::string(key) + " is not supported");
            }
            if (document.count("scenes")) {
                const auto& scenes = document.at("scenes");
                if (!scenes.is_array() || scenes.empty() || optionalNumber(document, "scene") >= scenes.size())
                    invalid("default scene reference is out of range");
                if (scenes.size() != 1)
                    unsupported("multiple scenes are not supported yet");
                const auto& roots = scenes.at(0).at("nodes");
                if (!roots.is_array() || roots.size() != 1 || number(roots.at(0)) != 0 || !document.count("nodes") || document.at("nodes").empty())
                    invalid("scene must reference the supported mesh node");
            } else if (document.count("scene")) {
                invalid("default scene references missing scenes");
            }
            if (document.count("nodes")) {
                const auto& nodes = document.at("nodes");
                if (!nodes.is_array() || nodes.size() > 1)
                    unsupported("multiple scene nodes are not supported yet");
                for (const auto& node : nodes) {
                    object(node);
                    if (!node.count("mesh") || number(node.at("mesh")) != 0 || node.count("skin") || node.count("children") || node.count("weights"))
                        unsupported("scene hierarchy or skinning is not supported yet");
                    identity(node, "translation", { 0, 0, 0 });
                    identity(node, "rotation", { 0, 0, 0, 1 });
                    identity(node, "scale", { 1, 1, 1 });
                    identity(node, "matrix", { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 });
                }
            }
        }
        double unitScalar(const Json& value)
        {
            const double result = scalar(value, 1);
            if (result < 0)
                invalid("material channel is outside zero to one");
            return result;
        }
        uint32_t samplerValue(const Json& sampler, const char* key, const std::set<size_t>& allowed, size_t fallback)
        {
            const size_t value = optionalNumber(sampler, key, fallback);
            if (!allowed.count(value))
                invalid(std::string("unsupported sampler enum: ") + key);
            return static_cast<uint32_t>(value);
        }
        void readMaterial(const Json& document, const Decoder& decoder, const std::optional<size_t>& materialIndex, GlbAsset& result)
        {
            if (!materialIndex)
                return;
            const auto& material = indexed(document.at("materials"), *materialIndex);
            object(material);
            if (material.count("normalTexture") || material.count("occlusionTexture") || material.count("emissiveTexture"))
                unsupported("additional imported material maps are not supported yet");
            if (material.count("alphaMode")) {
                const auto mode = material.at("alphaMode").get<std::string>();
                if (mode == "OPAQUE")
                    result.alphaMode = GlbAlphaMode::Opaque;
                else if (mode == "MASK")
                    result.alphaMode = GlbAlphaMode::Mask;
                else if (mode == "BLEND")
                    result.alphaMode = GlbAlphaMode::Blend;
                else
                    invalid("unknown material alpha mode");
            }
            if (material.count("alphaCutoff")) {
                result.alphaCutoff = scalar(material.at("alphaCutoff"));
                if (result.alphaCutoff < 0)
                    invalid("negative alpha cutoff");
            }
            if (material.count("doubleSided")) {
                if (!material.at("doubleSided").is_boolean())
                    invalid("doubleSided must be boolean");
                result.isDoubleSided = material.at("doubleSided").get<bool>();
            }
            if (material.count("emissiveFactor")) {
                const auto& channels = material.at("emissiveFactor");
                if (!channels.is_array() || channels.size() != 3)
                    invalid("emissive factor requires three channels");
                for (size_t axis = 0; axis < 3; ++axis)
                    result.emissive[axis] = unitScalar(channels.at(axis));
            }
            if (!material.count("pbrMetallicRoughness"))
                return;
            const auto& pbr = material.at("pbrMetallicRoughness");
            object(pbr);
            if (pbr.count("metallicRoughnessTexture"))
                unsupported("imported metallic-roughness maps are not supported yet");
            if (pbr.count("metallicFactor"))
                result.metallic = unitScalar(pbr.at("metallicFactor"));
            if (pbr.count("roughnessFactor"))
                result.roughness = unitScalar(pbr.at("roughnessFactor"));
            if (pbr.count("baseColorFactor")) {
                const auto& factor = pbr.at("baseColorFactor");
                if (!factor.is_array() || factor.size() != 4)
                    invalid("base color factor requires four channels");
                for (size_t axis = 0; axis < 4; ++axis) {
                    result.baseColor[axis] = scalar(factor.at(axis), 1);
                    if (result.baseColor[axis] < 0)
                        invalid("base color factor is outside zero to one");
                }
            }
            if (!pbr.count("baseColorTexture"))
                return;
            const auto& binding = pbr.at("baseColorTexture");
            object(binding);
            if (optionalNumber(binding, "texCoord") != 0 || binding.count("extensions"))
                unsupported("texture coordinate transforms or sets are not supported yet");
            const auto& texture = indexed(document.at("textures"), number(binding.at("index")));
            object(texture);
            result.sampler.emplace();
            if (texture.count("sampler")) {
                const auto& sampler = indexed(document.at("samplers"), number(texture.at("sampler")));
                object(sampler);
                result.sampler->wrapS = samplerValue(sampler, "wrapS", { 33071, 33648, 10497 }, 10497);
                result.sampler->wrapT = samplerValue(sampler, "wrapT", { 33071, 33648, 10497 }, 10497);
                if (sampler.count("magFilter"))
                    result.sampler->magnification = samplerValue(sampler, "magFilter", { 9728, 9729 }, 9729);
                if (sampler.count("minFilter"))
                    result.sampler->minification = samplerValue(sampler, "minFilter", { 9728, 9729, 9984, 9985, 9986, 9987 }, 9987);
            }
            const auto& image = indexed(document.at("images"), number(texture.at("source")));
            object(image);
            if (image.count("uri"))
                unsupported("external and data URI images are not supported");
            const std::string mime = image.at("mimeType").get<std::string>();
            if (mime != "image/png" && mime != "image/jpeg")
                unsupported("unsupported embedded image MIME type");
            const auto storage = decoder.view(number(image.at("bufferView")));
            if (storage.stride)
                invalid("encoded image buffer view has a vertex stride");
            if (storage.size > 16 * 1024 * 1024)
                limit("embedded image exceeds 16 MiB");
            result.image = GlbEncodedImage { mime, { storage.bytes, storage.bytes + storage.size } };
        }
        GlbAsset decode(const Json& document, const uint8_t* binary, size_t binarySize)
        {
            validateScene(document);
            Decoder decoder(document, binary, binarySize);
            const auto& meshes = document.at("meshes");
            if (!meshes.is_array() || meshes.size() != 1)
                unsupported("exactly one mesh is supported");
            const auto& mesh = meshes.at(0);
            if (mesh.count("weights"))
                unsupported("morph weights are not supported yet");
            const auto& primitives = mesh.at("primitives");
            if (!primitives.is_array() || primitives.empty())
                invalid("mesh has no primitives");
            GlbAsset result;
            std::optional<size_t> selectedMaterial;
            bool firstPrimitive = true;
            bool hasVertexColors = false;
            for (const auto& primitive : primitives) {
                object(primitive);
                if (optionalNumber(primitive, "mode", 4) != 4 || primitive.count("targets"))
                    unsupported("only triangle primitives without morph targets are supported");
                const std::optional<size_t> material = primitive.count("material") ? std::optional<size_t>(number(primitive.at("material"))) : std::nullopt;
                if (!firstPrimitive && material != selectedMaterial)
                    unsupported("multiple primitive materials are not supported yet");
                selectedMaterial = material;
                firstPrimitive = false;
                const auto& attributes = primitive.at("attributes");
                object(attributes);
                for (auto it = attributes.begin(); it != attributes.end(); ++it) {
                    if (it.key() != "POSITION" && it.key() != "NORMAL" && it.key() != "TEXCOORD_0" && it.key() != "COLOR_0")
                        unsupported("unsupported vertex attribute: " + it.key());
                }
                const auto positions = decoder.accessor(number(attributes.at("POSITION")), "VEC3", maximumVertices);
                const size_t base = result.model.vertices.size();
                if (positions.count > maximumVertices - base)
                    limit("total vertex count exceeds budget");
                for (size_t index = 0; index < positions.count; ++index)
                    result.model.vertices.emplace_back(decoder.floating(positions, index, 0), decoder.floating(positions, index, 1), decoder.floating(positions, index, 2));
                std::vector<Vector3> normals(positions.count);
                if (attributes.count("NORMAL")) {
                    const auto values = decoder.accessor(number(attributes.at("NORMAL")), "VEC3", maximumVertices);
                    if (values.count != positions.count)
                        invalid("normal and position counts differ");
                    for (size_t index = 0; index < values.count; ++index) {
                        normals[index] = Vector3(decoder.floating(values, index, 0), decoder.floating(values, index, 1), decoder.floating(values, index, 2));
                        if (normals[index].length() < 1e-12)
                            invalid("zero-length normal");
                        normals[index] = normals[index].normalized();
                    }
                }
                std::vector<Vector2> uvs;
                if (attributes.count("TEXCOORD_0")) {
                    const auto values = decoder.accessor(number(attributes.at("TEXCOORD_0")), "VEC2", maximumVertices);
                    if (values.count != positions.count)
                        invalid("UV and position counts differ");
                    for (size_t index = 0; index < values.count; ++index)
                        uvs.emplace_back(decoder.floating(values, index, 0), decoder.floating(values, index, 1));
                }
                std::vector<Color> colors(positions.count, Color(1, 1, 1, 1));
                if (attributes.count("COLOR_0")) {
                    hasVertexColors = true;
                    const size_t index = number(attributes.at("COLOR_0"));
                    const auto& description = indexed(document.at("accessors"), index);
                    const std::string type = description.at("type").get<std::string>();
                    if (type != "VEC3" && type != "VEC4")
                        invalid("invalid vertex color shape");
                    const auto values = decoder.accessor(index, type, maximumVertices);
                    if (values.count != positions.count)
                        invalid("color and position counts differ");
                    for (size_t i = 0; i < values.count; ++i)
                        colors[i] = Color(decoder.color(values, i, 0), decoder.color(values, i, 1), decoder.color(values, i, 2), type == "VEC4" ? decoder.color(values, i, 3) : 1);
                }
                std::vector<size_t> indices;
                if (primitive.count("indices")) {
                    const auto values = decoder.accessor(number(primitive.at("indices")), "SCALAR", maximumFaces * 3);
                    if (values.componentType == 5126 || values.normalized)
                        invalid("triangle indices require unsigned integer components");
                    for (size_t index = 0; index < values.count; ++index) {
                        const size_t vertex = decoder.unsignedValue(values.bytes + index * values.stride, values.componentType);
                        if (vertex >= positions.count)
                            invalid("triangle index exceeds primitive vertex count");
                        indices.push_back(vertex);
                    }
                } else {
                    for (size_t index = 0; index < positions.count; ++index)
                        indices.push_back(index);
                }
                if (indices.size() % 3)
                    invalid("triangle index count is not divisible by three");
                if (indices.size() / 3 > maximumFaces - result.model.faces.size())
                    limit("total triangle count exceeds budget");
                for (size_t index = 0; index < indices.size(); index += 3) {
                    const size_t a = indices[index], b = indices[index + 1], c = indices[index + 2];
                    const auto normal = Vector3::crossProduct(result.model.vertices[base + b] - result.model.vertices[base + a], result.model.vertices[base + c] - result.model.vertices[base + a]);
                    if (normal.length() < 1e-12)
                        invalid("degenerate triangle");
                    result.model.faces.push_back({ base + a, base + b, base + c });
                    if (!attributes.count("NORMAL")) {
                        normals[a] += normal;
                        normals[b] += normal;
                        normals[c] += normal;
                    }
                    if (!uvs.empty()) {
                        const std::array<PositionKey, 3> key { PositionKey(result.model.vertices[base + a]), PositionKey(result.model.vertices[base + b]), PositionKey(result.model.vertices[base + c]) };
                        if (!result.model.triangleUvs.emplace(key, std::array<Vector2, 3> { uvs[a], uvs[b], uvs[c] }).second)
                            unsupported("duplicate position-key UV triangles cannot be represented losslessly");
                    }
                }
                if (!attributes.count("NORMAL")) {
                    for (auto& normal : normals) {
                        if (normal.length() < 1e-12)
                            invalid("unused or canceling vertex has no generated normal");
                        normal = normal.normalized();
                    }
                }
                result.model.vertexNormals.insert(result.model.vertexNormals.end(), normals.begin(), normals.end());
                result.model.vertexColors.insert(result.model.vertexColors.end(), colors.begin(), colors.end());
            }
            // An empty color array lets the inherited generator retain part colors.
            // Mixed primitives need aligned white defaults only when colors are present.
            if (!hasVertexColors)
                result.model.vertexColors.clear();
            readMaterial(document, decoder, selectedMaterial, result);
            if (result.image && result.model.triangleUvs.size() != result.model.faces.size())
                invalid("textured primitives require UVs on every triangle");
            return result;
        }
    }
    GlbAsset readGlb(const uint8_t* bytes, size_t size)
    {
        if (size > maximumInput)
            limit("input exceeds 64 MiB");
        if (!bytes || size < 28)
            invalid("truncated header");
        if (word(bytes) != 0x46546c67 || word(bytes + 4) != 2 || word(bytes + 8) != size)
            invalid("invalid magic, version or total length");
        const uint8_t* json = nullptr;
        const uint8_t* binary = nullptr;
        size_t jsonSize = 0, binarySize = 0;
        for (size_t offset = 12; offset < size;) {
            if (size - offset < 8)
                invalid("truncated chunk header");
            const size_t length = word(bytes + offset);
            const uint32_t type = word(bytes + offset + 4);
            offset += 8;
            if (length % 4 || length > size - offset)
                invalid("invalid chunk length");
            if (type == 0x4e4f534a) {
                if (json || offset != 20 || !length)
                    invalid("JSON must be the first and only JSON chunk");
                if (length > maximumJson)
                    limit("JSON exceeds 1 MiB");
                json = bytes + offset;
                jsonSize = length;
            } else if (type == 0x004e4942) {
                if (!json || binary || !length)
                    invalid("invalid BIN chunk ordering");
                binary = bytes + offset;
                binarySize = length;
            }
            offset += length;
        }
        if (!json || !binary)
            invalid("JSON and BIN chunks are required");
        try {
            return decode(parseJson(json, jsonSize), binary, binarySize);
        } catch (const Json::exception& cause) {
            invalid(std::string("invalid JSON or schema: ") + cause.what());
        }
    }
}
}

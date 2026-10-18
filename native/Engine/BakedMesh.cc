// SPDX-License-Identifier: MIT
#include "BakedMesh.h"
#include "GenerationPipeline.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <sstream>

namespace dust3d {
namespace native {
    namespace {
        constexpr size_t maximumBytes = 268435456;
        [[noreturn]] void invalid(const char* cause) { throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, cause); }
        [[noreturn]] void limit(const char* cause) { throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, cause); }
        size_t dimension(size_t value, size_t fallback)
        {
            if (value > 8192)
                invalid("Bake dimensions must be from 1 to 8192, or zero for defaults.");
            return value ? value : fallback;
        }
        double scalar(const std::map<std::string, std::string>& attributes, const char* key, double fallback, double minimum, double maximum)
        {
            const auto& text = String::valueOrEmpty(attributes, key);
            if (text.empty())
                return fallback;
            auto normalized = text;
            std::replace(normalized.begin(), normalized.end(), ',', '.');
            std::istringstream input(normalized);
            input.imbue(std::locale::classic());
            double value = 0;
            input >> value;
            if (!input || !std::isfinite(value) || value < minimum || value > maximum)
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, std::string("Invalid bake scalar: ") + key);
            input >> std::ws;
            if (!input.eof())
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, std::string("Trailing data in bake scalar: ") + key);
            return value;
        }
    }

    BakedMeshData::BakedMeshData(const Dust3DBakeOptions& options)
        : m_options(options)
    {
        if (options.struct_size != sizeof(options))
            invalid("Baking requires correctly sized options.");
        if (options.maximum_uv_payload_bytes > maximumBytes || options.maximum_bake_payload_bytes > maximumBytes)
            limit("Bake and UV payload budgets must not exceed 256 MiB.");
        m_options.maximum_uv_payload_bytes = options.maximum_uv_payload_bytes ? options.maximum_uv_payload_bytes : DUST3D_DEFAULT_UV_PAYLOAD_BYTES;
        m_options.maximum_bake_payload_bytes = options.maximum_bake_payload_bytes ? options.maximum_bake_payload_bytes : maximumBytes;
        m_options.atlas_texture_size = dimension(options.atlas_texture_size, 4096);
        m_options.color_size = dimension(options.color_size, 4096);
        m_options.normal_size = dimension(options.normal_size, 2048);
        m_options.material_size = dimension(options.material_size, 1024);
        m_options.maximum_spatial_grid_entries = options.maximum_spatial_grid_entries ? options.maximum_spatial_grid_entries : 1000000;
        if (m_options.maximum_spatial_grid_entries > 16000000)
            invalid("The bake spatial grid limit must not exceed sixteen million entries.");
        if (options.image_count > 10000 || (options.image_count && !options.images))
            invalid("Baking requires at most 10000 image descriptors and a valid descriptor buffer.");
        checkCancellation(options.cancellation);
        for (size_t index = 0; index < options.image_count; ++index) {
            checkCancellation(options.cancellation);
            const auto& image = options.images[index];
            if (!std::memchr(image.image_id, 0, sizeof(image.image_id)))
                invalid("A bake image identity must be terminated within its fixed buffer.");
            const Uuid identity(image.image_id);
            if (identity.isNull() || image.width == 0 || image.height == 0 || image.width > 8192 || image.height > 8192 || !image.pixels)
                invalid("A bake image requires a non-null identity, pixels and dimensions from 1 to 8192.");
            const auto rowBytes = image.width * 4;
            if (image.row_stride < rowBytes || image.byte_count < rowBytes || (image.height > 1 && image.row_stride > (image.byte_count - rowBytes) / (image.height - 1)))
                invalid("A bake image stride/span does not cover its readable RGBA8 rows.");
            if (!m_images.emplace(identity, UvAtlasImage { identity, image.width, image.height }).second)
                invalid("Bake image identities must be unique, including engine aliases.");
            m_pixels.emplace(identity, TextureImageView { image.width, image.height, image.row_stride, image.byte_count, image.pixels });
        }
        // No caller descriptor array is needed after its views have been copied.
        m_options.images = nullptr;
        m_options.image_count = 0;
    }

    TextureBakeInputs BakedMeshData::resolvedInputs(const Snapshot& snapshot) const
    {
        TextureBakeInputs result;
        result.images = m_pixels;
        std::map<Uuid, std::map<std::string, std::string>> definitions;
        for (const auto& [key, attributes] : snapshot.components) {
            checkCancellation(cancellation());
            const Uuid identity(key);
            if (identity.isNull())
                continue;
            // Editing labels and dirty flags do not affect generated pixels. Keep
            // conflicting source definitions explicit instead of picking an alias.
            std::map<std::string, std::string> visual;
            for (const auto* field : { "linkDataType", "linkData", "wrap", "wrapPattern", "wrapPatternColor", "wrapPatternScale", "wrapBelly", "wrapWrinkles", "__wrapFolds" }) {
                const auto value = String::valueOrEmpty(attributes, field);
                if (!value.empty())
                    visual[field] = value;
            }
            const auto [found, inserted] = definitions.emplace(identity, visual);
            if (!inserted && found->second != visual)
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Aliased bake components contain conflicting definitions.");
            const auto part = snapshot.parts.find(String::valueOrEmpty(attributes, "linkData"));
            if (String::valueOrEmpty(attributes, "linkDataType") == "partId" && part != snapshot.parts.end())
                result.materials[identity] = { scalar(part->second, "metallic", 0, 0, 1), scalar(part->second, "roughness", 1, 0, 1), scalar(part->second, "emissive", 0, 0, 2) };
            if (String::valueOrEmpty(attributes, "wrap").empty())
                continue;
            auto& surface = result.surfaces[identity];
            const auto patternName = String::valueOrEmpty(attributes, "wrapPattern");
            const auto type = SurfacePattern::typeFromString(patternName);
            if (type == SurfacePattern::Type::None && !patternName.empty() && patternName != "None")
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "The bake surface pattern is not supported.");
            if (type != SurfacePattern::Type::None) {
                SurfacePattern::Settings pattern;
                pattern.type = type;
                pattern.baseColor = normalizedUvColor(String::valueOrEmpty(attributes, "color"));
                const auto ink = String::valueOrEmpty(attributes, "wrapPatternColor");
                pattern.patternColor = ink.empty() ? SurfacePattern::defaultPatternColor(pattern.baseColor) : normalizedUvColor(ink);
                pattern.scale = std::max(0.005, scalar(attributes, "wrapPatternScale", 0.06, 0, std::numeric_limits<float>::max()));
                pattern.belly = scalar(attributes, "wrapBelly", 0, 0, 1);
                pattern.seed = SurfacePattern::seedFromString(key);
                surface.pattern = pattern;
            }
            surface.wrinkleStrength = scalar(attributes, "wrapWrinkles", 0, 0, 1);
            surface.folds = ClothFolds::deserialize(String::valueOrEmpty(attributes, "__wrapFolds"));
        }
        return result;
    }

    void BakedMeshData::validateVisuals(const Snapshot& snapshot) const
    {
        validateUvVisuals(snapshot, m_images);
        (void)resolvedInputs(snapshot);
    }

    void BakedMeshData::prepare(const Object& object, const Snapshot& snapshot, const UvAtlas& atlas)
    {
        checkCancellation(cancellation());
        const auto quota = m_options.maximum_bake_payload_bytes;
        if (m_originalIndices.size() > quota / sizeof(Dust3DTextureTangent))
            limit("The baked triangle tangent payload exceeds its configured budget.");
        const auto tangentBytes = m_originalIndices.size() * sizeof(Dust3DTextureTangent);
        const auto colorBytes = m_options.color_size * m_options.color_size * 4;
        if (colorBytes > quota - tangentBytes)
            limit("The mandatory baked color/tangent payload exceeds its configured budget.");
        if (object.triangles.size() > maximumBytes / sizeof(TextureTangent))
            limit("The temporary baker tangent payload exceeds its independent ceiling.");
        const auto temporaryTangents = object.triangles.size() * sizeof(TextureTangent);
        TextureBakeOptions options;
        options.colorSize = m_options.color_size;
        options.normalSize = m_options.normal_size;
        options.materialSize = m_options.material_size;
        options.maximumSpatialGridEntries = m_options.maximum_spatial_grid_entries;
        options.maximumPayloadBytes = std::min(maximumBytes, quota - tangentBytes + temporaryTangents);
        options.cancellation = cancellation() ? &cancellation()->cancelled : nullptr;
        auto result = TextureBaker::bake(object, atlas, resolvedInputs(snapshot), options);
        const auto pixelBytes = result.payloadBytes - temporaryTangents;
        if (pixelBytes > quota - tangentBytes)
            limit("The final baked map/tangent payload exceeds its configured budget.");
        tangents.reserve(m_originalIndices.size());
        for (const auto index : m_originalIndices) {
            checkCancellation(cancellation());
            if (index >= result.tangents.size())
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "A retained bake triangle has no aligned tangent record.");
            const auto& source = result.tangents[index];
            Dust3DTextureTangent tangent {};
            for (size_t axis = 0; axis < 3; ++axis)
                tangent.direction[axis] = static_cast<float>(source.direction[axis]);
            tangent.handedness = static_cast<float>(source.handedness);
            tangent.known = source.known ? 1 : 0;
            tangents.push_back(tangent);
        }
        maps[0] = std::move(result.color);
        maps[1] = std::move(result.normal);
        maps[2] = std::move(result.metallic);
        maps[3] = std::move(result.roughness);
        maps[4] = std::move(result.emissive);
        payloadBytes = pixelBytes + tangentBytes;
        // Generated triangle ordinals and caller pixel views are call-local workspace.
        std::vector<size_t>().swap(m_originalIndices);
        m_pixels.clear();
        m_options.cancellation = nullptr;
    }
}
}

Dust3DBakedMesh* dust3d_baked_mesh_load(const uint8_t* bytes, size_t size, const Dust3DBakeOptions* options, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!options)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Baking requires explicit options.");
        if (options->struct_size != sizeof(Dust3DBakeOptions))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Baking requires correctly sized options.");
        auto owner = std::make_unique<Dust3DBakedMesh>();
        owner->data = std::make_unique<BakedMeshData>(*options);
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(options->cancellation);
        std::string warning;
        auto snapshot = readDocument(bytes, size, warning);
        owner->data->validateVisuals(*snapshot);
        owner->uvs = std::make_unique<Dust3DUvMesh>();
        owner->uvs->metadata = std::make_unique<UvMetadata>(*snapshot, owner->data->uvBudget());
        owner->uvs->geometry = generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, nullptr, owner->uvs->metadata.get(), owner->data->images(), owner->data->textureSize(), owner->data.get());
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const dust3d::TextureBakeError& cause) {
        const auto status = cause.code() == dust3d::TextureBakeErrorCode::cancelled ? DUST3D_STATUS_CANCELLED
            : cause.code() == dust3d::TextureBakeErrorCode::limitExceeded           ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == dust3d::TextureBakeErrorCode::invalidInput            ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                                    : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const dust3d::UvAtlasError& cause) {
        const auto status = cause.code() == dust3d::UvAtlasErrorCode::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == dust3d::UvAtlasErrorCode::invalidInput                ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                                    : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown native texture-bake failure.");
    }
    return nullptr;
}
void dust3d_baked_mesh_destroy(Dust3DBakedMesh* mesh) { delete mesh; }
const Dust3DUvMesh* dust3d_baked_mesh_uvs(const Dust3DBakedMesh* mesh) { return mesh ? mesh->uvs.get() : nullptr; }
Dust3DTextureView dust3d_baked_mesh_texture(const Dust3DBakedMesh* mesh, uint32_t map)
{
    const auto index = map;
    if (!mesh || index >= 5 || !mesh->data->maps[index])
        return {};
    const auto& image = *mesh->data->maps[index];
    return { image.width, image.height, image.width * 4, image.pixels.size(), image.pixels.data() };
}
const Dust3DTextureTangent* dust3d_baked_mesh_tangents(const Dust3DBakedMesh* mesh) { return mesh && !mesh->data->tangents.empty() ? mesh->data->tangents.data() : nullptr; }
size_t dust3d_baked_mesh_tangent_count(const Dust3DBakedMesh* mesh) { return mesh ? mesh->data->tangents.size() : 0; }
size_t dust3d_baked_mesh_payload_bytes(const Dust3DBakedMesh* mesh) { return mesh ? mesh->data->payloadBytes : 0; }

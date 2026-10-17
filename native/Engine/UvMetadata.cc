// SPDX-License-Identifier: MIT
#include "UvMetadata.h"
#include "GenerationPipeline.h"
#include <algorithm>
#include <cstring>
#include <new>

namespace dust3d {
namespace native {
    namespace {
        Color normalizedColor(const std::string& text)
        {
            if (text.empty())
                return Color(1, 1, 1);
            if ((text.size() != 7 && text.size() != 9) || text[0] != '#')
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "UV colors require #RRGGBB or alpha-first #AARRGGBB notation.");
            auto digit = [](char value) -> unsigned {
                if (value >= '0' && value <= '9')
                    return unsigned(value - '0');
                if (value >= 'a' && value <= 'f')
                    return unsigned(value - 'a' + 10);
                if (value >= 'A' && value <= 'F')
                    return unsigned(value - 'A' + 10);
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "A UV color contains a non-hexadecimal channel.");
            };
            auto channel = [&](size_t offset) { return (digit(text[offset]) * 16 + digit(text[offset + 1])) / 255.0; };
            const size_t offset = text.size() == 9 ? 3 : 1;
            return Color(channel(offset), channel(offset + 2), channel(offset + 4), text.size() == 9 ? channel(1) : 1);
        }

        bool sameVisual(const UvAtlasComponent& first, const UvAtlasComponent& second)
        {
            for (size_t channel = 0; channel < 4; ++channel) {
                if (first.color[channel] != second.color[channel])
                    return false;
            }
            if (bool(first.image) != bool(second.image))
                return false;
            return !first.image || (first.image->id == second.image->id && first.image->width == second.image->width && first.image->height == second.image->height);
        }

        std::map<Uuid, UvAtlasComponent> resolvedComponents(const Snapshot& generated, const std::map<Uuid, UvAtlasImage>& images)
        {
            std::map<Uuid, UvAtlasComponent> components;
            for (const auto& [key, attributes] : generated.components) {
                const Uuid identity(key);
                if (identity.isNull())
                    continue;
                UvAtlasComponent component;
                component.color = normalizedColor(String::valueOrEmpty(attributes, "color"));
                const auto imageKey = String::valueOrEmpty(attributes, "colorImageId");
                if (!imageKey.empty()) {
                    const auto found = images.find(Uuid(imageKey));
                    if (found == images.end())
                        throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "A UV component image has no resolved image dimensions.");
                    component.image = found->second;
                }
                const auto [found, inserted] = components.emplace(identity, component);
                if (!inserted && !sameVisual(found->second, component))
                    throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Component aliases contain conflicting UV visual definitions.");
            }
            return components;
        }

        size_t validateOptions(const Dust3DUvOptions* options)
        {
            if (!options || options->struct_size != sizeof(Dust3DUvOptions))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "UV generation requires correctly sized options.");
            if (options->maximum_payload_bytes > DUST3D_MAX_UV_PAYLOAD_BYTES)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The UV payload budget exceeds 256 MiB.");
            if (options->texture_size > 8192 || options->image_count > 10000 || (options->image_count && !options->images))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "UV options need a texture scale up to 8192 and at most 10000 image descriptors with a valid buffer.");
            return options->maximum_payload_bytes == 0 ? DUST3D_DEFAULT_UV_PAYLOAD_BYTES : options->maximum_payload_bytes;
        }

        std::map<Uuid, UvAtlasImage> resolvedImages(const Dust3DUvOptions& options)
        {
            std::map<Uuid, UvAtlasImage> images;
            for (size_t index = 0; index < options.image_count; ++index) {
                const auto& image = options.images[index];
                const auto* end = static_cast<const char*>(std::memchr(image.image_id, 0, sizeof(image.image_id)));
                if (!end || image.width == 0 || image.height == 0 || image.width > 8192 || image.height > 8192)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "A UV image descriptor needs a terminated identity and dimensions from 1 to 8192.");
                const Uuid identity(std::string(image.image_id, end));
                if (identity.isNull() || !images.emplace(identity, UvAtlasImage { identity, image.width, image.height }).second)
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "UV image identities must be valid and unique, including engine aliases.");
            }
            return images;
        }
    }

    UvMetadata::UvMetadata(const Snapshot& original, size_t maximumBytes)
        : m_maximumBytes(maximumBytes)
    {
        for (const auto& [key, attributes] : original.components) {
            charge(1, sizeof(std::string));
            charge(key.size());
            charge(1);
            const auto index = m_components.size();
            m_components.push_back(key);
            const Uuid identity(key);
            if (identity.isNull())
                continue;
            const auto [found, inserted] = m_componentIndices.emplace(identity, index);
            if (!inserted)
                found->second = SIZE_MAX;
            else {
                charge(1, sizeof(decltype(m_componentIndices)::value_type));
                charge(identity.toString().size());
            }
        }
    }

    void UvMetadata::charge(size_t count, size_t width)
    {
        if (width && count > (m_maximumBytes - m_payloadBytes) / width)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The UV metadata exceeds its configured logical payload budget.");
        m_payloadBytes += count * width;
    }

    std::pair<Dust3DUvComponentKind, size_t> UvMetadata::resolve(const Uuid& identity) const
    {
        const auto found = m_componentIndices.find(identity);
        if (identity.isNull() || found == m_componentIndices.end())
            return { DUST3D_UV_UNKNOWN, SIZE_MAX };
        if (found->second == SIZE_MAX)
            return { DUST3D_UV_AMBIGUOUS, SIZE_MAX };
        return { DUST3D_UV_COMPONENT, found->second };
    }

    void UvMetadata::prepare(const Object& object, const Snapshot& generated, const std::map<Uuid, UvAtlasImage>& images, size_t textureSize)
    {
        const auto components = resolvedComponents(generated, images);
        UvAtlasOptions options;
        options.textureSize = textureSize;
        // Retain the atlas and its C views under one aggregate logical budget.
        options.maximumPayloadBytes = m_maximumBytes - m_payloadBytes;
        if (options.maximumPayloadBytes == 0)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "No UV metadata budget remains for the atlas.");
        auto atlas = UvAtlasBuilder::build(object, components, options);
        charge(atlas.payloadBytes);
        charge(atlas.layouts.size(), sizeof(Dust3DUvChart));
        charge(atlas.gradients.size(), sizeof(Dust3DUvGradient));
        for (const auto& layout : atlas.layouts) {
            Dust3DUvChart chart {};
            chart.left = layout.left;
            chart.top = layout.top;
            chart.width = layout.width;
            chart.height = layout.height;
            chart.flipped = layout.flipped;
            for (size_t channel = 0; channel < 4; ++channel)
                chart.color[channel] = layout.color[channel];
            const auto identity = resolve(layout.sourceId);
            chart.component_kind = identity.first;
            chart.component_index = identity.second;
            if (!layout.id.isNull()) {
                const auto text = layout.id.toString();
                if (text.size() >= sizeof(chart.image_id))
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "A packed image identity exceeds the UV descriptor buffer.");
                std::memcpy(chart.image_id, text.c_str(), text.size() + 1);
            }
            m_charts.push_back(chart);
        }
        for (const auto& gradient : atlas.gradients) {
            Dust3DUvGradient value {};
            value.chart_index = gradient.layoutIndex;
            value.resolution = gradient.resolution;
            for (size_t channel = 0; channel < 4; ++channel) {
                value.large_color[channel] = gradient.largeColor[channel];
                value.small_color[channel] = gradient.smallColor[channel];
            }
            m_gradients.push_back(value);
        }
        m_atlas = std::move(atlas);
    }

    void UvMetadata::appendTriangle(size_t originalIndex)
    {
        if (originalIndex >= m_atlas.triangleUvs.size() || originalIndex >= m_atlas.triangleUvKnown.size() || originalIndex >= m_atlas.triangleComponentIds.size())
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "A retained triangle has no aligned UV metadata record.");
        charge(1, sizeof(Dust3DUvTriangle));
        Dust3DUvTriangle triangle {};
        triangle.known = m_atlas.triangleUvKnown[originalIndex];
        for (size_t corner = 0; corner < 3; ++corner) {
            triangle.uv[corner][0] = m_atlas.triangleUvs[originalIndex][corner].x();
            triangle.uv[corner][1] = m_atlas.triangleUvs[originalIndex][corner].y();
        }
        const auto identity = resolve(m_atlas.triangleComponentIds[originalIndex]);
        triangle.component_kind = identity.first;
        triangle.component_index = identity.second;
        m_triangles.push_back(triangle);
    }
}
}

Dust3DUvMesh* dust3d_uv_mesh_load(const uint8_t* bytes, size_t size, const Dust3DUvOptions* options, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        const auto maximumBytes = validateOptions(options);
        const auto images = resolvedImages(*options);
        std::lock_guard<std::mutex> lock(engineMutex());
        std::string warning;
        auto snapshot = readDocument(bytes, size, warning);
        // Reject unsupported colors before inherited generation parses source values.
        (void)resolvedComponents(*snapshot, images);
        auto owner = std::make_unique<Dust3DUvMesh>();
        owner->metadata = std::make_unique<UvMetadata>(*snapshot, maximumBytes);
        owner->geometry = generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, owner->metadata.get(), images, options->texture_size == 0 ? 4096 : options->texture_size);
        return owner.release();
    } catch (const DocumentError& exception) {
        setError(error, exception.status(), exception.what());
    } catch (const dust3d::UvAtlasError& exception) {
        const auto status = exception.code() == dust3d::UvAtlasErrorCode::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
            : exception.code() == dust3d::UvAtlasErrorCode::invalidInput                ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                                        : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, exception.what());
    } catch (const std::bad_alloc& exception) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
    } catch (const std::exception& exception) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, exception.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown UV metadata generation failure.");
    }
    return nullptr;
}

void dust3d_uv_mesh_destroy(Dust3DUvMesh* mesh) { delete mesh; }
const Dust3DMesh* dust3d_uv_mesh_geometry(const Dust3DUvMesh* mesh) { return mesh ? mesh->geometry.get() : nullptr; }
const Dust3DUvTriangle* dust3d_uv_mesh_triangles(const Dust3DUvMesh* mesh) { return mesh && !mesh->metadata->triangles().empty() ? mesh->metadata->triangles().data() : nullptr; }
const Dust3DUvChart* dust3d_uv_mesh_charts(const Dust3DUvMesh* mesh) { return mesh && !mesh->metadata->charts().empty() ? mesh->metadata->charts().data() : nullptr; }
const Dust3DUvGradient* dust3d_uv_mesh_gradients(const Dust3DUvMesh* mesh) { return mesh && !mesh->metadata->gradients().empty() ? mesh->metadata->gradients().data() : nullptr; }
size_t dust3d_uv_mesh_triangle_count(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->triangles().size() : 0; }
size_t dust3d_uv_mesh_chart_count(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->charts().size() : 0; }
size_t dust3d_uv_mesh_gradient_count(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->gradients().size() : 0; }
size_t dust3d_uv_mesh_component_count(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->components().size() : 0; }
const char* dust3d_uv_mesh_component_id(const Dust3DUvMesh* mesh, size_t index) { return mesh && index < mesh->metadata->components().size() ? mesh->metadata->components()[index].c_str() : ""; }
size_t dust3d_uv_mesh_payload_bytes(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->payloadBytes() : 0; }
double dust3d_uv_mesh_texture_size(const Dust3DUvMesh* mesh) { return mesh ? mesh->metadata->textureSize() : 0; }

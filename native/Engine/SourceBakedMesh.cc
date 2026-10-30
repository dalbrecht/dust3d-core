// SPDX-License-Identifier: MIT
#include "SourceBakedMesh.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dust3d/uv/texture_baker_internal.h>
#include <limits>
#include <set>
namespace dust3d {
namespace native {
    namespace {
        constexpr size_t maximumBytes = 256 * 1024 * 1024;
        [[noreturn]] void invalid(const char* cause) { throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, cause); }
        [[noreturn]] void limit(const char* cause) { throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, cause); }
        void charge(RetainedPayloadBudget& budget, size_t count, size_t width = 1)
        {
            if (!budget.tryAdd(count, width))
                limit("Source bake retained output exceeds its aggregate quota.");
        }
        size_t quota(size_t value, size_t fallback)
        {
            if (value > maximumBytes)
                limit("Source bake payload quotas cannot exceed 256 MiB.");
            return value ? value : fallback;
        }
        TextureImageView view(const Dust3DTextureView& image)
        {
            if (!image.width || !image.height || image.width > 8192 || image.height > 8192 || !image.pixels)
                invalid("A source bake image requires pixels and dimensions in 1..8192.");
            const size_t row = image.width * 4;
            if (image.row_stride < row || image.byte_count < row || (image.height > 1 && image.row_stride > (image.byte_count - row) / (image.height - 1)))
                invalid("A source bake image stride/span is truncated or overflows.");
            return { image.width, image.height, image.row_stride, image.byte_count, image.pixels };
        }
        ColorTextureWrap wrapping(uint32_t value)
        {
            switch (value) {
            case 10497:
                return ColorTextureWrap::repeatTexture;
            case 33648:
                return ColorTextureWrap::mirroredRepeat;
            case 33071:
                return ColorTextureWrap::clampToEdge;
            default:
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported imported texture wrapping mode.");
            }
        }
        ColorTextureMinificationFilter minification(uint32_t value)
        {
            switch (value) {
            case 9728:
                return ColorTextureMinificationFilter::nearest;
            case 9729:
                return ColorTextureMinificationFilter::linear;
            case 9984:
                return ColorTextureMinificationFilter::nearestMipmapNearest;
            case 9985:
                return ColorTextureMinificationFilter::linearMipmapNearest;
            case 9986:
                return ColorTextureMinificationFilter::nearestMipmapLinear;
            case 9987:
                return ColorTextureMinificationFilter::linearMipmapLinear;
            default:
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported imported texture minification filter.");
            }
        }
        using TriangleKey = std::array<PositionKey, 3>;
        void populateProcedural(const Object& original, const std::vector<size_t>& retained, const ImportedSurfaceData& sources,
            Object& procedural, std::vector<size_t>& proceduralToRetained, const Dust3DCancellationToken* cancellation)
        {
            std::map<TriangleKey, std::set<Uuid>> allowed;
            std::set<TriangleKey> unknownOwners;
            for (size_t index = 0; index < retained.size(); ++index) {
                checkCancellation(cancellation);
                if (sources.triangles().at(index).model_index != SIZE_MAX)
                    continue;
                const size_t originalIndex = retained[index];
                const auto& face = original.triangles.at(originalIndex);
                const size_t base = procedural.vertices.size();
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto& position = original.vertices.at(face.at(corner));
                    for (size_t axis = 0; axis < 3; ++axis) {
                        const double scaled = position[axis] * 100000;
                        const double bound = std::ldexp(1.0, std::numeric_limits<long>::digits);
                        if (!std::isfinite(scaled) || scaled < -bound || scaled >= bound)
                            throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Procedural source bake positions exceed the UV key range.");
                    }
                    procedural.vertices.push_back(position);
                }
                const TriangleKey key { PositionKey(procedural.vertices[base]), PositionKey(procedural.vertices[base + 1]), PositionKey(procedural.vertices[base + 2]) };
                const Uuid owner = originalIndex < original.triangleComponentIds.size() ? original.triangleComponentIds[originalIndex] : Uuid {};
                for (size_t rotation = 0; rotation < 3; ++rotation) {
                    TriangleKey rotated { key[rotation], key[(rotation + 1) % 3], key[(rotation + 2) % 3] };
                    if (!owner.isNull())
                        allowed[rotated].insert(owner);
                    else {
                        (void)allowed[rotated];
                        unknownOwners.insert(rotated);
                    }
                }
                procedural.triangles.push_back({ base, base + 1, base + 2 });
                procedural.triangleComponentIds.push_back(owner);
                proceduralToRetained.push_back(index);
            }
            for (const auto& [component, mappings] : original.componentTriangleUvs)
                for (const auto& [key, uvs] : mappings) {
                    checkCancellation(cancellation);
                    const auto found = allowed.find(key);
                    if (found != allowed.end() && (unknownOwners.count(key) || found->second.empty() || found->second.count(component)))
                        procedural.componentTriangleUvs[component].emplace(key, uvs);
                }
            for (const auto& [key, owner] : original.brokenTrianglesToComponentIdMap)
                if (allowed.count(key))
                    procedural.brokenTrianglesToComponentIdMap.emplace(key, owner);
            for (const auto& seam : original.seamTriangleUvs) {
                decltype(original.seamTriangleUvs)::value_type filtered;
                for (const auto& key : seam.first)
                    if (allowed.count(key))
                        filtered.first.insert(key);
                for (const auto& key : seam.second)
                    if (allowed.count(key))
                        filtered.second.insert(key);
                if (!filtered.first.empty() || !filtered.second.empty())
                    procedural.seamTriangleUvs.push_back(std::move(filtered));
            }
        }
        TextureImageView borrowed(const TextureImage& image)
        {
            return { image.width, image.height, image.width * 4, image.pixels.size(), image.pixels.data() };
        }
    }
    void appendSourceMipFallbackWarning(std::string& warning, const char* diagnostic, RetainedPayloadBudget& output)
    {
        if (!diagnostic || !*diagnostic)
            return;
        const size_t previousCapacity = warning.capacity();
        if (!warning.empty())
            warning += ' ';
        warning += diagnostic;
        if (warning.capacity() > previousCapacity)
            charge(output, warning.capacity() - previousCapacity);
    }
    SourceBakedData::SourceBakedData(const Dust3DSourceBakeOptions& options, ImportedModels& imports, size_t maximumMipLevels)
        : m_options(options)
        , m_maximumOutputBytes(quota(options.maximum_output_payload_bytes, maximumBytes))
        , m_maximumMipLevels(maximumMipLevels)
        , m_imports(&imports)
    {
        if (maximumMipLevels < 1 || maximumMipLevels > 14)
            invalid("Source bake mip level limit must be in 1..14.");
        if (options.struct_size != sizeof(options))
            invalid("Source baking requires correctly sized options.");
        m_options.maximum_output_payload_bytes = m_maximumOutputBytes;
        m_options.maximum_sampler_payload_bytes = quota(options.maximum_sampler_payload_bytes, 64 * 1024 * 1024);
        m_options.maximum_atlas_payload_bytes = quota(options.maximum_atlas_payload_bytes, 64 * 1024 * 1024);
        m_options.maximum_chart_count = options.maximum_chart_count ? options.maximum_chart_count : 50000;
        m_options.maximum_raster_samples = options.maximum_raster_samples ? options.maximum_raster_samples : 250000000;
        if (m_options.maximum_chart_count > 100000 || m_options.maximum_raster_samples > 1000000000
            || options.model_image_count > 10000 || (options.model_image_count && !options.model_images))
            invalid("Source bake image/chart/work limits are invalid.");
        m_legacy = std::make_unique<BakedMeshData>(options.legacy);
        m_options.legacy = m_legacy->options();
        for (size_t index = 0; index < options.model_image_count; ++index) {
            checkCancellation(cancellation());
            const auto& image = options.model_images[index];
            if (!image.model_id || !image.model_id_length || image.model_id_length > 128 || std::memchr(image.model_id, 0, image.model_id_length))
                invalid("Source bake image model identities require 1..128 exact non-NUL bytes.");
            std::string identity(image.model_id, image.model_id_length);
            if (!m_modelImages.emplace(std::move(identity), view(image.image)).second)
                invalid("Source bake decoded model image identities must be unique.");
        }
        m_options.model_images = nullptr;
        m_options.model_image_count = 0;
    }
    void SourceBakedData::validateVisuals(const Snapshot& snapshot) const
    {
        for (const auto& [identity, component] : snapshot.components) {
            (void)identity;
            checkCancellation(cancellation());
            const auto part = snapshot.parts.find(String::valueOrEmpty(component, "linkData"));
            if (String::valueOrEmpty(component, "linkDataType") == "partId" && part != snapshot.parts.end()
                && String::valueOrEmpty(part->second, "target") == "ImportedModel" && !String::valueOrEmpty(component, "colorImageId").empty())
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Imported component color-image overrides are not yet supported by source baking.");
        }
        m_legacy->validateVisuals(snapshot);
    }
    void SourceBakedData::prepare(const Object& object, const Snapshot& snapshot, Dust3DMesh& geometry, const ImportedSurfaceData& sources)
    {
        checkCancellation(cancellation());
        const size_t count = m_originalIndices.size();
        if (sources.triangles().size() != count || geometry.vertices.size() != count * 3)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Source bake geometry and retained source records are not aligned.");
        RetainedPayloadBudget output(m_maximumOutputBytes);
        charge(output, sources.payloadBytes());
        charge(output, 1, sizeof(Dust3DSourceBakedMesh) + sizeof(SourceBakedData));
        charge(output, count, sizeof(Dust3DSourceBakedTriangle));
        triangles.reserve(count);
        charge(output, triangles.capacity() - count, sizeof(Dust3DSourceBakedTriangle));
        const size_t remaining = m_maximumOutputBytes - output.usedBytes();
        if (!remaining)
            limit("Source bake has no retained budget left for maps.");
        SourceTextureBakeInputs inputs;
        std::map<size_t, SourceColorEncoding> used;
        for (const auto& triangle : sources.triangles()) {
            checkCancellation(cancellation());
            if (triangle.model_index != SIZE_MAX) {
                const auto encoding = triangle.color_encoding == DUST3D_SURFACE_COLOR_LINEAR ? SourceColorEncoding::linear : SourceColorEncoding::srgb;
                const auto [found, inserted] = used.emplace(triangle.model_index, encoding);
                if (!inserted && found->second != encoding)
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "A generated model has inconsistent source color encoding.");
            }
        }
        auto assets = m_imports->takeAssets();
        for (const auto& [identity, pixels] : m_modelImages) {
            (void)pixels;
            const auto found = assets.find(identity);
            if (found == assets.end() || !found->second.image)
                throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Decoded source image has no exact textured model owner.");
        }
        std::map<std::string, ColorTextureSampler> samplers;
        size_t samplerBytes = 0;
        for (const auto& [index, encoding] : used) {
            checkCancellation(cancellation());
            const auto& identity = sources.models().at(index);
            const auto& asset = assets.at(identity);
            SourceTextureMaterial material;
            material.baseColor = asset.baseColor;
            material.metallic = asset.metallic;
            material.roughness = asset.roughness;
            material.emissive = asset.emissive;
            material.policy = { asset.alphaMode == GlbAlphaMode::Opaque ? SourceTextureAlphaMode::opaque : asset.alphaMode == GlbAlphaMode::Mask ? SourceTextureAlphaMode::mask
                                                                                                                                                 : SourceTextureAlphaMode::blend,
                asset.alphaCutoff, asset.isDoubleSided };
            material.vertexColorEncoding = encoding;
            if (asset.image) {
                const auto pixels = m_modelImages.find(identity);
                if (pixels == m_modelImages.end())
                    throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "A textured imported model has no resolved decoded RGBA image.");
                ColorTextureSamplingOptions sampling;
                if (asset.sampler) {
                    sampling.wrapS = wrapping(asset.sampler->wrapS);
                    sampling.wrapT = wrapping(asset.sampler->wrapT);
                    if (asset.sampler->magnification) {
                        if (*asset.sampler->magnification != 9728 && *asset.sampler->magnification != 9729)
                            throw DocumentError(DUST3D_STATUS_UNSUPPORTED_ASSET, "Unsupported imported magnification filter.");
                        sampling.magnification = *asset.sampler->magnification == 9728 ? ColorTextureMagnificationFilter::nearest : ColorTextureMagnificationFilter::linear;
                    }
                    if (asset.sampler->minification)
                        sampling.minification = minification(*asset.sampler->minification);
                }
                const size_t available = m_options.maximum_sampler_payload_bytes - samplerBytes;
                if (!available)
                    limit("Source bake sampler payload quota is exhausted.");
                sampling.maximumPayloadBytes = available;
                sampling.cancellation = cancellation() ? &cancellation()->cancelled : nullptr;
                auto sampler = ColorTextureSampler::create(pixels->second, sampling);
                samplerBytes += sampler.payloadBytes();
                auto inserted = samplers.emplace(identity, std::move(sampler));
                material.colorTexture = &inserted.first->second;
            }
            inputs.materials.emplace(identity, material);
        }
        Object filtered;
        filtered.triangleCornerUvs.resize(count);
        for (size_t index = 0; index < count; ++index) {
            checkCancellation(cancellation());
            const auto& source = sources.triangles()[index];
            filtered.triangleModelKeys.push_back(source.model_index == SIZE_MAX ? "" : sources.models().at(source.model_index));
            filtered.triangleComponentKeys.push_back(source.component_index == SIZE_MAX ? "" : sources.components().at(source.component_index).identity);
            if (source.uv_known) {
                std::array<Vector2, 3> uvs;
                for (size_t corner = 0; corner < 3; ++corner)
                    uvs[corner] = { source.uv[corner][0], source.uv[corner][1] };
                filtered.triangleCornerUvs[index] = uvs;
            }
            std::array<Color, 3> colors;
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& vertex = geometry.vertices[index * 3 + corner];
                filtered.vertices.emplace_back(vertex.position[0], vertex.position[1], vertex.position[2]);
                colors[corner] = { source.color[corner][0], source.color[corner][1], source.color[corner][2], source.color[corner][3] };
            }
            filtered.triangles.push_back({ index * 3, index * 3 + 1, index * 3 + 2 });
            inputs.triangleColors.push_back(colors);
        }
        Object procedural;
        std::vector<size_t> proceduralToRetained;
        populateProcedural(object, m_originalIndices, sources, procedural, proceduralToRetained, cancellation());
        UvAtlas legacy;
        legacy.triangleUvs.resize(count);
        legacy.triangleUvKnown.resize(count);
        std::optional<TextureBakeResult> legacyBake;
        if (!procedural.triangles.empty()) {
            UvAtlasOptions atlasOptions;
            atlasOptions.textureSize = m_options.legacy.atlas_texture_size;
            atlasOptions.maximumPayloadBytes = m_options.legacy.maximum_uv_payload_bytes;
            auto proceduralAtlas = UvAtlasBuilder::build(procedural, resolvedUvComponents(snapshot, m_legacy->images()), atlasOptions);
            legacy.packedTextureSize = proceduralAtlas.packedTextureSize;
            for (size_t index = 0; index < proceduralToRetained.size(); ++index) {
                legacy.triangleUvs[proceduralToRetained[index]] = proceduralAtlas.triangleUvs[index];
                legacy.triangleUvKnown[proceduralToRetained[index]] = proceduralAtlas.triangleUvKnown[index];
            }
            if (std::any_of(proceduralAtlas.triangleUvKnown.begin(), proceduralAtlas.triangleUvKnown.end(), [](uint8_t known) { return known != 0; })) {
                TextureBakeOptions bakeOptions;
                bakeOptions.colorSize = m_options.legacy.color_size;
                bakeOptions.normalSize = m_options.legacy.normal_size;
                bakeOptions.materialSize = m_options.legacy.material_size;
                bakeOptions.maximumSpatialGridEntries = m_options.legacy.maximum_spatial_grid_entries;
                bakeOptions.maximumPayloadBytes = m_options.legacy.maximum_bake_payload_bytes;
                bakeOptions.cancellation = cancellation() ? &cancellation()->cancelled : nullptr;
                legacyBake = TextureBaker::bake(procedural, proceduralAtlas, m_legacy->resolvedInputs(snapshot), bakeOptions);
                inputs.legacy.color = borrowed(legacyBake->color);
                if (legacyBake->normal)
                    inputs.legacy.normal = borrowed(*legacyBake->normal);
                if (legacyBake->metallic)
                    inputs.legacy.metallic = borrowed(*legacyBake->metallic);
                if (legacyBake->roughness)
                    inputs.legacy.roughness = borrowed(*legacyBake->roughness);
                if (legacyBake->emissive)
                    inputs.legacy.emissive = borrowed(*legacyBake->emissive);
            }
            if (m_maximumMipLevels > 1)
                legacy.layouts = std::move(proceduralAtlas.layouts);
        }
        SourceUvAtlasOptions atlasOptions;
        atlasOptions.textureSize = m_options.legacy.atlas_texture_size;
        atlasOptions.maximumPayloadBytes = m_options.maximum_atlas_payload_bytes;
        atlasOptions.maximumChartCount = m_options.maximum_chart_count;
        atlasOptions.cancellation = cancellation() ? &cancellation()->cancelled : nullptr;
        const auto atlas = SourceUvAtlasBuilder::build(filtered, legacy, atlasOptions);
        SourceTextureBakeOptions bakeOptions;
        bakeOptions.colorSize = m_options.legacy.color_size;
        bakeOptions.materialSize = m_options.legacy.material_size;
        bakeOptions.normalSize = m_options.legacy.normal_size;
        bakeOptions.maximumChartCount = m_options.maximum_chart_count;
        bakeOptions.maximumRasterSamples = m_options.maximum_raster_samples;
        bakeOptions.cancellation = atlasOptions.cancellation;
        RetainedPayloadBudget plannedMaps(remaining);
        charge(plannedMaps, bakeOptions.colorSize * bakeOptions.colorSize, 4);
        charge(plannedMaps, bakeOptions.materialSize * bakeOptions.materialSize, 12);
        if (inputs.legacy.normal)
            charge(plannedMaps, bakeOptions.normalSize * bakeOptions.normalSize, 4);
        // C++ policies are temporary workspace; account actual final pixel capacities
        // against the native aggregate instead of guessing STL growth behavior.
        bakeOptions.maximumPayloadBytes = maximumBytes;
        SourceTextureBakeResult result;
        const char* mipFallbackDiagnostic = "";
        if (m_maximumMipLevels > 1) {
            SourceTextureMipBakeOptions mipOptions;
            mipOptions.bake = bakeOptions;
            mipOptions.maximumLevels = m_maximumMipLevels;
            mipOptions.legacyAtlas = &legacy;
            auto chain = SourceTextureBaker::bakeMipChain(atlas, inputs, mipOptions);
            mipFallbackDiagnostic = chain.fallbackDiagnostic();
            maximumSamplingLod = chain.maximumSamplingLod;
            result = std::move(chain.base);
            mipLevels[0] = std::move(chain.colorLevels);
            mipLevels[1] = std::move(chain.normalLevels);
            mipLevels[2] = std::move(chain.metallicLevels);
            mipLevels[3] = std::move(chain.roughnessLevels);
            mipLevels[4] = std::move(chain.emissiveLevels);
        } else {
            result = SourceTextureBaker::bake(atlas, inputs, bakeOptions);
            maximumSamplingLod = result.maximumSamplingLod;
        }
        inputs = {};
        samplers.clear();
        assets.clear();
        legacyBake.reset();
        UvAtlas packed;
        packed.triangleUvs.reserve(count);
        packed.triangleUvKnown.reserve(count);
        for (const auto& triangle : atlas.triangles) {
            packed.triangleUvs.push_back(triangle.packedUvs);
            packed.triangleUvKnown.push_back(triangle.known);
        }
        TextureBakeOptions tangentOptions;
        tangentOptions.cancellation = atlasOptions.cancellation;
        const auto tangents = detail::buildTextureTangents(filtered, packed, tangentOptions);
        for (size_t index = 0; index < count; ++index) {
            checkCancellation(cancellation());
            Dust3DSourceBakedTriangle triangle {};
            triangle.uv_known = atlas.triangles[index].known;
            triangle.model_index = sources.triangles()[index].model_index;
            triangle.component_index = sources.triangles()[index].component_index;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t axis = 0; axis < 2; ++axis)
                    triangle.uv[corner][axis] = atlas.triangles[index].packedUvs[corner][axis];
            if (const auto& policy = result.trianglePolicies[index]) {
                triangle.policy_known = 1;
                triangle.alpha_mode = policy->alphaMode == SourceTextureAlphaMode::opaque ? DUST3D_IMPORTED_ALPHA_OPAQUE : policy->alphaMode == SourceTextureAlphaMode::mask ? DUST3D_IMPORTED_ALPHA_MASK
                                                                                                                                                                             : DUST3D_IMPORTED_ALPHA_BLEND;
                triangle.alpha_cutoff = policy->alphaCutoff;
                triangle.is_double_sided = policy->isDoubleSided;
            }
            const auto& frame = tangents[index];
            if (!std::isfinite(frame.handedness) || (frame.known && (std::abs(frame.handedness) != 1 || !std::isfinite(frame.direction.lengthSquared()) || std::abs(frame.direction.lengthSquared() - 1) > 1e-6)))
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Source bake produced an invalid tangent frame.");
            for (size_t axis = 0; axis < 3; ++axis)
                if (!std::isfinite(frame.direction[axis]) || std::abs(frame.direction[axis]) > std::numeric_limits<float>::max())
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "Source bake tangent exceeds the finite ABI range.");
            triangle.tangent.known = tangents[index].known;
            triangle.tangent.handedness = static_cast<float>(tangents[index].handedness);
            for (size_t axis = 0; axis < 3; ++axis)
                triangle.tangent.direction[axis] = static_cast<float>(tangents[index].direction[axis]);
            triangles.push_back(triangle);
            if (triangle.uv_known)
                for (size_t corner = 0; corner < 3; ++corner)
                    for (size_t channel = 0; channel < 4; ++channel)
                        geometry.vertices[index * 3 + corner].color[channel] = 1;
        }
        maps[0] = std::move(result.color);
        maps[1] = std::move(result.normal);
        maps[2] = std::move(result.metallic);
        maps[3] = std::move(result.roughness);
        maps[4] = std::move(result.emissive);
        for (const auto& image : maps)
            if (image)
                charge(output, image->pixels.capacity());
        for (const auto& levels : mipLevels) {
            charge(output, levels.capacity(), sizeof(TextureImage));
            for (const auto& image : levels)
                charge(output, image.pixels.capacity());
        }
        const std::string obsolete = "Imported geometry preview: GLB materials, textures and transparency are not yet applied.";
        const auto notice = geometry.warning.find(obsolete);
        if (notice != std::string::npos)
            geometry.warning.erase(notice, obsolete.size());
        while (!geometry.warning.empty() && geometry.warning.back() == ' ')
            geometry.warning.pop_back();
        appendSourceMipFallbackWarning(geometry.warning, mipFallbackDiagnostic, output);
        payloadBytes = output.usedBytes();
        checkCancellation(cancellation());
        m_legacy.reset();
        m_imports = nullptr;
        m_modelImages.clear();
        std::vector<size_t>().swap(m_originalIndices);
        m_options = {};
    }
}
}
static Dust3DSourceBakedMesh* loadSourceBakedMesh(const uint8_t* bytes, size_t size, const Dust3DSourceBakeOptions* options, size_t maximumLevels, Dust3DError* error)
{
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!options || options->struct_size != sizeof(*options))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Source baking requires correctly sized options.");
        std::lock_guard<std::mutex> lock(engineMutex());
        ImportedModels imports;
        auto owner = std::make_unique<Dust3DSourceBakedMesh>();
        owner->data = std::make_unique<SourceBakedData>(*options, imports, maximumLevels);
        std::string warning;
        auto snapshot = readDocument(bytes, size, warning, nullptr, &imports);
        owner->data->validateVisuals(*snapshot);
        owner->source = std::make_unique<Dust3DImportedSurfaceMesh>();
        owner->source->data = std::make_unique<ImportedSurfaceData>(*snapshot, imports, owner->data->maximumOutputBytes());
        GenerationOutputs outputs;
        outputs.surfaces = owner->source->data.get();
        outputs.sourceBaked = owner->data.get();
        owner->source->geometry = generateMesh(std::move(snapshot), warning, &imports, nullptr, nullptr, outputs);
        checkCancellation(options->legacy.cancellation);
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const dust3d::TextureBakeError& cause) {
        auto status = cause.code() == dust3d::TextureBakeErrorCode::cancelled ? DUST3D_STATUS_CANCELLED : cause.code() == dust3d::TextureBakeErrorCode::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == dust3d::TextureBakeErrorCode::invalidInput                                                                                              ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                                                                                                                      : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const dust3d::UvAtlasError& cause) {
        auto status = cause.code() == dust3d::UvAtlasErrorCode::cancelled ? DUST3D_STATUS_CANCELLED : cause.code() == dust3d::UvAtlasErrorCode::limitExceeded ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == dust3d::UvAtlasErrorCode::invalidInput                                                                                          ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                                                                                                              : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown native source-bake failure.");
    }
    return nullptr;
}
Dust3DSourceBakedMesh* dust3d_source_baked_mesh_load(const uint8_t* bytes, size_t size, const Dust3DSourceBakeOptions* options, Dust3DError* error)
{
    return loadSourceBakedMesh(bytes, size, options, 1, error);
}
Dust3DSourceBakedMesh* dust3d_source_baked_mesh_load_with_mips(const uint8_t* bytes, size_t size, const Dust3DSourceBakeMipOptions* options, Dust3DError* error)
{
    if (!options || options->struct_size != sizeof(*options)) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Source mip baking requires correctly sized options.");
        return nullptr;
    }
    return loadSourceBakedMesh(bytes, size, &options->source, options->maximum_levels ? options->maximum_levels : 14, error);
}
size_t dust3d_source_baked_mesh_texture_level_count(const Dust3DSourceBakedMesh* mesh, uint32_t map)
{
    return mesh && map < mesh->data->maps.size() && mesh->data->maps[map] ? 1 + mesh->data->mipLevels[map].size() : 0;
}
Dust3DTextureView dust3d_source_baked_mesh_texture_level(const Dust3DSourceBakedMesh* mesh, uint32_t map, size_t level)
{
    if (level >= dust3d_source_baked_mesh_texture_level_count(mesh, map))
        return {};
    if (!level)
        return dust3d_source_baked_mesh_texture(mesh, map);
    const auto& image = mesh->data->mipLevels[map][level - 1];
    return { image.width, image.height, image.width * 4, image.pixels.size(), image.pixels.data() };
}
void dust3d_source_baked_mesh_destroy(Dust3DSourceBakedMesh* mesh) { delete mesh; }
const Dust3DMesh* dust3d_source_baked_mesh_geometry(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->source->geometry.get() : nullptr; }
const Dust3DSourceBakedTriangle* dust3d_source_baked_mesh_triangles(const Dust3DSourceBakedMesh* mesh) { return mesh && !mesh->data->triangles.empty() ? mesh->data->triangles.data() : nullptr; }
size_t dust3d_source_baked_mesh_triangle_count(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->data->triangles.size() : 0; }
size_t dust3d_source_baked_mesh_payload_bytes(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->data->payloadBytes : 0; }
size_t dust3d_source_baked_mesh_model_count(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->source->data->models().size() : 0; }
size_t dust3d_source_baked_mesh_component_count(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->source->data->components().size() : 0; }
const char* dust3d_source_baked_mesh_model_id(const Dust3DSourceBakedMesh* mesh, size_t index) { return mesh && index < mesh->source->data->models().size() ? mesh->source->data->models()[index].c_str() : ""; }
const char* dust3d_source_baked_mesh_component_id(const Dust3DSourceBakedMesh* mesh, size_t index) { return mesh && index < mesh->source->data->components().size() ? mesh->source->data->components()[index].identity.c_str() : ""; }
Dust3DSurfaceComponentKind dust3d_source_baked_mesh_component_kind(const Dust3DSourceBakedMesh* mesh, size_t index) { return mesh && index < mesh->source->data->components().size() ? mesh->source->data->components()[index].kind : DUST3D_SURFACE_COMPONENT_UNKNOWN; }
double dust3d_source_baked_mesh_maximum_sampling_lod(const Dust3DSourceBakedMesh* mesh) { return mesh ? mesh->data->maximumSamplingLod : 0; }
Dust3DTextureView dust3d_source_baked_mesh_texture(const Dust3DSourceBakedMesh* mesh, uint32_t map)
{
    if (!mesh || map >= mesh->data->maps.size() || !mesh->data->maps[map])
        return {};
    const auto& image = *mesh->data->maps[map];
    return { image.width, image.height, image.width * 4, image.pixels.size(), image.pixels.data() };
}

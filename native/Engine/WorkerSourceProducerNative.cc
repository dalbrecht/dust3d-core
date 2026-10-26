// SPDX-License-Identifier: MIT
#include "WorkerSourceProducerNative.h"
#include "EngineInternal.h"
#include "WorkerImportedMaterialsCopy.h"
#include "WorkerOperation.h"
#include "WorkerProceduralImagesNative.h"
#include "WorkerSourceBakedCopy.h"
#include "WorkerSourceNativeFailure.h"
#include <algorithm>
#include <cstring>
#include <memory>
namespace dust3d {
namespace worker {
    FailurePayload copySourceNativeFailure(Dust3DStatus status, std::string_view cause)
    {
        Dust3DError error {};
        error.status = status;
        auto failure = copyNativeFailure(error);
        const FailurePayloadLimits limits;
        const auto count = static_cast<size_t>(std::min({ uint64_t(cause.size()), limits.maximumCauseBytes, limits.maximumWireBytes - detailedFailurePrefixBytes }));
        failure.cause.clear();
        if (count)
            failure.cause.assign(cause.begin(), cause.begin() + count);
        failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(status), static_cast<uint32_t>(status), static_cast<uint8_t>(count != cause.size()) };
        return failure;
    }
    SourceBakedPayload generateSourceBakedRender(const std::vector<uint8_t>& archive,
        ProceduralImageDecoder* proceduralDecoder, ImportedRenderImageDecoder* importedDecoder,
        const NativeSourceProducerOptions& options)
    {
        validateNativeSourceProducerOptions(options);
        if (archive.empty() || archive.size() > maximumPayloadBytes)
            throw LegacyProducerError(LegacyProducerErrorCode::invalidLimits, "Source bake archive exceeds supported positive bounds.");
        std::vector<ProceduralImageInput> inputs;
        try {
            inputs = loadProceduralImageInputs(archive, options.preparation.procedural.images);
        } catch (const native::DocumentError& cause) {
            throw LegacyProducerNativeError(copySourceNativeFailure(cause.status(), cause.what()));
        }
        Dust3DError error {};
        const auto fail = [&] {
            auto failure = copyNativeFailure(error);
            failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(error.status), static_cast<uint32_t>(error.status), 0 };
            throw LegacyProducerNativeError(std::move(failure));
        };
        Dust3DImportedAssetOptions materialOptions {};
        materialOptions.struct_size = sizeof(materialOptions);
        materialOptions.maximum_payload_bytes = static_cast<size_t>(options.maximumMaterialNativeBytes);
        std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> assets(
            dust3d_imported_assets_load(archive.data(), archive.size(), &materialOptions, &error), dust3d_imported_assets_destroy);
        if (!assets)
            fail();
        auto facts = copyNativeImportedMaterials(assets.get(), options.preparation.materials);
        assets.reset();
        const auto decoded = prepareSourceBakeImages(inputs, facts, proceduralDecoder, importedDecoder, options.preparation);
        std::vector<Dust3DBakeImage> views;
        views.reserve(inputs.size());
        for (size_t index = 0; index < inputs.size(); ++index) {
            const auto& input = inputs[index];
            const auto& image = decoded.procedural[index];
            Dust3DBakeImage view {};
            static_assert(sizeof(view.image_id) >= 39, "Canonical procedural identities require 38 bytes plus NUL.");
            std::memcpy(view.image_id, input.identity.data(), input.identity.size());
            view.width = image.width;
            view.height = image.height;
            view.row_stride = static_cast<size_t>(image.width) * 4;
            view.byte_count = image.pixels.size();
            view.pixels = image.pixels.data();
            views.push_back(view);
        }
        Dust3DBakeOptions nativeOptions {};
        nativeOptions.struct_size = sizeof(nativeOptions);
        nativeOptions.maximum_uv_payload_bytes = static_cast<size_t>(options.preparation.procedural.maximumUvNativeBytes);
        nativeOptions.maximum_bake_payload_bytes = static_cast<size_t>(options.preparation.procedural.maximumBakeNativeBytes);
        nativeOptions.atlas_texture_size = static_cast<size_t>(options.preparation.procedural.atlasTextureSize ? options.preparation.procedural.atlasTextureSize : std::min(uint64_t(4096), options.preparation.procedural.maximumPermittedTextureDimension));
        nativeOptions.color_size = static_cast<size_t>(options.preparation.procedural.colorSize ? options.preparation.procedural.colorSize : std::min(uint64_t(4096), options.preparation.procedural.maximumPermittedTextureDimension));
        nativeOptions.normal_size = static_cast<size_t>(options.preparation.procedural.normalSize ? options.preparation.procedural.normalSize : std::min(uint64_t(2048), options.preparation.procedural.maximumPermittedTextureDimension));
        nativeOptions.material_size = static_cast<size_t>(options.preparation.procedural.materialSize ? options.preparation.procedural.materialSize : std::min(uint64_t(1024), options.preparation.procedural.maximumPermittedTextureDimension));
        nativeOptions.maximum_spatial_grid_entries = static_cast<size_t>(options.preparation.procedural.maximumSpatialGridEntries ? options.preparation.procedural.maximumSpatialGridEntries : std::min(uint64_t(1000000), options.preparation.procedural.maximumPermittedSpatialGridEntries));
        nativeOptions.images = views.data();
        nativeOptions.image_count = views.size();

        std::vector<Dust3DModelBakeImage> modelViews;
        modelViews.reserve(decoded.models.size());
        for (const auto& model : decoded.models) {
            Dust3DModelBakeImage view {};
            view.model_id = model.modelId.data();
            view.model_id_length = model.modelId.size();
            view.image.width = model.image.width;
            view.image.height = model.image.height;
            view.image.row_stride = static_cast<size_t>(model.image.width) * 4;
            view.image.pixels = model.image.pixels.data();
            view.image.byte_count = model.image.pixels.size();
            modelViews.push_back(view);
        }
        Dust3DSourceBakeMipOptions mipOptions {};
        mipOptions.struct_size = sizeof(mipOptions);
        mipOptions.maximum_levels = static_cast<size_t>(options.maximumLevels ? options.maximumLevels : options.maximumPermittedLevels);
        auto& source = mipOptions.source;
        source.struct_size = sizeof(source);
        source.legacy = nativeOptions;
        source.model_images = modelViews.data();
        source.model_image_count = modelViews.size();
        source.maximum_sampler_payload_bytes = static_cast<size_t>(options.maximumSamplerNativeBytes);
        source.maximum_atlas_payload_bytes = static_cast<size_t>(options.maximumAtlasNativeBytes);
        source.maximum_output_payload_bytes = static_cast<size_t>(options.maximumOutputNativeBytes);
        source.maximum_chart_count = static_cast<size_t>(options.maximumChartCount ? options.maximumChartCount : std::min(uint64_t(50000), options.maximumPermittedChartCount));
        source.maximum_raster_samples = static_cast<size_t>(options.maximumRasterSamples ? options.maximumRasterSamples : std::min(uint64_t(250000000), options.maximumPermittedRasterSamples));
        std::unique_ptr<Dust3DSourceBakedMesh, decltype(&dust3d_source_baked_mesh_destroy)> owner(
            options.shouldGenerateMips ? dust3d_source_baked_mesh_load_with_mips(archive.data(), archive.size(), &mipOptions, &error)
                                       : dust3d_source_baked_mesh_load(archive.data(), archive.size(), &source, &error),
            dust3d_source_baked_mesh_destroy);
        if (!owner)
            fail();
        return copyNativeSourceBakedMesh(owner.get(), options.output);
    }
}
}

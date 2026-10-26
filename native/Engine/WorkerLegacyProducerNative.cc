// SPDX-License-Identifier: MIT
#include "WorkerLegacyProducerNative.h"
#include "EngineInternal.h"
#include "WorkerLegacyBakedCopy.h"
#include "WorkerOperation.h"
#include "WorkerProceduralImagesNative.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
namespace dust3d {
namespace worker {
    LegacyProducerNativeError::LegacyProducerNativeError(FailurePayload failure)
        : std::runtime_error(std::string(failure.cause.begin(), failure.cause.end()))
        , m_failure(std::move(failure))
    {
    }
    LegacyBakedPayload generateLegacyBakedRender(const std::vector<uint8_t>& archive, ProceduralImageDecoder* decoder, const LegacyProducerOptions& options)
    {
        static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max(), "Validated native quotas must fit size_t.");
        validateLegacyProducerOptions(options);
        std::vector<ProceduralImageInput> inputs;
        try {
            inputs = loadProceduralImageInputs(archive, options.images);
        } catch (const native::DocumentError& cause) {
            Dust3DError error {};
            native::setError(&error, cause.status(), cause.what());
            auto failure = copyNativeFailure(error);
            failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(error.status), static_cast<uint32_t>(error.status),
                static_cast<uint8_t>(std::strlen(cause.what()) >= sizeof(error.message)) };
            throw LegacyProducerNativeError(std::move(failure));
        }
        const auto decoded = prepareProceduralImagePixels(inputs, decoder, options);
        std::vector<Dust3DBakeImage> views;
        views.reserve(inputs.size());
        for (size_t index = 0; index < inputs.size(); ++index) {
            const auto& input = inputs[index];
            const auto& image = decoded[index];
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
        nativeOptions.maximum_uv_payload_bytes = static_cast<size_t>(options.maximumUvNativeBytes);
        nativeOptions.maximum_bake_payload_bytes = static_cast<size_t>(options.maximumBakeNativeBytes);
        nativeOptions.atlas_texture_size = static_cast<size_t>(options.atlasTextureSize ? options.atlasTextureSize : std::min(uint64_t(4096), options.maximumPermittedTextureDimension));
        nativeOptions.color_size = static_cast<size_t>(options.colorSize ? options.colorSize : std::min(uint64_t(4096), options.maximumPermittedTextureDimension));
        nativeOptions.normal_size = static_cast<size_t>(options.normalSize ? options.normalSize : std::min(uint64_t(2048), options.maximumPermittedTextureDimension));
        nativeOptions.material_size = static_cast<size_t>(options.materialSize ? options.materialSize : std::min(uint64_t(1024), options.maximumPermittedTextureDimension));
        nativeOptions.maximum_spatial_grid_entries = static_cast<size_t>(options.maximumSpatialGridEntries ? options.maximumSpatialGridEntries : std::min(uint64_t(1000000), options.maximumPermittedSpatialGridEntries));
        nativeOptions.images = views.data();
        nativeOptions.image_count = views.size();
        Dust3DError error {};
        std::unique_ptr<Dust3DBakedMesh, decltype(&dust3d_baked_mesh_destroy)> owner(
            dust3d_baked_mesh_load(archive.data(), archive.size(), &nativeOptions, &error), dust3d_baked_mesh_destroy);
        if (!owner) {
            auto failure = copyNativeFailure(error);
            failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(error.status), static_cast<uint32_t>(error.status), 0 };
            throw LegacyProducerNativeError(std::move(failure));
        }
        return copyNativeLegacyBakedMesh(owner.get(), options.output);
    }
}
}

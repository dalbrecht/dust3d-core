// SPDX-License-Identifier: MIT
#include "WorkerImportedProducerNative.h"
#include "WorkerImportedMaterialsCopy.h"
#include "WorkerImportedSurfaceCopy.h"
#include "WorkerOperation.h"
#include <memory>
namespace dust3d {
namespace worker {
    ImportedProducerNativeError::ImportedProducerNativeError(FailurePayload failure)
        : std::runtime_error(std::string(failure.cause.begin(), failure.cause.end()))
        , m_failure(std::move(failure))
    {
    }
    std::optional<ImportedRenderPayload> generateImportedRender(const std::vector<uint8_t>& archive,
        ImportedRenderImageDecoder& decoder, const NativeImportedProducerLimits& limits)
    {
        validateNativeImportedProducerLimits(limits);
        if (archive.empty() || archive.size() > maximumPayloadBytes)
            throw ImportedProducerError(ImportedProducerErrorCode::invalidLimits, "Imported producer archive length is outside supported positive bounds.");
        Dust3DError error {};
        Dust3DImportedAssetOptions materialOptions {};
        materialOptions.struct_size = sizeof(materialOptions);
        materialOptions.maximum_payload_bytes = static_cast<size_t>(limits.maximumMaterialNativeBytes);
        std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> assets(
            dust3d_imported_assets_load(archive.data(), archive.size(), &materialOptions, &error), dust3d_imported_assets_destroy);
        if (!assets) {
            auto failure = copyNativeFailure(error);
            failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(error.status), static_cast<uint32_t>(error.status), 0 };
            throw ImportedProducerNativeError(std::move(failure));
        }
        auto facts = copyNativeImportedMaterials(assets.get(), limits.preparation.facts);
        assets.reset();
        if (facts.materials.empty())
            return std::nullopt;
        Dust3DImportedSurfaceOptions surfaceOptions {};
        surfaceOptions.struct_size = sizeof(surfaceOptions);
        surfaceOptions.maximum_payload_bytes = static_cast<size_t>(limits.maximumSurfaceNativeBytes);
        std::unique_ptr<Dust3DImportedSurfaceMesh, decltype(&dust3d_imported_surface_mesh_destroy)> surface(
            dust3d_imported_surface_mesh_load(archive.data(), archive.size(), &surfaceOptions, &error), dust3d_imported_surface_mesh_destroy);
        if (!surface) {
            auto failure = copyNativeFailure(error);
            failure.detail = FailureDetail { FailureDomain::native, static_cast<int32_t>(error.status), static_cast<uint32_t>(error.status), 0 };
            throw ImportedProducerNativeError(std::move(failure));
        }
        auto geometry = copyNativeImportedSurface(surface.get(), limits.preparation.render.surface);
        surface.reset();
        return prepareImportedRender(std::move(geometry), std::move(facts), decoder, limits.preparation);
    }
}
}

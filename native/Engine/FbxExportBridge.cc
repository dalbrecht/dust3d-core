// SPDX-License-Identifier: MIT
#include "BakedMesh.h"
#include "FbxBakedMaterial.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include <dust3d/base/retained_payload_budget.h>

struct Dust3DFbxExport {
    dust3d::native::OwnedFbxDocument document;
    std::string warning;
    size_t payloadBytes = 0;
};

Dust3DFbxExport* dust3d_fbx_export_load(const uint8_t* bytes, size_t size, const Dust3DFbxExportOptions* options, Dust3DError* error)
{
    using namespace dust3d;
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!options || options->struct_size != sizeof(*options))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX export requires correctly sized options.");
        if (options->mode != DUST3D_FBX_EXPORT_GEOMETRY && options->mode != DUST3D_FBX_EXPORT_BAKED)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unknown static FBX export mode.");
        planFbxGeometrySelection(1, 1, 1, false, false, options->maximum_geometry_payload_bytes);
        const auto maximumPng = options->maximum_png_payload_bytes ? options->maximum_png_payload_bytes : 64 * 1024 * 1024;
        if (maximumPng < 57 || maximumPng > 64 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX PNG capacity quota must be57bytes...64MiB.");
        const auto maximumOutput = options->maximum_output_payload_bytes ? options->maximum_output_payload_bytes : 64 * 1024 * 1024;
        if (maximumOutput <= sizeof(Dust3DFbxExport) + 1 || maximumOutput > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX final output quota exceeds supported bounds.");
        checkCancellation(options->cancellation);
        std::unique_ptr<BakedMeshData> bake;
        if (options->mode == DUST3D_FBX_EXPORT_BAKED) {
            if (options->bake.cancellation && options->bake.cancellation != options->cancellation)
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX bake and export cancellation tokens must agree.");
            auto settings = options->bake;
            settings.cancellation = options->cancellation;
            bake = std::make_unique<BakedMeshData>(settings);
        }
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(options->cancellation);
        std::string warning;
        // No import owner is supplied: model assets reject before GLB decoding.
        auto snapshot = readDocument(bytes, size, warning);
        RetainedPayloadBudget overhead(maximumOutput);
        overhead.tryAdd(sizeof(Dust3DFbxExport));
        overhead.accountDynamic(warning);
        if (overhead.isExceeded() || overhead.usedBytes() >= maximumOutput)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX owner and warning exceed final output quota.");
        const FbxDocumentLimits limits { maximumOutput - overhead.usedBytes() + sizeof(OwnedFbxDocument), 65536 };
        std::unique_ptr<UvMetadata> uvs;
        std::unique_ptr<FbxGeometrySelection> geometry;
        GenerationOutputs outputs;
        outputs.fbxGeometry = &geometry;
        outputs.maximumFbxGeometryBytes = options->maximum_geometry_payload_bytes;
        outputs.cancellation = options->cancellation;
        if (bake) {
            bake->validateVisuals(*snapshot);
            uvs = std::make_unique<UvMetadata>(*snapshot, bake->uvBudget());
            outputs.uvs = uvs.get();
            outputs.baked = bake.get();
            outputs.images = &bake->images();
            outputs.textureSize = bake->textureSize();
            outputs.useFbxAtlasUvs = true;
        }
        // Export owns a separate double snapshot; release preview/atlas workspace before encoding.
        (void)generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, outputs);
        uvs.reset();
        checkCancellation(options->cancellation);
        auto document = [&] {
            if (!bake)
                return encodeFbxMeshScene(geometry->object(), limits);
            FbxBakedImageViews images {};
            for (size_t channel = 0; channel < bake->maps.size(); ++channel)
                images[channel] = bake->maps[channel] ? &*bake->maps[channel] : nullptr;
            const auto material = makeFbxBakedMaterial(images, maximumPng);
            checkCancellation(options->cancellation);
            return encodeFbxTexturedMeshScene(geometry->object(), material, limits);
        }();
        checkCancellation(options->cancellation);
        auto owner = std::make_unique<Dust3DFbxExport>(Dust3DFbxExport { std::move(document), std::move(warning), 0 });
        RetainedPayloadBudget retained(maximumOutput);
        retained.tryAdd(sizeof(*owner));
        retained.accountDynamicFields(owner->document.bytes(), owner->warning);
        if (retained.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX retained final output exceeds quota.");
        owner->payloadBytes = retained.usedBytes();
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const TextureBakeError& cause) {
        const auto status = cause.code() == TextureBakeErrorCode::cancelled ? DUST3D_STATUS_CANCELLED
            : cause.code() == TextureBakeErrorCode::limitExceeded           ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == TextureBakeErrorCode::invalidInput            ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                            : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const UvAtlasError& cause) {
        const auto status = cause.code() == UvAtlasErrorCode::cancelled ? DUST3D_STATUS_CANCELLED
            : cause.code() == UvAtlasErrorCode::limitExceeded           ? DUST3D_STATUS_LIMIT_EXCEEDED
            : cause.code() == UvAtlasErrorCode::invalidInput            ? DUST3D_STATUS_INVALID_DOCUMENT
                                                                        : DUST3D_STATUS_GENERATION_FAILED;
        setError(error, status, cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown non-standard static FBX export failure.");
    }
    return nullptr;
}
void dust3d_fbx_export_destroy(Dust3DFbxExport* output) { delete output; }
const uint8_t* dust3d_fbx_export_bytes(const Dust3DFbxExport* output)
{
    return output && !output->document.bytes().empty() ? output->document.bytes().data() : nullptr;
}
size_t dust3d_fbx_export_byte_count(const Dust3DFbxExport* output) { return output ? output->document.bytes().size() : 0; }
size_t dust3d_fbx_export_payload_bytes(const Dust3DFbxExport* output) { return output ? output->payloadBytes : 0; }
const char* dust3d_fbx_export_warning(const Dust3DFbxExport* output) { return output ? output->warning.c_str() : ""; }

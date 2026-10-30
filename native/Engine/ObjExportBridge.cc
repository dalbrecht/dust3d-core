// SPDX-License-Identifier: MIT
#include "Cancellation.h"
#include "GenerationPipeline.h"
#include "ObjDocument.h"
#include <dust3d/base/retained_payload_budget.h>
struct Dust3DObjExport {
    std::unique_ptr<dust3d::native::OwnedObjDocument> document;
    std::string warning;
    size_t payloadBytes = 0;
};
Dust3DObjExport* dust3d_obj_export_load(const uint8_t* bytes, size_t size, const Dust3DObjExportOptions* options, Dust3DError* error)
{
    using namespace dust3d;
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!options || options->struct_size != sizeof(*options))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "OBJ export requires correctly sized options.");
        const size_t maximum = options->maximum_output_payload_bytes ? options->maximum_output_payload_bytes : 64 * 1024 * 1024;
        if (maximum <= sizeof(Dust3DObjExport) + sizeof(OwnedObjDocument) + 1 || maximum > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ final output quota exceeds supported bounds.");
        checkCancellation(options->cancellation);
        std::lock_guard<std::mutex> lock(engineMutex());
        checkCancellation(options->cancellation);
        std::string warning;
        // Imported model assets reject before decoding: no import owner is supplied.
        auto snapshot = readDocument(bytes, size, warning);
        RetainedPayloadBudget overhead(maximum);
        overhead.tryAdd(sizeof(Dust3DObjExport));
        overhead.accountDynamic(warning);
        if (overhead.isExceeded() || overhead.usedBytes() >= maximum)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ owner and warning exceed output quota.");
        std::unique_ptr<OwnedObjDocument> document;
        GenerationOutputs outputs;
        outputs.purpose = GenerationPurpose::originalExport;
        outputs.objDocument = &document;
        outputs.maximumObjPayloadBytes = maximum - overhead.usedBytes();
        outputs.cancellation = options->cancellation;
        (void)generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, outputs);
        checkCancellation(options->cancellation);
        auto owner = std::make_unique<Dust3DObjExport>(Dust3DObjExport { std::move(document), std::move(warning), 0 });
        RetainedPayloadBudget retained(maximum);
        retained.tryAdd(sizeof(*owner));
        retained.tryAdd(sizeof(OwnedObjDocument));
        retained.accountDynamicFields(owner->document->bytes(), owner->warning);
        if (retained.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "OBJ retained final output exceeds quota.");
        owner->payloadBytes = retained.usedBytes();
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown non-standard OBJ export failure.");
    }
    return nullptr;
}
Dust3DObjExport* dust3d_indexed_mesh_export_obj(const Dust3DIndexedMesh* mesh, const Dust3DObjExportOptions* options, Dust3DError* error)
{
    using namespace dust3d;
    using namespace dust3d::native;
    setError(error, DUST3D_STATUS_OK, "");
    try {
        if (!mesh || !options || options->struct_size != sizeof(*options))
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Indexed OBJ requires a source and correctly sized options.");
        const size_t maximum = options->maximum_output_payload_bytes ? options->maximum_output_payload_bytes : 64 * 1024 * 1024;
        if (maximum <= sizeof(Dust3DObjExport) + sizeof(OwnedObjDocument) + 1 || maximum > 256 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ output quota exceeds supported bounds.");
        checkCancellation(options->cancellation);
        RetainedPayloadBudget minimum(maximum);
        minimum.tryAdd(sizeof(Dust3DObjExport));
        minimum.tryAdd(mesh->warning.size());
        minimum.tryAdd(1);
        if (minimum.isExceeded() || minimum.usedBytes() >= maximum)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ owner and warning exceed output quota.");
        std::string warning = mesh->warning;
        RetainedPayloadBudget overhead(maximum);
        overhead.tryAdd(sizeof(Dust3DObjExport));
        overhead.accountDynamic(warning);
        if (overhead.isExceeded() || overhead.usedBytes() >= maximum)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ owner and warning exceed output quota.");
        // This formatter only reads immutable indexed data and uses no inherited engine globals.
        auto document = std::make_unique<OwnedObjDocument>(encodeIndexedObjDocument(*mesh, maximum - overhead.usedBytes(), options->cancellation));
        checkCancellation(options->cancellation);
        auto owner = std::make_unique<Dust3DObjExport>(Dust3DObjExport { std::move(document), std::move(warning), 0 });
        RetainedPayloadBudget retained(maximum);
        retained.tryAdd(sizeof(*owner));
        retained.tryAdd(sizeof(OwnedObjDocument));
        retained.accountDynamicFields(owner->document->bytes(), owner->warning);
        if (retained.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Indexed OBJ retained output exceeds quota.");
        owner->payloadBytes = retained.usedBytes();
        checkCancellation(options->cancellation);
        return owner.release();
    } catch (const DocumentError& cause) {
        setError(error, cause.status(), cause.what());
    } catch (const std::bad_alloc& cause) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, cause.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown indexed OBJ export failure.");
    }
    return nullptr;
}
void dust3d_obj_export_destroy(Dust3DObjExport* output) { delete output; }
const uint8_t* dust3d_obj_export_bytes(const Dust3DObjExport* output) { return output ? output->document->bytes().data() : nullptr; }
size_t dust3d_obj_export_byte_count(const Dust3DObjExport* output) { return output ? output->document->bytes().size() : 0; }
size_t dust3d_obj_export_payload_bytes(const Dust3DObjExport* output) { return output ? output->payloadBytes : 0; }
const char* dust3d_obj_export_warning(const Dust3DObjExport* output) { return output ? output->warning.c_str() : ""; }

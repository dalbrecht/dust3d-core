// SPDX-License-Identifier: MIT
#ifndef DUST3D_WORKER_SERVICE_INTERNAL_H_
#define DUST3D_WORKER_SERVICE_INTERNAL_H_
#include "WorkerAdmission.h"
#include <Dust3DWorkerRuntime.h>
#include <algorithm>
#include <iterator>
namespace dust3d::worker::bridge {
inline RequestIdentity copyRequestIdentity(const Dust3DWorkerRequestIdentity& source)
{
    RequestIdentity result;
    std::copy(std::begin(source.document), std::end(source.document), result.document.begin());
    result.request = source.request;
    result.revision = source.revision;
    return result;
}
inline Dust3DWorkerRequestIdentity copyRequestIdentity(const RequestIdentity& source)
{
    Dust3DWorkerRequestIdentity result {};
    std::copy(source.document.begin(), source.document.end(), result.document);
    result.request = source.request;
    result.revision = source.revision;
    return result;
}
inline ServiceReceipt copyServiceReceipt(const Dust3DWorkerServiceReceipt& source)
{
    if (source.operation != DUST3D_WORKER_OPERATION_MESH && source.operation != DUST3D_WORKER_OPERATION_UV_MESH
        && source.operation != DUST3D_WORKER_OPERATION_SOURCED_MESH
        && source.operation != DUST3D_WORKER_OPERATION_INDEXED_SOURCED_MESH && source.operation != DUST3D_WORKER_OPERATION_INDEXED_MESH && source.operation != DUST3D_WORKER_OPERATION_IMPORTED_RENDER
        && source.operation != DUST3D_WORKER_OPERATION_LEGACY_BAKED
        && source.operation != DUST3D_WORKER_OPERATION_SOURCE_BAKED)
        throw ProtocolError(ProtocolErrorCode::invalidInput, "Service receipt operation is unsupported.");
    ServiceReceipt result;
    result.runtimeStatus = source.runtime_status;
    result.admissionStatus = source.admission_status;
    result.operation = static_cast<Operation>(source.operation);
    result.request = source.request_identity;
    result.process = source.process_identifier;
    result.systemError = source.system_error;
    result.ioStage = source.io_stage;
    result.detailSource = source.detail_source;
    result.detailCode = source.detail_code;
    result.messageTruncated = source.message_truncated;
    result.finalizationError = source.finalization_error;
    return result;
}
struct SemanticPolicies {
    MeshPayloadLimits mesh;
    FailurePayloadLimits failure;
    UvMeshPayloadLimits uv;
    SourcedMeshPayloadLimits sources;
    IndexedReplyLimits indexed;
    ImportedRenderLimits imported;
    LegacyBakedLimits legacy;
    SourceBakedLimits sourceBake;
};
inline SemanticPolicies semanticPolicies(const Limits& wire, uint64_t maximum)
{
    if (!maximum || maximum > 128ULL * 1024 * 1024)
        throw ProtocolError(ProtocolErrorCode::invalidLimits, "Semantic decoded-byte budget is unsupported.");
    SemanticPolicies result;
    result.mesh.maximumWireBytes = wire.maximumOutputBytes;
    result.mesh.maximumDecodedBytes = maximum;
    result.failure.maximumWireBytes = std::min(result.failure.maximumWireBytes, wire.maximumFailureBytes);
    result.failure.maximumCauseBytes = std::min({ result.failure.maximumCauseBytes, maximum, result.failure.maximumWireBytes });
    result.uv.maximumWireBytes = result.mesh.maximumWireBytes;
    result.uv.maximumDecodedBytes = maximum;
    result.uv.geometry = result.mesh;
    result.uv.uv.maximumWireBytes = result.uv.maximumWireBytes;
    result.uv.uv.maximumDecodedBytes = maximum;
    result.sources.maximumWireBytes = wire.maximumOutputBytes;
    result.sources.maximumDecodedBytes = maximum;
    result.sources.geometry = result.mesh;
    result.sources.sources.maximumWireBytes = wire.maximumOutputBytes;
    result.sources.sources.maximumDecodedBytes = std::min(result.sources.sources.maximumDecodedBytes, maximum);
    result.indexed.mesh.maximumWireBytes = wire.maximumOutputBytes;
    result.indexed.mesh.maximumDecodedBytes = maximum;
    result.indexed.sourcedMesh.maximumWireBytes = wire.maximumOutputBytes;
    result.indexed.sourcedMesh.maximumDecodedBytes = maximum;
    result.indexed.sourcedMesh.geometry = result.indexed.mesh;
    result.indexed.sourcedMesh.sources.maximumWireBytes = wire.maximumOutputBytes;
    result.indexed.sourcedMesh.sources.maximumDecodedBytes = std::min(result.indexed.sourcedMesh.sources.maximumDecodedBytes, maximum);
    result.imported.maximumWireBytes = wire.maximumOutputBytes;
    result.imported.maximumDecodedBytes = maximum;
    result.imported.maximumDecodedPixelBytes = std::min(result.imported.maximumDecodedPixelBytes, maximum);
    result.imported.surface.maximumWireBytes = wire.maximumOutputBytes;
    result.imported.surface.maximumDecodedBytes = maximum;
    result.imported.surface.geometry = result.mesh;
    result.imported.surface.surface.maximumWireBytes = wire.maximumOutputBytes;
    result.imported.surface.surface.maximumDecodedBytes = maximum;
    result.imported.materials.maximumWireBytes = wire.maximumOutputBytes;
    result.imported.materials.maximumDecodedBytes = std::min(result.imported.materials.maximumDecodedBytes, maximum);
    result.imported.materials.maximumEncodedBytes = std::min(result.imported.materials.maximumEncodedBytes, maximum);
    result.imported.materials.maximumImageBytes = std::min(result.imported.materials.maximumImageBytes, maximum);
    result.legacy.maximumWireBytes = wire.maximumOutputBytes;
    result.legacy.maximumDecodedBytes = maximum;
    result.legacy.geometry = result.mesh;
    result.legacy.uv = result.uv.uv;
    result.legacy.textures.maximumWireBytes = wire.maximumOutputBytes;
    result.legacy.textures.maximumDecodedBytes = std::min(result.legacy.textures.maximumDecodedBytes, maximum);
    result.legacy.textures.maximumPixelBytes = std::min(result.legacy.textures.maximumPixelBytes, maximum);
    result.sourceBake.maximumWireBytes = wire.maximumOutputBytes;
    result.sourceBake.maximumDecodedBytes = maximum;
    result.sourceBake.geometry = result.mesh;
    result.sourceBake.surface.maximumWireBytes = wire.maximumOutputBytes;
    result.sourceBake.surface.maximumDecodedBytes = std::min(result.sourceBake.surface.maximumDecodedBytes, maximum);
    result.sourceBake.textures = result.legacy.textures;
    return result;
}
}
#endif

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
    if (source.operation != DUST3D_WORKER_OPERATION_MESH && source.operation != DUST3D_WORKER_OPERATION_UV_MESH)
        throw ProtocolError(ProtocolErrorCode::invalidInput, "Service receipt operation is unsupported.");
    ServiceReceipt result;
    result.runtimeStatus = source.runtime_status;
    result.admissionStatus = source.admission_status;
    result.operation = source.operation == DUST3D_WORKER_OPERATION_MESH ? Operation::generateMesh : Operation::generateUvMesh;
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
    return result;
}
}
#endif

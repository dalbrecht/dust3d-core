// SPDX-License-Identifier: MIT
#include "WorkerAdmission.h"

namespace dust3d {
namespace worker {
    namespace {
        void validateRequest(const Header& request, const Limits& requestLimits)
        {
            encodeHeader(request, requestLimits);
            if (request.kind != FrameKind::request)
                throw AdmissionError(AdmissionErrorCode::invalidRequest, "Pending work requires a request header");
        }
        AdmittedReply decodeReply(const Header& request, const uint8_t* bytes, size_t size,
            const Limits& wireLimits, const MeshPayloadLimits& meshLimits,
            const FailurePayloadLimits& failureLimits, const UvMeshPayloadLimits& uvLimits)
        {
            const auto frame = decodeFrame(bytes, size, wireLimits);
            validateResponseIdentity(frame.header, request);
            if (frame.header.kind == FrameKind::response) {
                if (request.operation == Operation::generateUvMesh)
                    return decodeUvMeshPayload(frame.payload.data(), frame.payload.size(), uvLimits);
                return decodeMeshPayload(frame.payload.data(), frame.payload.size(), meshLimits);
            }
            return decodeFailurePayload(frame.payload.data(), frame.payload.size(), failureLimits);
        }
    }
    AdmissionError::AdmissionError(AdmissionErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    PendingReply::PendingReply(const Header& request, const Limits& requestLimits)
        : m_request(request)
    {
        validateRequest(request, requestLimits);
    }
    AdmittedReply PendingReply::complete(const RequestIdentity& active, ProcessOutcome outcome,
        const uint8_t* bytes, size_t size, const Limits& wireLimits,
        const MeshPayloadLimits& meshLimits, const FailurePayloadLimits& failureLimits,
        const UvMeshPayloadLimits& uvLimits)
    {
        if (m_isRetired)
            throw AdmissionError(AdmissionErrorCode::retired, "Worker request is already retired");
        // Retire before parsing/allocation so a failed completion cannot publish on replay.
        retire();
        if (!outcome.exitedNormally || outcome.exitCode != 0)
            throw AdmissionError(AdmissionErrorCode::unsuccessfulExit,
                "Worker completion rejected: normal=" + std::to_string(outcome.exitedNormally) + " code=" + std::to_string(outcome.exitCode));
        if (!(active == m_request.identity))
            throw AdmissionError(AdmissionErrorCode::staleRequest, "Worker request is no longer active");
        return decodeReply(m_request, bytes, size, wireLimits, meshLimits, failureLimits, uvLimits);
    }
    PendingServiceReply::PendingServiceReply(const Header& request, uint64_t connectionGeneration, const Limits& requestLimits)
        : m_request(request)
        , m_connectionGeneration(connectionGeneration)
    {
        validateRequest(request, requestLimits);
        if (!connectionGeneration)
            throw AdmissionError(AdmissionErrorCode::invalidServiceSession, "Service connection generation must be nonzero");
    }
    AdmittedReply PendingServiceReply::complete(const RequestIdentity& active, const ServiceObservation& observed,
        const ServiceReceipt& receipt, const uint8_t* bytes, size_t size, const Limits& wireLimits,
        const MeshPayloadLimits& meshLimits, const FailurePayloadLimits& failureLimits, const UvMeshPayloadLimits& uvLimits)
    {
        if (m_isRetired)
            throw AdmissionError(AdmissionErrorCode::retired, "Service request is already retired");
        retire();
        if (!(active == m_request.identity))
            throw AdmissionError(AdmissionErrorCode::staleRequest, "Service request is no longer active");
        if (observed.connectionGeneration != m_connectionGeneration || observed.process <= 0 || receipt.process != observed.process)
            throw AdmissionError(AdmissionErrorCode::unexpectedServicePeer, "Service receipt does not match the observed connection peer");
        if (!observed.reachedEndOfStream)
            throw AdmissionError(AdmissionErrorCode::incompleteStream, "Service response has not reached observed EOF");
        if (receipt.operation != m_request.operation || receipt.request != m_request.identity.request)
            throw AdmissionError(AdmissionErrorCode::mismatchedServiceReceipt, "Service receipt operation/request does not match pending work");
        if (receipt.runtimeStatus || receipt.admissionStatus || receipt.systemError || receipt.ioStage || receipt.detailSource || receipt.detailCode || receipt.messageTruncated || receipt.finalizationError)
            throw AdmissionError(AdmissionErrorCode::unsuccessfulService,
                "Service receipt rejected: runtime=" + std::to_string(receipt.runtimeStatus)
                    + " admission=" + std::to_string(receipt.admissionStatus) + " errno=" + std::to_string(receipt.systemError)
                    + " stage=" + std::to_string(receipt.ioStage) + " source=" + std::to_string(receipt.detailSource)
                    + " code=" + std::to_string(receipt.detailCode) + " truncated=" + std::to_string(receipt.messageTruncated)
                    + " finalization=" + std::to_string(receipt.finalizationError));
        return decodeReply(m_request, bytes, size, wireLimits, meshLimits, failureLimits, uvLimits);
    }

}
}

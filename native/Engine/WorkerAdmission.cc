// SPDX-License-Identifier: MIT
#include "WorkerAdmission.h"

namespace dust3d {
namespace worker {
    AdmissionError::AdmissionError(AdmissionErrorCode code, const std::string& message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    PendingReply::PendingReply(const Header& request, const Limits& requestLimits)
        : m_request(request)
    {
        encodeHeader(request, requestLimits);
        if (request.kind != FrameKind::request)
            throw AdmissionError(AdmissionErrorCode::invalidRequest, "Pending work requires a request header");
    }
    AdmittedReply PendingReply::complete(const RequestIdentity& active, ProcessOutcome outcome,
        const uint8_t* bytes, size_t size, const Limits& wireLimits,
        const MeshPayloadLimits& meshLimits, const FailurePayloadLimits& failureLimits)
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
        const auto frame = decodeFrame(bytes, size, wireLimits);
        validateResponseIdentity(frame.header, m_request);
        if (frame.header.kind == FrameKind::response)
            return decodeMeshPayload(frame.payload.data(), frame.payload.size(), meshLimits);
        return decodeFailurePayload(frame.payload.data(), frame.payload.size(), failureLimits);
    }
}
}

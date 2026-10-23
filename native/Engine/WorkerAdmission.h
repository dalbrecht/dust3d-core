// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_ADMISSION_H_
#define DUST3D_NATIVE_WORKER_ADMISSION_H_
#include "WorkerFailure.h"
#include "WorkerMesh.h"
#include <string>
#include <variant>

namespace dust3d {
namespace worker {
    /// Transport-observed terminal process state, never inferred from reply bytes.
    struct ProcessOutcome {
        bool exitedNormally = false;
        int exitCode = 0;
    };
    enum class AdmissionErrorCode { invalidRequest,
        retired,
        unsuccessfulExit,
        staleRequest };
    class AdmissionError : public std::runtime_error {
    public:
        AdmissionError(AdmissionErrorCode code, const std::string& message);
        AdmissionErrorCode code() const noexcept { return m_code; }

    private:
        AdmissionErrorCode m_code;
    };
    using AdmittedReply = std::variant<MeshPayload, FailurePayload>;

    /// Single-owner completion gate. Caller serializes access and supplies authoritative active identity.
    /// Does not spawn, collect, cancel, retry, log, sandbox or enforce transport resource limits.
    class PendingReply {
    public:
        /// Validate the original request header; throws ProtocolError or AdmissionError.
        explicit PendingReply(const Header& request);
        PendingReply(const PendingReply&) = delete;
        PendingReply& operator=(const PendingReply&) = delete;
        PendingReply(PendingReply&&) = delete;
        PendingReply& operator=(PendingReply&&) = delete;
        /// Idempotently retire cancelled/replaced/closed work before handling future completion.
        void retire() noexcept { m_isRetired = true; }
        bool isRetired() const noexcept { return m_isRetired; }
        /// Retire first, then require terminal exit zero, current identity, exact frame and payload semantics.
        /// Returns independent owned data only once. Every rejected completion remains retired.
        /// Throws AdmissionError, codec errors or allocation errors; original typed causes propagate.
        AdmittedReply complete(const RequestIdentity& active, ProcessOutcome outcome,
            const uint8_t* bytes, size_t size, const Limits& wireLimits = {},
            const MeshPayloadLimits& meshLimits = {}, const FailurePayloadLimits& failureLimits = {});

    private:
        Header m_request;
        bool m_isRetired = false;
    };
}
}
#endif

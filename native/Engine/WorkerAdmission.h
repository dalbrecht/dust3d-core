// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_ADMISSION_H_
#define DUST3D_NATIVE_WORKER_ADMISSION_H_
#include "WorkerFailure.h"
#include "WorkerImportedRender.h"
#include "WorkerIndexedSources.h"
#include "WorkerLegacyBaked.h"
#include "WorkerSourceBaked.h"
#include "WorkerSourcedMesh.h"
#include "WorkerUvMesh.h"
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
        staleRequest,
        invalidServiceSession,
        unexpectedServicePeer,
        incompleteStream,
        unsuccessfulService,
        mismatchedServiceReceipt };
    class AdmissionError : public std::runtime_error {
    public:
        AdmissionError(AdmissionErrorCode code, const std::string& message);
        AdmissionErrorCode code() const noexcept { return m_code; }

    private:
        AdmissionErrorCode m_code;
    };
    /// Successful imported absence is distinct from both geometry and failure.
    struct ImportedRenderReply {
        std::optional<ImportedRenderPayload> value;
    };
    using AdmittedReply = std::variant<MeshPayload, FailurePayload, UvMeshPayload, SourcedMeshPayload, IndexedMeshPayload, IndexedSourcesPayload, ImportedRenderReply, LegacyBakedPayload, SourceBakedPayload>;
    /// Independent ordinary-indexed and atomic indexed/source output policies.
    struct IndexedReplyLimits {
        IndexedMeshPayloadLimits mesh;
        IndexedSourcesPayloadLimits sourcedMesh;
    };

    /// Trusted caller observations, never reconstructed from service-supplied bytes.
    /// Generation identifies the authenticated connection lifetime; process is its OS-observed PID.
    struct ServiceObservation {
        uint64_t connectionGeneration = 0;
        int64_t process = 0;
        bool reachedEndOfStream = false;
    };
    /// Scalar service receipt. Zero runtime/admission statuses mean frame-written/accepted.
    /// A frame-written failure payload is a completed transaction, not successful geometry.
    struct ServiceReceipt {
        uint32_t runtimeStatus = 1;
        uint32_t admissionStatus = 1;
        Operation operation = Operation::generateMesh;
        uint64_t request = 0;
        int64_t process = 0;
        int32_t systemError = 0;
        uint32_t ioStage = 0;
        uint32_t detailSource = 0;
        int32_t detailCode = 0;
        uint8_t messageTruncated = 0;
        int32_t finalizationError = 0;
    };

    /// Single-use semantic gate for a persistent service; no invented ProcessOutcome.
    /// Caller serializes access, authenticates the connection and supplies real OS/EOF/current-source observations.
    /// Does not authenticate, collect, stop, reap, sandbox or publish application state itself.
    class PendingServiceReply {
    public:
        /// @param request Original request header, validated against requestLimits.
        /// @param connectionGeneration Nonzero trusted connection lifetime selected before submission.
        /// @param requestLimits Configured input/header limits.
        /// @throws ProtocolError or AdmissionError for invalid request/session values.
        PendingServiceReply(const Header& request, uint64_t connectionGeneration, const Limits& requestLimits = {});
        PendingServiceReply(const PendingServiceReply&) = delete;
        PendingServiceReply& operator=(const PendingServiceReply&) = delete;
        PendingServiceReply(PendingServiceReply&&) = delete;
        PendingServiceReply& operator=(PendingServiceReply&&) = delete;
        /// Retire cancelled/replaced/closed work before any future completion.
        void retire() noexcept { m_isRetired = true; }
        bool isRetired() const noexcept { return m_isRetired; }
        /// Retire first, correlate current identity/connection/receipt/actual EOF, then decode owned semantics.
        /// @param active Authoritative active document/request/revision, not copied from the reply.
        /// @param observed Real observations from the caller's authenticated transport and collector.
        /// @param receipt Scalar callback from that connection; never trusted as peer observation.
        /// @param bytes Complete bounded frame; borrowed only for this call.
        /// @param size Actual collected frame byte count.
        /// @param wireLimits Exact frame byte limits.
        /// @param meshLimits Base mesh semantic/output limits.
        /// @param failureLimits Original failure payload/text limits.
        /// @param uvLimits Atomic paired geometry/UV semantic/output limits.
        /// @param sourceLimits Atomic geometry/source semantic/output limits.
        /// @param indexedLimits Independent ordinary/indexed-source semantic policies.
        /// @return Independent expanded/indexed/UV/source values or original typed failure, once only.
        /// @throws AdmissionError, codec or allocation errors; rejection leaves this gate retired.
        /// Example: pending.complete(active, observed, receipt, collected.data(), collected.size()).
        AdmittedReply complete(const RequestIdentity& active, const ServiceObservation& observed,
            const ServiceReceipt& receipt, const uint8_t* bytes, size_t size,
            const Limits& wireLimits = {}, const MeshPayloadLimits& meshLimits = {},
            const FailurePayloadLimits& failureLimits = {}, const UvMeshPayloadLimits& uvLimits = {}, const SourcedMeshPayloadLimits& sourceLimits = {}, const IndexedReplyLimits& indexedLimits = {}, const ImportedRenderLimits& importedLimits = {}, const LegacyBakedLimits& legacyLimits = {}, const SourceBakedLimits& sourceBakeLimits = {});

    private:
        Header m_request;
        uint64_t m_connectionGeneration;
        bool m_isRetired = false;
    };

    /// Single-owner completion gate. Caller serializes access and supplies authoritative active identity.
    /// Does not spawn, collect, cancel, retry, log, sandbox or enforce transport resource limits.
    class PendingReply {
    public:
        /// Validate the original request header with its configured input limits, without reading its payload.
        /// Throws ProtocolError or AdmissionError. Defaults preserve the standard wire quotas.
        explicit PendingReply(const Header& request, const Limits& requestLimits = {});
        PendingReply(const PendingReply&) = delete;
        PendingReply& operator=(const PendingReply&) = delete;
        PendingReply(PendingReply&&) = delete;
        PendingReply& operator=(PendingReply&&) = delete;
        /// Idempotently retire cancelled/replaced/closed work before handling future completion.
        void retire() noexcept { m_isRetired = true; }
        bool isRetired() const noexcept { return m_isRetired; }
        /// Retire first, then require terminal exit zero, current identity, exact frame and payload semantics.
        /// UV/source/indexed responses use their matching independent policies; base mesh/failure limits remain separate.
        /// Returns independent owned data only once. Every rejected completion remains retired.
        /// Throws AdmissionError, codec errors or allocation errors; original typed causes propagate.
        AdmittedReply complete(const RequestIdentity& active, ProcessOutcome outcome,
            const uint8_t* bytes, size_t size, const Limits& wireLimits = {},
            const MeshPayloadLimits& meshLimits = {}, const FailurePayloadLimits& failureLimits = {},
            const UvMeshPayloadLimits& uvLimits = {}, const SourcedMeshPayloadLimits& sourceLimits = {}, const IndexedReplyLimits& indexedLimits = {}, const ImportedRenderLimits& importedLimits = {}, const LegacyBakedLimits& legacyLimits = {}, const SourceBakedLimits& sourceBakeLimits = {});

    private:
        Header m_request;
        bool m_isRetired = false;
    };
}
}
#endif

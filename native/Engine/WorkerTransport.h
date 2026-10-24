// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_TRANSPORT_H_
#define DUST3D_NATIVE_WORKER_TRANSPORT_H_
#include "WorkerAdmission.h"

namespace dust3d {
namespace worker {
    struct TransportLimits {
        Limits wire;
        size_t maximumIoChunkBytes = 65536;
        size_t maximumCapturedDiagnosticBytes = 4096;
    };
    struct CollectedJob {
        ProcessOutcome outcome;
        std::vector<uint8_t> reply;
        std::vector<uint8_t> diagnostic;
        bool diagnosticTruncated = false;
    };
    enum class TransportErrorCode { invalidConfiguration,
        setup,
        spawn,
        input,
        output,
        diagnostic,
        wait };
    /// OS errors retain the original errno/posix_spawn code; no document/child diagnostic content is logged.
    class TransportError : public std::runtime_error {
    public:
        TransportError(TransportErrorCode code, int operatingSystemCode, const char* operation);
        TransportErrorCode code() const noexcept { return m_code; }
        int operatingSystemCode() const noexcept { return m_operatingSystemCode; }

    private:
        TransportErrorCode m_code;
        int m_operatingSystemCode;
    };
    /// Development-only synchronous Unix transport. Pass a trusted absolute helper executable path.
    /// Validate/encode request, spawn directly without shell, concurrently send/drain three channels,
    /// collect bounded reply/diagnostic bytes and observe termination. No data is admitted/published here.
    /// Feed the result to PendingReply with authoritative current identity after this returns.
    /// Caller must run off the UI thread and serialize application state/admission separately.
    /// No deadline, hard cancellation, RSS/filesystem sandbox or parent-death policy is provided.
    /// Cleanup closes channels and reaps the child, and can block on uncooperative generation.
    /// Throws typed transport/codec errors or allocation errors; never automatically retries.
    CollectedJob runWorkerJob(const std::string& executable, const Frame& request, const TransportLimits& limits = {});
}
}
#endif

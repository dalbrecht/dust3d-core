// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_JOB_IO_H_
#define DUST3D_NATIVE_WORKER_JOB_IO_H_
#include "WorkerImportedProducerNative.h"
#include "WorkerLegacyProducerNative.h"
#include "WorkerProtocol.h"
#include "WorkerSessionOperation.h"
#include "WorkerSourceProducerNative.h"
#include <optional>
namespace dust3d {
namespace worker {
    struct WorkerIoResult {
        bool complete;
        int error;
    };
    enum class WorkerJobStage { input,
        output };
    /// Original syscall cause stored without dynamic members. No document content is logged.
    class WorkerJobIOError : public std::exception {
    public:
        WorkerJobIOError(WorkerJobStage stage, int systemError) noexcept;
        WorkerJobStage stage() const noexcept { return m_stage; }
        int systemError() const noexcept { return m_systemError; }
        const char* what() const noexcept override;

    private:
        WorkerJobStage m_stage;
        int m_systemError;
    };
    struct WorkerJobContext {
        uint64_t requestIdentity = 0;
        std::optional<Operation> operation;
    };
    /// Host-side admission declaration, never inferred from untrusted wire bytes.
    struct WorkerJobExpectation {
        Operation operation;
        RequestIdentity identity;
    };
    /// Content-independent diagnostic label; absent/unsupported observations remain unknown.
    /// @param operation Optional observed operation, never inferred before header validation.
    /// @return Static literal valid for process lifetime; no document text is included.
    /// Example: workerOperationName(context.operation);
    const char* workerOperationName(std::optional<Operation> operation) noexcept;
    /// Write a stable borrowed span using bounded syscall chunks; never close the descriptor.
    /// @param descriptor Borrowed writable channel. Host must establish broken-pipe signal policy.
    /// @param bytes Borrowed complete storage; NULL allowed only for zero size.
    /// @param size Exact span, valid until return. Empty writes succeed without I/O.
    /// @return Complete or original errno/EIO; interrupted calls retry, no deadline is imposed.
    /// Example: auto outcome = writeWorkerBytes(fd, bytes.data(), bytes.size());
    WorkerIoResult writeWorkerBytes(int descriptor, const uint8_t* bytes, size_t size) noexcept;
    /// Read one exact request plus EOF on separate borrowed job channels.
    /// Resets context before any I/O; validates bounded framing before allocation.
    /// Throws original protocol/I/O/allocation causes. No native work/descriptor close.
    /// Optional host expectation is validated and compared before body retention.
    /// It is not authentication. Example: auto frame = readWorkerRequest(input, output, context, &expected);
    Frame readWorkerRequest(int input, int output, WorkerJobContext& context, const WorkerJobExpectation* expected = nullptr);
    /// Encode/write an owned reply, preserving original output I/O errors.
    /// Borrowed descriptor; host establishes signal/resource policy. Never closes it.
    /// Example: writeWorkerReply(output, response);
    void writeWorkerReply(int output, const Frame& response);
    /// Serve one operation9 request through a retained host-owned session.
    /// Same channel, isolation, lifetime and exception obligations as serveWorkerJob.
    /// Trusted generation is supplied by host, never inferred from wire bytes.
    /// Caller serializes jobs/lifecycle and keeps session alive; no native interruption.
    /// Example: serveWorkerSessionJob(input, output, context, session, generation, {});
    void serveWorkerSessionJob(int input, int output, WorkerJobContext& context, ConnectionGenerationSession& session, uint64_t generation, const SessionOutputLimits& limits, const WorkerJobExpectation* expected = nullptr);
    /// Serve one bounded frame followed by EOF on borrowed channels, through serialized native C ABI.
    /// @param input Borrowed readable request channel, exclusively owned by the host for this job.
    /// @param output Separate host-owned writable reply channel; no path is opened or closed.
    /// @param context Reset before input, then records only the validated request identity/operation.
    /// @throws ProtocolError, WorkerJobIOError, codec or allocation errors; no automatic retry/fallback.
    /// Uses existing default wire/output policies. Host establishes resource, SIGPIPE and stdout/stderr
    /// isolation before calling. Blocking I/O requires a non-UI worker; this is not containment or
    /// cancellation. Host closes job channels after return/throw and preserves original failures.
    /// Example: WorkerJobContext context; serveWorkerJob(inputFD, outputFD, context);
    void serveWorkerJob(int input, int output, WorkerJobContext& context, ImportedRenderImageDecoder* decoder = nullptr, const NativeImportedProducerLimits& importedLimits = {},
        ProceduralImageDecoder* proceduralDecoder = nullptr, const LegacyProducerOptions& legacyLimits = {}, const NativeSourceProducerOptions& sourceBakeLimits = {});
}
}
#endif

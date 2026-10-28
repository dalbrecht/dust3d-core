// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_ARCHIVE_IO_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_ARCHIVE_IO_H_
#include "WorkerPreviewArchive.h"
namespace dust3d {
namespace worker {
    /// Write one captured archive as bounded D3PC chunks on a borrowed descriptor.
    /// @param output Host-owned writable channel; never opened, closed or shut down.
    /// Host must establish broken-pipe signal/resource policy before this call.
    /// @param identity Independently captured correlation, archive direction only.
    /// @param context Frozen native archive/indexed/source settings. The existing
    /// prefix carries archive/source only; indexed policy stays independent host metadata.
    /// @param archive Stable readable storage until return; never retained.
    /// @param size Must match captured archive count before the first write.
    /// @param policy Frozen whole-transfer/chunk bounds; no global caps are changed.
    /// @throws Original PreviewChunkError, SourceRequestError, WorkerJobIOError or
    /// allocation cause. Partial writes are never publishable; host discards output.
    /// Blocking worker-thread operation, no deadline/cancellation or peer admission.
    /// Return does not imply EOF: host must finalize its own channel after return.
    /// Example: writePreviewArchive(outputFD, identity, captured, bytes, size, policy);
    void writePreviewArchive(int output, PreviewChunkIdentity identity, PreviewArchiveContext context, const uint8_t* archive, size_t size, PreviewChunkPolicy policy);
    /// Read until actual descriptor EOF, then return one complete immutable owner.
    /// @param input Exclusive borrowed job channel; never opened or closed here.
    /// @param identity Independent host identity; not inferred from incoming bytes.
    /// @param context Frozen independently admitted request controls.
    /// @param policy Frozen transfer/chunk bounds, validated before reading bytes.
    /// @return Complete archive owner; no native parsing/dispatch/authentication.
    /// @throws Original framing/prefix/I/O/allocation error; partial owner is destroyed.
    /// Retries EINTR only; no deadline, cancellation, peer or current-source admission.
    /// Caller supplies isolated non-UI execution and bounded host supervision.
    /// Example: auto owner = readPreviewArchive(inputFD, identity, captured, policy);
    std::unique_ptr<PreviewArchiveCollector> readPreviewArchive(int input, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy);
}
}
#endif

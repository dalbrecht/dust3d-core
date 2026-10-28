// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_SESSION_IO_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_SESSION_IO_H_
#include "WorkerConnectionSession.h"
#include "WorkerPreviewSession.h"
namespace dust3d {
namespace worker {
    /// Preflight and write complete session values through bounded D3PC chunks.
    /// @param output Borrowed writable descriptor, never opened/closed/shut down.
    /// Host establishes broken-pipe signal/resource policy before calling.
    /// @param identity Frozen session-direction correlation, not peer authentication.
    /// @param context Independently captured archive/native controls.
    /// @param value Stable owned values borrowed until return; no wire-body copy.
    /// @param policy Explicit frozen whole-transfer/chunk bounds.
    /// @throws Original semantic/range/chunk/I/O/allocation cause. Partial output
    /// is never publishable; host finalizes EOF and discards output after failure.
    /// Blocking isolated-worker operation, no deadline/cancellation/admission.
    /// Example: writePreviewSessionChannel(fd, identity, controls, value, policy);
    void writePreviewSessionChannel(int output, PreviewChunkIdentity identity, PreviewArchiveContext context, const SessionOutput& value, PreviewChunkPolicy policy);
    /// Receive full semantic/framing state and actual EOF before moving one value.
    /// @param input Exclusive borrowed descriptor; never opened or closed here.
    /// @param identity Independently captured session identity, not stream authority.
    /// @param context Original admitted request controls, frozen before reading.
    /// @param policy Explicit whole-transfer/chunk bounds checked before reading.
    /// @return Independent validated value; no pointer into the collector escapes.
    /// @throws Original semantic/range/chunk/I/O/allocation error; partial values die.
    /// Uses one fixed64KiB read buffer in addition to bounded collector workspace.
    /// Retries EINTR only. Caller provides non-UI supervision, peer/current-source
    /// admission and descriptor lifetime. No deadline, cancellation or native calls.
    /// Example: auto value = readPreviewSessionChannel(fd, identity, controls, policy);
    SessionOutput readPreviewSessionChannel(int input, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy);
    /// Serve one captured archive through EOF, same-session native work and output.
    /// @param input Exclusive borrowed archive channel, separate from output.
    /// @param output Borrowed reply channel; host finalizes EOF after return.
    /// @param session Live worker-local owner; caller serializes jobs/lifetime.
    /// @param identity Independent archive-direction request/lifetime correlation.
    /// @param context Frozen original archive/native controls, not stream authority.
    /// @param inputPolicy Explicit captured request transfer/chunk bounds.
    /// @param outputPolicy Explicit reply bounds, sufficient for mandatory prefixes.
    /// @throws Original argument/framing/source/scope/native-copy/I/O/allocation cause.
    /// Complete correlated input precedes connection admission, as in legacy serving.
    /// After admission, IDs are consumed even on native/copy/write failure. Native
    /// failures are owned outcomes; successful return means written values, not mesh
    /// success, actual reply EOF, healthy process exit or host publication admission.
    /// Blocking non-UI operation; host establishes signal policy and supervision.
    /// Example: servePreviewSessionJob(inFD, outFD, session, id, controls, inPolicy, outPolicy);
    void servePreviewSessionJob(int input, int output, ConnectionGenerationSession& session, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy inputPolicy, PreviewChunkPolicy outputPolicy);
}
}
#endif

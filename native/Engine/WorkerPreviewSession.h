// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PREVIEW_SESSION_H_
#define DUST3D_NATIVE_WORKER_PREVIEW_SESSION_H_
#include "WorkerPreviewArchive.h"
#include "WorkerPreviewRange.h"
#include "WorkerSessionOutput.h"
namespace dust3d {
namespace worker {
    /// Preflight complete owned values against independent native budgets and shared
    /// semantic validation, then stream existing canonical session bytes in D3PC chunks.
    /// @param identity Frozen session-direction correlation; not peer authentication.
    /// @param context Independently captured archive count and original native policies.
    /// @param value Stable owned values borrowed through return, never normalized.
    /// @param policy Explicit transfer/chunk bound; no legacy default/cap is changed.
    /// @param sink Complete-span-or-throw sink; borrowed bytes expire on return.
    /// @throws Original typed semantic/range/chunk/callback/allocation cause. No native
    /// calls or partial publication. Host finalizes actual EOF and admits peer/current source.
    /// Example: writePreviewSession(identity, captured, value, policy, sink);
    void writePreviewSession(PreviewChunkIdentity identity, PreviewArchiveContext context, const SessionOutput& value, PreviewChunkPolicy policy, PreviewChunkWriteSink sink);
    /// Incremental typed-value owner under native record budgets, without retaining a
    /// duplicate whole wire body or allocating arrays from declared counts. Actual record
    /// bytes are validated into private storage; no partial result may be published.
    class PreviewSessionCollector {
    public:
        /// Freeze session-direction identity, original native controls and explicit
        /// transfer/chunk bounds before retaining bytes. Throws PreviewChunkError
        /// for invalid captured scope/policy; no native calls or declared-size arrays.
        /// Example: PreviewSessionCollector owner(identity, captured, frozenPolicy);
        PreviewSessionCollector(PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy);
        ~PreviewSessionCollector();
        PreviewSessionCollector(const PreviewSessionCollector&) = delete;
        PreviewSessionCollector& operator=(const PreviewSessionCollector&) = delete;
        /// Append borrowed actual bytes, arbitrarily split. First mutation failure seals
        /// this owner and is rethrown by all subsequent operations. No native execution.
        void append(const uint8_t* bytes, size_t size);
        /// Call only at actual descriptor EOF; requires complete semantic/framing state.
        void finishAtEOF();
        /// Move one complete independent value after EOF, once. An incomplete ownership
        /// query throws without mutating collection. No pointer into the collector escapes.
        /// Example: auto value = owner.take(); // after actual EOF and complete validation
        SessionOutput take();
        /// Complete means validated actual EOF and an unused independently owned value.
        bool isComplete() const noexcept;
        /// First mutation failure is retained permanently; query without throwing.
        bool isFailed() const noexcept;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_implementation;
    };
}
}
#endif

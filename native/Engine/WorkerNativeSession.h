// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_NATIVE_SESSION_H_
#define DUST3D_NATIVE_WORKER_NATIVE_SESSION_H_
#include "WorkerGenerationStatisticsCopy.h"
#include "WorkerIndexedSourceOperation.h"
#include "WorkerSessionResult.h"
#include <memory>
#include <mutex>
namespace dust3d {
namespace worker {
    struct NativeSessionOptions {
        IndexedSourcesPayloadLimits output;
        IndexedMeshPayloadLimits ordinary;
        GenerationStatisticsLimits statistics;
    };
    class NativeSessionError : public std::runtime_error {
    public:
        explicit NativeSessionError(FailurePayload failure);
        const FailurePayload& failure() const noexcept { return m_failure; }

    private:
        FailurePayload m_failure;
    };
    /// Exclusive worker-owned cache scoped to one host-selected document. Methods
    /// serialize generation/copy/statistics under one mutex; native calls also use
    /// the engine lock. Caller guarantees lifetime through calls; no concurrent destroy.
    class NativeGenerationSession {
    public:
        /// Create bounded native retention; zero disables cache, maximum256MiB.
        /// Reject invalid document/budget before native work. Throws ProtocolError,
        /// NativeSessionError preserving native cause, or allocation errors.
        NativeGenerationSession(const std::array<uint8_t, 16>& document, uint64_t maximumCachePayloadBytes);
        NativeGenerationSession(const NativeGenerationSession&) = delete;
        NativeGenerationSession& operator=(const NativeGenerationSession&) = delete;
        /// Generate matching indexed/source copies and actual same-call cache stats.
        /// Source status3/4 triggers ordinary indexed regeneration inside this same
        /// worker session, retaining the source cause and any fallback failure.
        /// Status5 is successful emptiness. Wrong document/request/policy rejects
        /// before native work. Statistics-only failures remain separate from geometry.
        /// No parent fallback, transport authentication, cancellation or RSS guarantee.
        /// Example: auto result = session.generateSourcedIndexed(archive, identity, {});
        NativeSessionGeneration generateSourcedIndexed(const std::vector<uint8_t>& archive, const RequestIdentity& identity, const NativeSessionOptions& options);
        /// Generate/copy an opt-in native-budget preview from a stable borrowed archive.
        /// Validate host identity, non-null span and original native policies before
        /// work; archive1..256MiB has no legacy envelope/input quota or duplicate copy.
        /// Source status3/4 uses same-session ordinary fallback, preserving typed causes;
        /// statistics are actual optional observations, independently error-isolated.
        /// Caller retains archive through return. No authentication, transport activation,
        /// cancellation or RSS guarantee. Throws original argument/receipt-copy errors.
        /// Example: auto value = session.generatePreview(archiveOwner.view(), identity);
        NativeSessionGeneration generatePreview(PreviewArchiveView archive, const RequestIdentity& identity, const GenerationStatisticsLimits& statisticsLimits = {});
        /// Copy actual current native statistics; throws original typed/query errors.
        /// Example: auto value = session.statistics();
        GenerationStatistics statistics();
        /// Reset retained native state, preserving document and configured budget.
        /// Throws NativeSessionError; previously returned owned values stay valid.
        /// Example: session.reset();
        void reset();

    private:
        GenerationStatistics statisticsUnlocked(const GenerationStatisticsLimits& limits);
        std::array<uint8_t, 16> m_document;
        std::mutex m_mutex;
        std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> m_owner;
    };
}
}
#endif

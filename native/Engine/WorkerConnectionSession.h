// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_CONNECTION_SESSION_H_
#define DUST3D_NATIVE_WORKER_CONNECTION_SESSION_H_
#include "WorkerNativeSession.h"
#include "WorkerSessionScope.h"
namespace dust3d {
namespace worker {
    /// Exclusive document/cache owner created by one authenticated host connection.
    /// No registry or mutable cache is shared across connection owners. Calls serialize;
    /// caller keeps this wrapper alive through all calls. No concurrent destruction.
    class ConnectionGenerationSession {
    public:
        /// Validate scope before native owner creation; zero cache disables retention.
        /// Throws original SessionScopeError, ProtocolError, native or allocation errors.
        /// Example: ConnectionGenerationSession session(document, generation, cacheBytes);
        ConnectionGenerationSession(const std::array<uint8_t, 16>& document, uint64_t connectionGeneration, uint64_t maximumCachePayloadBytes);
        ConnectionGenerationSession(const ConnectionGenerationSession&) = delete;
        ConnectionGenerationSession& operator=(const ConnectionGenerationSession&) = delete;
        /// Admit the host-owned lifetime/document and consume an increasing request ID
        /// before native work; rejected scope never runs generation. Accepted attempts
        /// consume IDs even on policy/work errors. Returns independent output/metrics.
        /// Throws original scope/argument/native-copy errors. Host still admits revision
        /// publication separately. Example: auto value = session.generateSourcedIndexed(bytes, id, generation, {});
        NativeSessionGeneration generateSourcedIndexed(const std::vector<uint8_t>& archive, const RequestIdentity& identity, uint64_t connectionGeneration, const NativeSessionOptions& options);
        /// Admit lifetime/replay before opt-in native-budget borrowed archive generation.
        /// Accepted IDs are consumed even on argument/copy errors. Caller retains bytes;
        /// host separately authenticates peer and admits current-source publication.
        /// Throws original scope/argument/receipt-copy errors; no new transport route.
        /// Example: auto value = session.generatePreview(owner.view(), id, generation);
        NativeSessionGeneration generatePreview(PreviewArchiveView archive, const RequestIdentity& identity, uint64_t connectionGeneration, const GenerationStatisticsLimits& statisticsLimits = {});
        /// Copy current actual observations for this live lifetime; no invented metrics.
        /// Throws original scope/query errors. Example: auto stats = session.statistics(generation);
        GenerationStatistics statistics(uint64_t connectionGeneration);
        /// Reset live retention without resetting request replay protection.
        /// Throws original scope/native errors. Example: session.reset(generation);
        void reset(uint64_t connectionGeneration);
        /// Retire admission and release native cache after any serialized call completes.
        /// Idempotent; does not interrupt native execution or authenticate transport.
        /// Example: session.close();
        void close();

    private:
        std::mutex m_mutex;
        SessionScope m_scope;
        std::unique_ptr<NativeGenerationSession> m_owner;
    };
}
}
#endif

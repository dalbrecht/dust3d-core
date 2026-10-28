// SPDX-License-Identifier: MIT
#include "WorkerConnectionSession.h"
namespace dust3d {
namespace worker {
    ConnectionGenerationSession::ConnectionGenerationSession(const std::array<uint8_t, 16>& document, uint64_t connectionGeneration, uint64_t maximumCachePayloadBytes)
        : m_scope(document, connectionGeneration)
        , m_owner(std::make_unique<NativeGenerationSession>(document, maximumCachePayloadBytes))
    {
    }
    NativeSessionGeneration ConnectionGenerationSession::generateSourcedIndexed(const std::vector<uint8_t>& archive, const RequestIdentity& identity, uint64_t connectionGeneration, const NativeSessionOptions& options)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scope.admit(identity, connectionGeneration);
        return m_owner->generateSourcedIndexed(archive, identity, options);
    }
    GenerationStatistics ConnectionGenerationSession::statistics(uint64_t connectionGeneration)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scope.requireConnection(connectionGeneration);
        return m_owner->statistics();
    }
    NativeSessionGeneration ConnectionGenerationSession::generatePreview(PreviewArchiveView archive, const RequestIdentity& identity, uint64_t connectionGeneration, const GenerationStatisticsLimits& statisticsLimits)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scope.admit(identity, connectionGeneration);
        return m_owner->generatePreview(archive, identity, statisticsLimits);
    }
    void ConnectionGenerationSession::reset(uint64_t connectionGeneration)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scope.requireConnection(connectionGeneration);
        m_owner->reset();
    }
    void ConnectionGenerationSession::close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_scope.close();
        m_owner.reset();
    }
}
}

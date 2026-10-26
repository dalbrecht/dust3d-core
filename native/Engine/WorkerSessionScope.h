// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_SCOPE_H_
#define DUST3D_NATIVE_WORKER_SESSION_SCOPE_H_
#include "WorkerProtocol.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    enum class SessionScopeErrorCode { invalidScope,
        closed,
        wrongConnection,
        wrongDocument,
        reusedRequest };
    class SessionScopeError : public std::runtime_error {
    public:
        SessionScopeError(SessionScopeErrorCode code, const char* cause)
            : std::runtime_error(cause)
            , m_code(code)
        {
        }
        SessionScopeErrorCode code() const noexcept { return m_code; }

    private:
        SessionScopeErrorCode m_code;
    };
    /// Pure externally serialized lifetime admission. The host supplies document and
    /// connection scope from its authenticated owner, never from a reply or receipt.
    /// No I/O, native state, authentication or current-revision publication lives here.
    class SessionScope {
    public:
        /// Require nonzero immutable document and connection lifetime before ownership.
        /// Throws SessionScopeError(invalidScope). Example: SessionScope scope(document, generation);
        SessionScope(const std::array<uint8_t, 16>& document, uint64_t connectionGeneration)
            : m_document(document)
            , m_connectionGeneration(connectionGeneration)
        {
            if (!connectionGeneration || std::all_of(document.begin(), document.end(), [](uint8_t value) { return !value; }))
                throw SessionScopeError(SessionScopeErrorCode::invalidScope, "Worker session requires a host document and connection lifetime.");
        }
        SessionScope(const SessionScope&) = delete;
        SessionScope& operator=(const SessionScope&) = delete;
        /// Require this live host lifetime, without consuming a request.
        /// Throws closed/wrongConnection. Example: scope.requireConnection(generation);
        void requireConnection(uint64_t connectionGeneration) const
        {
            if (m_isClosed)
                throw SessionScopeError(SessionScopeErrorCode::closed, "Worker session is closed.");
            if (connectionGeneration != m_connectionGeneration)
                throw SessionScopeError(SessionScopeErrorCode::wrongConnection, "Worker session belongs to another connection lifetime.");
        }
        /// Admit exact document and strictly increasing nonzero request ID. Rejections
        /// preserve ordering state; accepted attempts consume the ID even if work fails.
        /// Revision is carried unchanged; PendingServiceReply admits current publication.
        /// Throws original scope errors. Example: scope.admit(request, generation);
        void admit(const RequestIdentity& identity, uint64_t connectionGeneration)
        {
            requireConnection(connectionGeneration);
            if (identity.document != m_document)
                throw SessionScopeError(SessionScopeErrorCode::wrongDocument, "Worker session belongs to another document.");
            if (!identity.request || identity.request <= m_lastRequest)
                throw SessionScopeError(SessionScopeErrorCode::reusedRequest, "Worker session request ID must increase.");
            m_lastRequest = identity.request;
        }
        /// Permanently retire admission. Idempotent; does not cancel native execution.
        /// Example: scope.close();
        void close() noexcept { m_isClosed = true; }

    private:
        std::array<uint8_t, 16> m_document;
        uint64_t m_connectionGeneration;
        uint64_t m_lastRequest = 0;
        bool m_isClosed = false;
    };
}
}
#endif

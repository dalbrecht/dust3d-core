// SPDX-License-Identifier: MIT
#include "WorkerReplyCollector.h"
#include <algorithm>
#include <limits>
namespace dust3d {
namespace worker {
    static_assert(maximumPayloadBytes <= std::numeric_limits<size_t>::max() - headerBytes, "Bounded reply spans must fit size_t.");
    WorkerReplyCollector::WorkerReplyCollector(const Header& request, const Limits& limits, std::vector<uint8_t>& output)
        : m_request(request)
        , m_limits(limits)
        , m_output(output)
    {
        if (!output.empty())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply collector requires empty output storage.");
        (void)encodeHeader(request, limits);
        if (request.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Worker reply collector requires a pending request.");
        m_output.reserve(headerBytes);
    }
    void WorkerReplyCollector::checkState() const
    {
        if (m_failed || m_output.size() != m_received)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply collector is rejected or its borrowed storage changed.");
    }
    size_t WorkerReplyCollector::nextReadBytes(size_t maximumChunk) const
    {
        checkState();
        if (!maximumChunk || maximumChunk > 65536)
            throw ProtocolError(ProtocolErrorCode::invalidLimits, "Worker reply read chunk is unsupported.");
        if (m_finished)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply collector has observed EOF.");
        const auto remaining = m_expectedBytes - m_received;
        return std::min(maximumChunk, remaining + (m_hasHeader ? 1 : 0));
    }
    void WorkerReplyCollector::append(const uint8_t* bytes, size_t size)
    {
        try {
            checkState();
            if (!size)
                return;
            if (!bytes)
                throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker reply span is null.");
            if (m_finished)
                throw ProtocolError(ProtocolErrorCode::trailingData, "Worker sent bytes after observed EOF.");
            if (!m_hasHeader) {
                const auto count = std::min(size, headerBytes - m_received);
                m_output.insert(m_output.end(), bytes, bytes + count);
                m_received += count;
                bytes += count;
                size -= count;
                if (m_received < headerBytes)
                    return;
                const auto header = decodeHeader(m_output.data(), headerBytes, m_limits);
                validateResponseIdentity(header, m_request);
                m_expectedBytes = headerBytes + static_cast<size_t>(header.payloadBytes);
                m_hasHeader = true;
            }
            if (size > m_expectedBytes - m_received)
                throw ProtocolError(ProtocolErrorCode::trailingData, "Worker sent bytes beyond the advertised response");
            m_output.insert(m_output.end(), bytes, bytes + size);
            m_received += size;
        } catch (...) {
            m_failed = true;
            throw;
        }
    }
    void WorkerReplyCollector::finish()
    {
        checkState();
        if (!m_hasHeader || m_received != m_expectedBytes) {
            m_failed = true;
            throw ProtocolError(ProtocolErrorCode::truncated, "Worker reply is incomplete at EOF.");
        }
        m_finished = true;
    }
}
}

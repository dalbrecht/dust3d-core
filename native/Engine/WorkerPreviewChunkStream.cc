// SPDX-License-Identifier: MIT
#include "WorkerPreviewChunkStream.h"
#include <algorithm>
#include <cstring>
#include <utility>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(PreviewChunkErrorCode code, const char* cause) { throw PreviewChunkError(code, cause); }
    }
    void detail::PreviewStreamStatus::check() const
    {
        if (m_failure)
            std::rethrow_exception(m_failure);
        if (m_insideSink)
            reject(PreviewChunkErrorCode::invalidSequence, "Preview stream callbacks must not reenter their endpoint.");
    }
    void detail::PreviewStreamStatus::capture() noexcept
    {
        if (!m_failure)
            m_failure = std::current_exception();
    }
    PreviewChunkWriter::PreviewChunkWriter(PreviewChunkIdentity identity, uint64_t totalBytes, PreviewChunkPolicy policy, PreviewChunkWriteSink sink)
        : m_identity(identity)
        , m_totalBytes(totalBytes)
        , m_policy(policy)
        , m_begin(encodePreviewChunkHeader({ PreviewChunkKind::begin, identity, 0, totalBytes, 0 }, policy))
        , m_sink(std::move(sink))
        , m_workspace(policy.chunkPayloadBytes)
    {
        // Header construction validates the policy before workspace allocation.
        if (!m_sink)
            reject(PreviewChunkErrorCode::invalidInput, "Preview byte writer requires a complete-span sink.");
    }
    void PreviewChunkWriter::start()
    {
        try {
            m_status.check();
            if (m_state != State::ready)
                reject(PreviewChunkErrorCode::invalidSequence, "Preview byte writer already started or finished.");
            m_state = State::writing;
            m_status.invoke([&] { m_sink(m_begin.data(), m_begin.size()); });
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    void PreviewChunkWriter::emitData()
    {
        if (!m_used)
            return;
        // Used bytes never exceed the policy-sized workspace (at most64KiB).
        const auto header = encodePreviewChunkHeader({ PreviewChunkKind::data, m_identity, m_emittedBytes, m_totalBytes, static_cast<uint32_t>(m_used) }, m_policy);
        m_status.invoke([&] { m_sink(header.data(), header.size()); m_sink(m_workspace.data(), m_used); });
        m_emittedBytes += m_used;
        m_used = 0;
    }
    void PreviewChunkWriter::append(const uint8_t* bytes, size_t size)
    {
        try {
            m_status.check();
            if (m_state != State::writing)
                reject(PreviewChunkErrorCode::invalidSequence, "Preview byte writer requires an active transfer.");
            if (!bytes && size)
                reject(PreviewChunkErrorCode::invalidInput, "Preview byte writer span is missing.");
            if (size > m_totalBytes - m_receivedBytes)
                reject(PreviewChunkErrorCode::payloadMismatch, "Preview byte writer span exceeds the promised remainder.");
            size_t offset = 0;
            while (offset < size) {
                const auto count = std::min(size - offset, m_workspace.size() - m_used);
                std::memcpy(m_workspace.data() + m_used, bytes + offset, count);
                m_used += count;
                m_receivedBytes += count;
                offset += count;
                if (m_used == m_workspace.size())
                    emitData();
            }
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    void PreviewChunkWriter::finish()
    {
        try {
            m_status.check();
            if (m_state != State::writing)
                reject(PreviewChunkErrorCode::invalidSequence, "Preview byte writer cannot finish outside its active transfer.");
            if (m_receivedBytes != m_totalBytes)
                reject(PreviewChunkErrorCode::payloadMismatch, "Preview byte writer body is incomplete.");
            emitData();
            const auto end = encodePreviewChunkHeader({ PreviewChunkKind::end, m_identity, m_totalBytes, m_totalBytes, 0 }, m_policy);
            m_status.invoke([&] { m_sink(end.data(), end.size()); });
            m_state = State::finished;
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    PreviewChunkReader::PreviewChunkReader(PreviewChunkIdentity expected, PreviewChunkPolicy policy, PreviewChunkReadSink sink)
        : m_expected(expected)
        , m_policy(policy)
        , m_sequence(expected, policy)
        , m_sink(std::move(sink))
        , m_workspace(policy.chunkPayloadBytes)
    {
        // Sequence construction validates frozen policy before allocating workspace.
        if (!m_sink)
            reject(PreviewChunkErrorCode::invalidInput, "Preview byte reader requires a provisional body sink.");
    }
    void PreviewChunkReader::emitPending()
    {
        const auto header = *m_pending;
        m_sequence.consume(header, m_bodyUsed);
        m_ended = header.kind == PreviewChunkKind::end;
        m_status.invoke([&] { m_sink(header, m_bodyUsed ? m_workspace.data() : nullptr, m_bodyUsed); });
        m_pending.reset();
        m_bodyUsed = 0;
    }
    void PreviewChunkReader::append(const uint8_t* bytes, size_t size)
    {
        try {
            m_status.check();
            if (m_sequence.isFramed())
                reject(PreviewChunkErrorCode::invalidSequence, "Preview byte reader already observed EOF.");
            if (!bytes && size)
                reject(PreviewChunkErrorCode::invalidInput, "Preview byte reader span is missing.");
            size_t offset = 0;
            while (offset < size) {
                if (m_ended)
                    reject(PreviewChunkErrorCode::invalidSequence, "Preview byte reader received trailing input after end.");
                if (!m_pending) {
                    const auto count = std::min(size - offset, m_header.size() - m_headerUsed);
                    std::memcpy(m_header.data() + m_headerUsed, bytes + offset, count);
                    m_headerUsed += count;
                    offset += count;
                    if (m_headerUsed != m_header.size())
                        continue;
                    auto header = decodePreviewChunkHeader(m_header.data(), m_header.size(), m_policy);
                    // Reject foreign scope before retaining even one body chunk.
                    if (!(header.identity == m_expected))
                        reject(PreviewChunkErrorCode::identityMismatch, "Preview byte stream differs from its captured identity.");
                    m_pending = header;
                    m_headerUsed = 0;
                    if (!header.payloadBytes)
                        emitPending();
                } else {
                    const auto count = std::min(size - offset, size_t(m_pending->payloadBytes) - m_bodyUsed);
                    std::memcpy(m_workspace.data() + m_bodyUsed, bytes + offset, count);
                    m_bodyUsed += count;
                    offset += count;
                    if (m_bodyUsed == m_pending->payloadBytes)
                        emitPending();
                }
            }
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    void PreviewChunkReader::finishAtEOF()
    {
        try {
            m_status.check();
            if (m_headerUsed || m_pending || m_bodyUsed)
                reject(PreviewChunkErrorCode::invalidSequence, "Preview byte stream ended inside a header or body.");
            m_sequence.finishAtEOF();
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
}
}

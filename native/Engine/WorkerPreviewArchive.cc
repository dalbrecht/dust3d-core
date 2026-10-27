// SPDX-License-Identifier: MIT
#include "WorkerPreviewArchive.h"
#include <algorithm>
#include <cstring>
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(PreviewChunkErrorCode code, const char* cause) { throw PreviewChunkError(code, cause); }
        PreviewChunkIdentity validateTransfer(PreviewChunkIdentity identity, const PreviewArchiveContext& context, const PreviewChunkPolicy& policy)
        {
            if (identity.direction != PreviewChunkDirection::archive)
                reject(PreviewChunkErrorCode::invalidIdentity, "Preview archive requires the archive direction.");
            measurePreviewChunking(sourceRequestPrefixBytes + context.archiveBytes, policy);
            return identity;
        }
    }
    PreviewArchiveContext detail::checkedPreviewArchiveContext(PreviewArchiveContext context)
    {
        if (!context.archiveBytes || context.archiveBytes > maximumPayloadBytes || context.archiveBytes > std::numeric_limits<size_t>::max() - sourceRequestPrefixBytes
            || context.maximumSourceBytes > maximumPayloadBytes || context.maximumIndexedBytes > std::numeric_limits<size_t>::max())
            reject(PreviewChunkErrorCode::invalidLimits, "Captured preview archive/options exceed native supported bounds.");
        return context;
    }
    PreviewArchiveCollector::PreviewArchiveCollector(PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy)
        : m_context(detail::checkedPreviewArchiveContext(context))
        , m_reader(validateTransfer(identity, m_context, policy), policy, [this](const PreviewChunkHeader& header, const uint8_t* bytes, size_t size) { consume(header, bytes, size); })
    {
    }
    void PreviewArchiveCollector::consume(const PreviewChunkHeader& header, const uint8_t* bytes, size_t size)
    {
        if (header.kind == PreviewChunkKind::begin) {
            if (header.totalBytes != sourceRequestPrefixBytes + m_context.archiveBytes)
                reject(PreviewChunkErrorCode::lengthMismatch, "Preview transfer length differs from the captured archive size.");
            return;
        }
        if (header.kind != PreviewChunkKind::data)
            return;
        if (m_prefixUsed < m_prefix.size()) {
            const auto count = std::min(size, m_prefix.size() - m_prefixUsed);
            std::memcpy(m_prefix.data() + m_prefixUsed, bytes, count);
            m_prefixUsed += count;
            bytes += count;
            size -= count;
            if (m_prefixUsed == m_prefix.size()) {
                const auto metadata = inspectSourceRequestPrefix(m_prefix.data(), m_prefix.size());
                if (metadata.archiveBytes != m_context.archiveBytes || metadata.maximumSourceBytes != m_context.maximumSourceBytes)
                    reject(PreviewChunkErrorCode::payloadMismatch, "Preview archive prefix differs from captured request controls.");
                // Native C entry points need contiguous readable archive bytes.
                // Allocate once, only after independently captured length and prefix agree.
                m_archive.reset(new uint8_t[static_cast<size_t>(m_context.archiveBytes)]);
            }
        }
        if (size) {
            // Framing enforces a contiguous exact total fixed by the begin callback.
            // This subtraction guard also keeps owned-buffer writes locally bounded.
            if (size > m_context.archiveBytes - m_archiveUsed)
                reject(PreviewChunkErrorCode::payloadMismatch, "Preview archive exceeds its captured remainder.");
            std::memcpy(m_archive.get() + m_archiveUsed, bytes, size);
            m_archiveUsed += size;
        }
    }
    void PreviewArchiveCollector::append(const uint8_t* bytes, size_t size)
    {
        try {
            m_status.check();
            m_reader.append(bytes, size);
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    void PreviewArchiveCollector::finishAtEOF()
    {
        try {
            m_status.check();
            m_reader.finishAtEOF();
        } catch (...) {
            m_status.capture();
            throw;
        }
    }
    PreviewArchiveView PreviewArchiveCollector::view() const
    {
        m_status.check();
        if (!m_reader.isFramed())
            reject(PreviewChunkErrorCode::invalidSequence, "Preview archive is unavailable until complete transfer and actual EOF.");
        return { { m_archive.get(), static_cast<size_t>(m_context.archiveBytes), m_context.maximumSourceBytes }, m_context.maximumIndexedBytes };
    }
}
}

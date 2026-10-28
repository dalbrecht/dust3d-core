// SPDX-License-Identifier: MIT
#include "WorkerNativeSession.h"
#include "WorkerPreviewFailure.h"
#include "WorkerSourceNativeFailure.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        PreviewArchiveContext previewContext(PreviewArchiveView archive)
        {
            return { archive.source.archiveBytes, archive.source.maximumSourceBytes, archive.maximumIndexedBytes };
        }
        SessionSourceResult generateSourcedPreview(Dust3DGenerationSession* session, PreviewArchiveView archive)
        {
            try {
                Dust3DError error {};
                Dust3DSourceOptions options { sizeof(options), static_cast<size_t>(archive.source.maximumSourceBytes) };
                std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> source(
                    dust3d_generation_session_load_sourced(session, archive.source.archive, archive.source.archiveBytes, &options, &error), dust3d_sourced_mesh_destroy);
                if (error.status != DUST3D_STATUS_OK)
                    return copySourceNativeFailure(error.status, error.message);
                if (!source)
                    throw std::logic_error("Native preview source success returned no owner.");
                std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(
                    dust3d_mesh_create_indexed(dust3d_sourced_mesh_geometry(source.get()), static_cast<size_t>(archive.maximumIndexedBytes), &error), dust3d_indexed_mesh_destroy);
                if (error.status != DUST3D_STATUS_OK)
                    return copySourceNativeFailure(error.status, error.message);
                if (!indexed)
                    throw std::logic_error("Native preview indexed success returned no owner.");
                return copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), previewContext(archive));
            } catch (...) {
                return copyPreviewGenerationFailure(std::current_exception());
            }
        }
    }
    IndexedGenerationResult generateSessionPreviewIndexedMesh(Dust3DGenerationSession* session, PreviewArchiveView archive)
    {
        (void)detail::checkedPreviewArchiveContext(previewContext(archive));
        if (!session || !archive.source.archive)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Ordinary preview requires a live session and borrowed archive.");
        try {
            Dust3DError error {};
            std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> source(
                dust3d_generation_session_load(session, archive.source.archive, archive.source.archiveBytes, &error), dust3d_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copySourceNativeFailure(error.status, error.message);
            if (!source)
                throw std::logic_error("Native ordinary preview success returned no owner.");
            std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(
                dust3d_mesh_create_indexed(source.get(), static_cast<size_t>(archive.maximumIndexedBytes), &error), dust3d_indexed_mesh_destroy);
            if (error.status != DUST3D_STATUS_OK)
                return copySourceNativeFailure(error.status, error.message);
            if (!indexed)
                throw std::logic_error("Native ordinary preview indexed success returned no owner.");
            return copyNativePreviewIndexedMesh(indexed.get(), previewContext(archive));
        } catch (...) {
            return copyPreviewGenerationFailure(std::current_exception());
        }
    }
    NativeSessionError::NativeSessionError(FailurePayload failure)
        : std::runtime_error(std::string(failure.cause.begin(), failure.cause.end()))
        , m_failure(std::move(failure))
    {
    }
    NativeGenerationSession::NativeGenerationSession(const std::array<uint8_t, 16>& document, uint64_t maximumCachePayloadBytes)
        : m_document(document)
        , m_owner(nullptr, dust3d_generation_session_destroy)
    {
        if (std::all_of(document.begin(), document.end(), [](uint8_t value) { return !value; }) || maximumCachePayloadBytes > maximumPayloadBytes)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker session document or retained cache policy is invalid.");
        Dust3DError error {};
        m_owner.reset(dust3d_generation_session_create(static_cast<size_t>(maximumCachePayloadBytes), &error));
        if (!m_owner)
            throw NativeSessionError(copySourceNativeFailure(error.status, error.message));
    }
    GenerationStatistics NativeGenerationSession::statisticsUnlocked(const GenerationStatisticsLimits& limits)
    {
        Dust3DError error {};
        Dust3DGenerationStats stats {};
        if (dust3d_generation_session_get_stats(m_owner.get(), &stats, sizeof(stats), &error) != DUST3D_STATUS_OK)
            throw NativeSessionError(copySourceNativeFailure(error.status, error.message));
        return copyNativeGenerationStatistics(stats, limits);
    }
    GenerationStatistics NativeGenerationSession::statistics()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return statisticsUnlocked({});
    }
    void NativeGenerationSession::reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Dust3DError error {};
        if (dust3d_generation_session_reset(m_owner.get(), &error) != DUST3D_STATUS_OK)
            throw NativeSessionError(copySourceNativeFailure(error.status, error.message));
    }
    NativeSessionGeneration NativeGenerationSession::generateSourcedIndexed(const std::vector<uint8_t>& archive, const RequestIdentity& identity, const NativeSessionOptions& options)
    {
        Header header;
        header.identity = identity;
        header.payloadBytes = archive.size();
        (void)encodeHeader(header, {});
        if (identity.document != m_document || archive.empty())
            throw ProtocolError(ProtocolErrorCode::invalidIdentity, "Worker session request does not match its host document.");
        validateIndexedSourcesPayloadLimits(options.output);
        validateIndexedMeshPayloadLimits(options.ordinary);
        (void)encodeGenerationStatistics(std::nullopt, options.statistics);
        std::lock_guard<std::mutex> lock(m_mutex);
        auto result = resolveSessionGeneration(generateSessionIndexedSourcedMesh(m_owner.get(), archive, options.output), [&] {
            return generateSessionIndexedMesh(m_owner.get(), archive, options.ordinary);
        });
        try {
            attachSessionStatistics(result, statisticsUnlocked(options.statistics));
        } catch (...) {
            // Keep secondary observation errors from discarding generated geometry;
            // exception_ptr retains the original type/cause without copying a diagnostic.
            attachSessionStatistics(result, std::current_exception());
        }
        return result;
    }
    NativeSessionGeneration NativeGenerationSession::generatePreview(PreviewArchiveView archive, const RequestIdentity& identity, const GenerationStatisticsLimits& statisticsLimits)
    {
        if (!identity.request || identity.document != m_document)
            throw ProtocolError(ProtocolErrorCode::invalidIdentity, "Native preview request does not match its host document/request scope.");
        (void)detail::checkedPreviewArchiveContext(previewContext(archive));
        if (!archive.source.archive)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Native preview requires stable borrowed archive storage.");
        (void)encodeGenerationStatistics(std::nullopt, statisticsLimits);
        std::lock_guard<std::mutex> lock(m_mutex);
        auto result = resolveSessionGeneration(generateSourcedPreview(m_owner.get(), archive), [&] {
            return generateSessionPreviewIndexedMesh(m_owner.get(), archive);
        });
        try {
            attachSessionStatistics(result, statisticsUnlocked(statisticsLimits));
        } catch (...) {
            attachSessionStatistics(result, std::current_exception());
        }
        return result;
    }
}
}

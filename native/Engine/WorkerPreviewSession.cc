// SPDX-License-Identifier: MIT
#include "WorkerPreviewSession.h"
#include "WorkerMeshValidation.h"
#include "WorkerSessionOutputValidation.h"
#include "WorkerSourceHintValidation.h"
#include "WorkerWire.h"
#include "include/Dust3DBridge.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <type_traits>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void rejectSession(SessionOutputErrorCode code, const char* cause) { throw SessionOutputError(code, cause); }
        uint64_t add(uint64_t left, uint64_t right)
        {
            if (right > std::numeric_limits<uint64_t>::max() - left)
                throw PreviewRangeError(PreviewRangeErrorCode::arithmeticOverflow, "Preview session byte count overflows UInt64.");
            return left + right;
        }
        PreviewNativeBudgets budgets(const PreviewArchiveContext& context) { return { context.maximumIndexedBytes, context.maximumSourceBytes }; }
        uint64_t sourceBudget(const PreviewArchiveContext& context) { return context.maximumSourceBytes ? context.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES; }
        PreviewChunkIdentity sessionIdentity(PreviewChunkIdentity value)
        {
            if (value.direction != PreviewChunkDirection::session)
                throw PreviewChunkError(PreviewChunkErrorCode::invalidIdentity, "Preview session requires the session direction.");
            return value;
        }
        void validateMembership(const PreviewGeometryShape& shape, const PreviewArchiveContext& context)
        {
            // Native membership counts come from distinct definitions in the raw XML
            // model span, which is contained in the independently captured archive.
            if (shape.nodeCount > context.archiveBytes || shape.partCount > context.archiveBytes)
                throw PreviewRangeError(PreviewRangeErrorCode::invalidCounts, "Preview membership counts exceed their captured archive byte bound.");
        }
        uint64_t failureLogical(const FailurePayload& value)
        {
            return add(value.cause.size(), value.context ? value.context->identity.size() : 0);
        }
        struct Description {
            uint32_t kind;
            const IndexedMeshPayload* geometry = nullptr;
            const SourceHintPayload* sources = nullptr;
            PreviewGeometryShape shape;
            PreviewRecordPlan records;
            std::array<std::vector<uint8_t>, 4> auxiliary;
            std::array<uint64_t, 4> lengths {};
            PreviewTransferPlan transfer;
        };
        Description describe(const SessionOutput& value, const PreviewArchiveContext& context)
        {
            Description result;
            result.kind = detail::sessionOutputKind(value);
            detail::validateSessionOutputOutcome(value, result.kind);
            if (const auto* pair = std::get_if<IndexedSourcesPayload>(&value.outcome)) {
                result.geometry = &pair->geometry;
                result.sources = &pair->sources;
            } else
                result.geometry = std::get_if<IndexedMeshPayload>(&value.outcome);
            if (result.geometry) {
                const auto& mesh = *result.geometry;
                if (mesh.vertices.empty() || mesh.indices.empty())
                    throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidGeometry, "Preview mesh outcomes require nonempty geometry; use the explicit empty outcome.");
                if (mesh.warning.size() > std::numeric_limits<uint32_t>::max())
                    throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::limitExceeded, "Preview warning cannot fit its existing wire field.");
                result.shape = { mesh.vertices.size(), mesh.indices.size(), mesh.nodeCount, mesh.partCount, mesh.warning.size(), std::nullopt };
                validateMembership(result.shape, context);
                if (result.sources) {
                    const auto& sources = *result.sources;
                    uint64_t text = 0;
                    for (const auto& identity : sources.identities)
                        text = add(add(text, identity.node.size()), identity.part.size());
                    result.shape.sources = PreviewSourceShape { sources.identities.size(), sources.corners.size(), sources.influences.size(), text };
                }
                result.records = planPreviewRecords(result.shape, budgets(context));
                detail::validateMeshWarning(reinterpret_cast<const uint8_t*>(mesh.warning.data()), mesh.warning.size());
                for (const auto& vertex : mesh.vertices)
                    detail::validateMeshVertex(vertex);
                for (const auto index : mesh.indices)
                    detail::validateIndexedMeshIndex(index, mesh.vertices.size());
                for (size_t index = 0; index < mesh.indices.size(); index += 3)
                    detail::validateMeshTriangle({ mesh.vertices[mesh.indices[index]], mesh.vertices[mesh.indices[index + 1]], mesh.vertices[mesh.indices[index + 2]] });
                if (result.sources) {
                    const auto& sources = *result.sources;
                    for (const auto& identity : sources.identities) {
                        detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(identity.node.data()), identity.node.size(), sourceBudget(context));
                        detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(identity.part.data()), identity.part.size(), sourceBudget(context));
                    }
                    for (const auto& influence : sources.influences)
                        detail::validateSourceHintInfluence(influence, sources.identities.size());
                    for (const auto& corner : sources.corners)
                        detail::validateSourceHintCorner(corner, sources.influences.size(), [&](uint64_t index) { return sources.influences.at(static_cast<size_t>(index)).kind; });
                }
            }
            uint64_t logical = 0, wire = 0;
            if (const auto* failure = std::get_if<FailurePayload>(&value.outcome)) {
                result.auxiliary[0] = encodeFailurePayload(*failure, {});
                logical = add(logical, failureLogical(*failure));
            }
            if (value.sourceIssue) {
                result.auxiliary[1] = encodeFailurePayload(*value.sourceIssue, {});
                logical = add(logical, failureLogical(*value.sourceIssue));
            }
            result.auxiliary[2] = encodeGenerationStatistics(value.statistics, {});
            if (value.statistics)
                logical = add(logical, value.statistics->cacheCause.size());
            if (value.statisticsIssue) {
                result.auxiliary[3] = encodeFailurePayload(*value.statisticsIssue, {});
                logical = add(logical, failureLogical(*value.statisticsIssue));
            }
            for (size_t index = 0; index < result.lengths.size(); ++index) {
                result.lengths[index] = result.auxiliary[index].size();
                wire = add(wire, result.lengths[index]);
            }
            PreviewTransferShape shape;
            shape.archiveBytes = context.archiveBytes;
            shape.vertexCount = result.shape.vertexCount;
            shape.indexCount = result.shape.indexCount;
            shape.nodeCount = result.shape.nodeCount;
            shape.partCount = result.shape.partCount;
            shape.warningBytes = result.shape.warningBytes;
            shape.sources = result.shape.sources;
            shape.auxiliaryWireBytes = wire;
            shape.auxiliaryLogicalBytes = logical;
            result.transfer = planPreviewTransfer(shape, budgets(context));
            if (result.geometry)
                result.lengths[0] = result.records.primaryWireBytes;
            return result;
        }
        std::array<uint8_t, sessionOutputPrefixBytes> sessionPrefix(const Description& value)
        {
            std::array<uint8_t, sessionOutputPrefixBytes> bytes {};
            detail::writeUnsigned(bytes.data(), 1, 2);
            detail::writeUnsigned(bytes.data() + 2, sessionOutputPrefixBytes, 2);
            detail::writeUnsigned(bytes.data() + 4, value.kind, 4);
            for (size_t index = 0; index < value.lengths.size(); ++index)
                detail::writeUnsigned(bytes.data() + 8 + index * 8, value.lengths[index], 8);
            return bytes;
        }
        void writeGeometry(PreviewChunkWriter& writer, const Description& value)
        {
            const auto& mesh = *value.geometry;
            if (value.sources) {
                std::array<uint8_t, indexedSourcesPrefixBytes> pair {};
                detail::writeUnsigned(pair.data(), 2, 2);
                detail::writeUnsigned(pair.data() + 2, indexedSourcesPrefixBytes, 2);
                detail::writeUnsigned(pair.data() + 8, value.records.geometryWireBytes, 8);
                detail::writeUnsigned(pair.data() + 16, value.records.sourceWireBytes, 8);
                writer.append(pair.data(), pair.size());
            }
            std::array<uint8_t, indexedMeshPrefixBytes> prefix {};
            detail::writeUnsigned(prefix.data(), 1, 2);
            detail::writeUnsigned(prefix.data() + 2, indexedMeshPrefixBytes, 2);
            detail::writeUnsigned(prefix.data() + 8, mesh.vertices.size(), 8);
            detail::writeUnsigned(prefix.data() + 16, mesh.indices.size(), 8);
            detail::writeUnsigned(prefix.data() + 24, mesh.nodeCount, 8);
            detail::writeUnsigned(prefix.data() + 32, mesh.partCount, 8);
            detail::writeUnsigned(prefix.data() + 40, meshVertexBytes, 4);
            detail::writeUnsigned(prefix.data() + 44, mesh.warning.size(), 4);
            writer.append(prefix.data(), prefix.size());
            writer.append(reinterpret_cast<const uint8_t*>(mesh.warning.data()), mesh.warning.size());
            std::array<uint8_t, meshVertexBytes> vertexBytes;
            for (const auto& vertex : mesh.vertices) {
                detail::writeMeshVertex(vertexBytes.data(), vertex);
                writer.append(vertexBytes.data(), vertexBytes.size());
            }
            std::array<uint8_t, 4> indexBytes;
            for (const auto index : mesh.indices) {
                detail::writeUnsigned(indexBytes.data(), index, 4);
                writer.append(indexBytes.data(), indexBytes.size());
            }
        }
        void writeSources(PreviewChunkWriter& writer, const Description& value)
        {
            const auto& sources = *value.sources;
            std::array<uint8_t, sourceHintPrefixBytes> prefix {};
            detail::writeUnsigned(prefix.data(), 1, 4);
            detail::writeUnsigned(prefix.data() + 8, sources.identities.size(), 8);
            detail::writeUnsigned(prefix.data() + 16, sources.corners.size(), 8);
            detail::writeUnsigned(prefix.data() + 24, sources.influences.size(), 8);
            detail::writeUnsigned(prefix.data() + 32, value.shape.sources->textBytes, 8);
            writer.append(prefix.data(), prefix.size());
            std::array<uint8_t, 8> lengths;
            for (const auto& identity : sources.identities) {
                detail::writeUnsigned(lengths.data(), identity.node.size(), 4);
                detail::writeUnsigned(lengths.data() + 4, identity.part.size(), 4);
                writer.append(lengths.data(), lengths.size());
                writer.append(reinterpret_cast<const uint8_t*>(identity.node.data()), identity.node.size());
                writer.append(reinterpret_cast<const uint8_t*>(identity.part.data()), identity.part.size());
            }
            for (const auto& corner : sources.corners) {
                std::array<uint8_t, sourceHintCornerBytes> bytes {};
                detail::writeUnsigned(bytes.data(), static_cast<uint32_t>(corner.kind), 4);
                detail::writeUnsigned(bytes.data() + 8, corner.firstInfluence, 8);
                detail::writeUnsigned(bytes.data() + 16, corner.influenceCount, 8);
                writer.append(bytes.data(), bytes.size());
            }
            for (const auto& influence : sources.influences) {
                std::array<uint8_t, sourceHintInfluenceBytes> bytes {};
                detail::writeUnsigned(bytes.data(), static_cast<uint32_t>(influence.kind), 4);
                detail::writeUnsigned(bytes.data() + 8, influence.identityIndex, 8);
                uint32_t bits;
                std::memcpy(&bits, &influence.weight, sizeof(bits));
                detail::writeUnsigned(bytes.data() + 16, bits, 4);
                writer.append(bytes.data(), bytes.size());
            }
        }
    }
    void writePreviewSession(PreviewChunkIdentity identity, PreviewArchiveContext context, const SessionOutput& value, PreviewChunkPolicy policy, PreviewChunkWriteSink sink)
    {
        context = detail::checkedPreviewArchiveContext(context);
        identity = sessionIdentity(identity);
        const auto description = describe(value, context);
        PreviewChunkWriter writer(identity, description.transfer.sessionPayloadBytes, policy, std::move(sink));
        const auto prefix = sessionPrefix(description);
        writer.start();
        writer.append(prefix.data(), prefix.size());
        if (description.geometry) {
            writeGeometry(writer, description);
            if (description.sources)
                writeSources(writer, description);
        }
        for (const auto& section : description.auxiliary)
            writer.append(section.data(), section.size());
        writer.finish();
    }

    struct PreviewSessionCollector::Implementation {
        enum class Phase { session,
            pair,
            mesh,
            warning,
            vertex,
            index,
            source,
            identity,
            node,
            part,
            corner,
            influence,
            auxiliary,
            done };
        explicit Implementation(PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy)
            : m_context(detail::checkedPreviewArchiveContext(context))
            , m_reader(sessionIdentity(identity), policy, [this](const PreviewChunkHeader& header, const uint8_t* bytes, size_t size) { consume(header, bytes, size); })
        {
        }
        IndexedMeshPayload& mesh()
        {
            if (m_kind == 1)
                return std::get<IndexedSourcesPayload>(m_value.outcome).geometry;
            return std::get<IndexedMeshPayload>(m_value.outcome);
        }
        SourceHintPayload& sources() { return std::get<IndexedSourcesPayload>(m_value.outcome).sources; }
        void enter(Phase phase, uint64_t count)
        {
            m_phase = phase;
            m_remaining = count;
            m_fixedUsed = 0;
        }
        void beginAuxiliary(size_t index)
        {
            m_auxiliaryIndex = index;
            m_auxiliary.clear();
            enter(Phase::auxiliary, m_lengths[index]);
        }
        void beginCorners()
        {
            if (m_textObserved != m_shape.sources->textBytes)
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Preview source identity text total is inconsistent.");
            enter(Phase::corner, sourceHintCornerBytes);
        }
        void readSession()
        {
            if (detail::readUnsigned(m_fixed.data(), 2) != 1)
                rejectSession(SessionOutputErrorCode::unsupportedVersion, "Session output version is unsupported.");
            m_kind = static_cast<uint32_t>(detail::readUnsigned(m_fixed.data() + 4, 4));
            if (detail::readUnsigned(m_fixed.data() + 2, 2) != sessionOutputPrefixBytes || m_kind < 1 || m_kind > 4
                || std::any_of(m_fixed.begin() + 40, m_fixed.end(), [](uint8_t byte) { return byte != 0; }))
                rejectSession(SessionOutputErrorCode::invalidLayout, "Session output layout or reserved fields are invalid.");
            uint64_t total = sessionOutputPrefixBytes;
            for (size_t index = 0; index < m_lengths.size(); ++index) {
                m_lengths[index] = detail::readUnsigned(m_fixed.data() + 8 + index * 8, 8);
                total = add(total, m_lengths[index]);
            }
            if (total != m_totalBytes || ((m_kind == 3) != (m_lengths[0] == 0)) || m_lengths[2] < generationStatisticsPrefixBytes)
                rejectSession(SessionOutputErrorCode::invalidLayout, "Preview session section lengths differ from its exact transfer.");
            for (const auto index : { size_t(1), size_t(3) })
                if (m_lengths[index] > FailurePayloadLimits {}.maximumWireBytes)
                    rejectSession(SessionOutputErrorCode::limitExceeded, "Preview session diagnostic section exceeds its independent policy.");
            if (m_lengths[2] > GenerationStatisticsLimits {}.maximumWireBytes
                || (m_kind == 4 && m_lengths[0] > FailurePayloadLimits {}.maximumWireBytes))
                rejectSession(SessionOutputErrorCode::limitExceeded, "Preview session observation/failure section exceeds its independent policy.");
            if (m_kind == 1) {
                m_value.outcome = IndexedSourcesPayload {};
                enter(Phase::pair, indexedSourcesPrefixBytes);
            } else if (m_kind == 2) {
                m_value.outcome = IndexedMeshPayload {};
                m_geometryBytes = m_lengths[0];
                enter(Phase::mesh, indexedMeshPrefixBytes);
            } else
                beginAuxiliary(m_kind == 4 ? 0 : 1);
        }
        void readPair()
        {
            if (detail::readUnsigned(m_fixed.data(), 2) != 2)
                throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::unsupportedVersion, "Indexed/source pair version is unsupported.");
            if (detail::readUnsigned(m_fixed.data() + 2, 2) != indexedSourcesPrefixBytes || detail::readUnsigned(m_fixed.data() + 4, 4))
                throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::invalidLayout, "Indexed/source pair prefix is invalid.");
            m_geometryBytes = detail::readUnsigned(m_fixed.data() + 8, 8);
            m_sourceBytes = detail::readUnsigned(m_fixed.data() + 16, 8);
            if (m_geometryBytes < indexedMeshPrefixBytes || m_sourceBytes < sourceHintPrefixBytes
                || add(add(indexedSourcesPrefixBytes, m_geometryBytes), m_sourceBytes) != m_lengths[0])
                throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::invalidLayout, "Indexed/source pair lengths are inconsistent.");
            enter(Phase::mesh, indexedMeshPrefixBytes);
        }
        void readMesh()
        {
            if (detail::readUnsigned(m_fixed.data(), 2) != 1)
                throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::unsupportedVersion, "Indexed geometry version is unsupported.");
            if (detail::readUnsigned(m_fixed.data() + 2, 2) != indexedMeshPrefixBytes || detail::readUnsigned(m_fixed.data() + 4, 4)
                || detail::readUnsigned(m_fixed.data() + 40, 4) != meshVertexBytes)
                throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidLayout, "Indexed geometry prefix is invalid.");
            m_shape = { detail::readUnsigned(m_fixed.data() + 8, 8), detail::readUnsigned(m_fixed.data() + 16, 8),
                detail::readUnsigned(m_fixed.data() + 24, 8), detail::readUnsigned(m_fixed.data() + 32, 8), detail::readUnsigned(m_fixed.data() + 44, 4), std::nullopt };
            validateMembership(m_shape, m_context);
            const auto plan = planPreviewRecords(m_shape, budgets(m_context));
            if (plan.geometryWireBytes != m_geometryBytes)
                throw IndexedMeshPayloadError(IndexedMeshPayloadErrorCode::invalidLayout, "Indexed geometry counts disagree with its exact section length.");
            mesh().nodeCount = m_shape.nodeCount;
            mesh().partCount = m_shape.partCount;
            enter(Phase::warning, m_shape.warningBytes);
        }
        void readSource()
        {
            if (detail::readUnsigned(m_fixed.data(), 4) != 1)
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::unsupportedVersion, "Worker source version is unsupported.");
            if (detail::readUnsigned(m_fixed.data() + 4, 4))
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Worker source reserved prefix is nonzero.");
            m_shape.sources = PreviewSourceShape { detail::readUnsigned(m_fixed.data() + 8, 8), detail::readUnsigned(m_fixed.data() + 16, 8),
                detail::readUnsigned(m_fixed.data() + 24, 8), detail::readUnsigned(m_fixed.data() + 32, 8) };
            const auto plan = planPreviewRecords(m_shape, budgets(m_context));
            if (plan.sourceWireBytes != m_sourceBytes)
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Source counts disagree with their exact section length.");
            if (m_shape.sources->identities)
                enter(Phase::identity, 8);
            else
                beginCorners();
        }
        void readIdentity()
        {
            m_nodeBytes = detail::readUnsigned(m_fixed.data(), 4);
            m_partBytes = detail::readUnsigned(m_fixed.data() + 4, 4);
            if (!m_nodeBytes || !m_partBytes || m_nodeBytes > sourceBudget(m_context) || m_partBytes > sourceBudget(m_context))
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidIdentity, "Preview source identity lengths exceed the native source bound.");
            const auto text = m_shape.sources->textBytes;
            if (m_textObserved > text || m_nodeBytes > text - m_textObserved || m_partBytes > text - m_textObserved - m_nodeBytes)
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Preview source identity text exceeds the declared remainder.");
            m_node.clear();
            m_part.clear();
            enter(Phase::node, m_nodeBytes);
        }
        void finishAuxiliary()
        {
            if (m_auxiliaryIndex == 0)
                m_value.outcome = decodeFailurePayload(m_auxiliary.data(), m_auxiliary.size(), {});
            else if (m_auxiliaryIndex == 1 && !m_auxiliary.empty())
                m_value.sourceIssue = decodeFailurePayload(m_auxiliary.data(), m_auxiliary.size(), {});
            else if (m_auxiliaryIndex == 2)
                m_value.statistics = decodeGenerationStatistics(m_auxiliary.data(), m_auxiliary.size(), {});
            else if (m_auxiliaryIndex == 3 && !m_auxiliary.empty())
                m_value.statisticsIssue = decodeFailurePayload(m_auxiliary.data(), m_auxiliary.size(), {});
            if (m_auxiliaryIndex < 3) {
                beginAuxiliary(m_auxiliaryIndex + 1);
                return;
            }
            const auto description = describe(m_value, m_context);
            if (description.transfer.sessionPayloadBytes != m_totalBytes || description.lengths != m_lengths)
                rejectSession(SessionOutputErrorCode::invalidLayout, "Owned preview result differs from its declared sections.");
            enter(Phase::done, 0);
        }
        void finishUnit()
        {
            switch (m_phase) {
            case Phase::session:
                readSession();
                break;
            case Phase::pair:
                readPair();
                break;
            case Phase::mesh:
                readMesh();
                break;
            case Phase::warning:
                detail::validateMeshWarning(reinterpret_cast<const uint8_t*>(mesh().warning.data()), mesh().warning.size());
                enter(Phase::vertex, meshVertexBytes);
                break;
            case Phase::vertex: {
                auto vertex = detail::readMeshVertex(m_fixed.data());
                detail::validateMeshVertex(vertex);
                mesh().vertices.push_back(vertex);
                enter(mesh().vertices.size() == m_shape.vertexCount ? Phase::index : Phase::vertex,
                    mesh().vertices.size() == m_shape.vertexCount ? 4 : meshVertexBytes);
                break;
            }
            case Phase::index: {
                const auto index = detail::readUnsigned(m_fixed.data(), 4);
                detail::validateIndexedMeshIndex(index, m_shape.vertexCount);
                auto& geometry = mesh();
                geometry.indices.push_back(static_cast<uint32_t>(index));
                const auto count = geometry.indices.size();
                if (count % 3 == 0)
                    detail::validateMeshTriangle({ geometry.vertices[geometry.indices[count - 3]], geometry.vertices[geometry.indices[count - 2]], geometry.vertices[geometry.indices[count - 1]] });
                if (count < m_shape.indexCount)
                    enter(Phase::index, 4);
                else if (m_kind == 1)
                    enter(Phase::source, sourceHintPrefixBytes);
                else
                    beginAuxiliary(1);
                break;
            }
            case Phase::source:
                readSource();
                break;
            case Phase::identity:
                readIdentity();
                break;
            case Phase::node:
                detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(m_node.data()), m_node.size(), sourceBudget(m_context));
                enter(Phase::part, m_partBytes);
                break;
            case Phase::part:
                detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(m_part.data()), m_part.size(), sourceBudget(m_context));
                m_textObserved = add(add(m_textObserved, m_node.size()), m_part.size());
                sources().identities.push_back({ std::move(m_node), std::move(m_part) });
                if (sources().identities.size() < m_shape.sources->identities)
                    enter(Phase::identity, 8);
                else
                    beginCorners();
                break;
            case Phase::corner: {
                if (detail::readUnsigned(m_fixed.data() + 4, 4))
                    throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Source corner reserved field is nonzero.");
                const auto corner = detail::readSourceHintCorner(m_fixed.data());
                detail::validateSourceHintCornerRange(corner, m_shape.sources->influences);
                sources().corners.push_back(corner);
                if (sources().corners.size() < m_shape.sources->corners)
                    enter(Phase::corner, sourceHintCornerBytes);
                else if (m_shape.sources->influences)
                    enter(Phase::influence, sourceHintInfluenceBytes);
                else
                    beginAuxiliary(1);
                break;
            }
            case Phase::influence: {
                if (detail::readUnsigned(m_fixed.data() + 4, 4) || detail::readUnsigned(m_fixed.data() + 20, 4))
                    throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidLayout, "Source influence reserved fields are nonzero.");
                const auto bits = detail::readUnsigned(m_fixed.data() + 16, 4);
                if ((bits & 0x7f800000U) == 0x7f800000U)
                    throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidWeight, "Source influence weight bits are nonfinite.");
                const auto influence = detail::readSourceHintInfluence(m_fixed.data());
                detail::validateSourceHintInfluence(influence, m_shape.sources->identities);
                sources().influences.push_back(influence);
                if (sources().influences.size() < m_shape.sources->influences)
                    enter(Phase::influence, sourceHintInfluenceBytes);
                else
                    beginAuxiliary(1);
                break;
            }
            case Phase::auxiliary:
                finishAuxiliary();
                break;
            case Phase::done:
                break;
            }
        }
        void consumeBody(const uint8_t* bytes, size_t size)
        {
            size_t offset = 0;
            while (offset < size || !m_remaining) {
                if (m_phase == Phase::done) {
                    if (offset < size)
                        rejectSession(SessionOutputErrorCode::trailingData, "Preview session has trailing semantic bytes.");
                    return;
                }
                if (!m_remaining) {
                    finishUnit();
                    continue;
                }
                const auto count = static_cast<size_t>(std::min<uint64_t>(size - offset, m_remaining));
                // This branch has both readable input and a positive phase remainder.
                const auto* span = bytes + offset;
                if (m_phase == Phase::warning)
                    mesh().warning.append(reinterpret_cast<const char*>(span), count);
                else if (m_phase == Phase::node)
                    m_node.append(reinterpret_cast<const char*>(span), count);
                else if (m_phase == Phase::part)
                    m_part.append(reinterpret_cast<const char*>(span), count);
                else if (m_phase == Phase::auxiliary)
                    m_auxiliary.insert(m_auxiliary.end(), span, span + count);
                else {
                    if (count > m_fixed.size() - m_fixedUsed)
                        rejectSession(SessionOutputErrorCode::invalidLayout, "Preview fixed record exceeds its workspace.");
                    std::memcpy(m_fixed.data() + m_fixedUsed, span, count);
                    m_fixedUsed += count;
                }
                m_remaining -= count;
                offset += count;
            }
        }
        void consume(const PreviewChunkHeader& header, const uint8_t* bytes, size_t size)
        {
            if (header.kind == PreviewChunkKind::begin)
                m_totalBytes = header.totalBytes;
            else if (header.kind == PreviewChunkKind::data)
                consumeBody(bytes, size);
            else if (m_phase != Phase::done)
                rejectSession(SessionOutputErrorCode::truncated, "Preview transfer ended inside a semantic section.");
        }
        const PreviewArchiveContext m_context;
        SessionOutput m_value;
        PreviewGeometryShape m_shape;
        Phase m_phase = Phase::session;
        uint32_t m_kind = 0;
        uint64_t m_remaining = sessionOutputPrefixBytes, m_totalBytes = 0, m_geometryBytes = 0, m_sourceBytes = 0;
        uint64_t m_textObserved = 0, m_nodeBytes = 0, m_partBytes = 0;
        std::array<uint64_t, 4> m_lengths {};
        std::array<uint8_t, sessionOutputPrefixBytes> m_fixed {};
        size_t m_fixedUsed = 0, m_auxiliaryIndex = 0;
        std::string m_node, m_part;
        std::vector<uint8_t> m_auxiliary;
        detail::PreviewStreamStatus m_status;
        bool m_taken = false;
        PreviewChunkReader m_reader;
    };
    PreviewSessionCollector::PreviewSessionCollector(PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy)
        : m_implementation(std::make_unique<Implementation>(identity, context, policy))
    {
    }
    PreviewSessionCollector::~PreviewSessionCollector() = default;
    void PreviewSessionCollector::append(const uint8_t* bytes, size_t size)
    {
        try {
            m_implementation->m_status.check();
            m_implementation->m_reader.append(bytes, size);
        } catch (...) {
            m_implementation->m_status.capture();
            throw;
        }
    }
    void PreviewSessionCollector::finishAtEOF()
    {
        try {
            m_implementation->m_status.check();
            m_implementation->m_reader.finishAtEOF();
        } catch (...) {
            m_implementation->m_status.capture();
            throw;
        }
    }
    SessionOutput PreviewSessionCollector::take()
    {
        static_assert(std::is_nothrow_move_constructible<SessionOutput>::value, "Complete ownership transfer must not lose a value on move.");
        m_implementation->m_status.check();
        if (!isComplete())
            throw PreviewChunkError(PreviewChunkErrorCode::invalidSequence, "Preview session ownership requires completed actual EOF and an unused value.");
        m_implementation->m_taken = true;
        return std::move(m_implementation->m_value);
    }
    bool PreviewSessionCollector::isComplete() const noexcept
    {
        return !isFailed() && !m_implementation->m_taken && m_implementation->m_reader.isFramed();
    }
    bool PreviewSessionCollector::isFailed() const noexcept { return m_implementation->m_status.isFailed(); }
}
}

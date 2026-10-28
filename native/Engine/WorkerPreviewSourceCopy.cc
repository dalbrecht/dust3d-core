// SPDX-License-Identifier: MIT
#include "WorkerIndexedSourceOperation.h"
#include "WorkerSourceHintValidation.h"
#include <limits>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void rejectSourceBudget()
        {
            throw PreviewRangeError(PreviewRangeErrorCode::nativeBudgetExceeded, "Preview source text exceeds its independent native budget.");
        }
        size_t identitySize(const char* text, uint64_t maximum)
        {
            if (!text)
                throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidInput, "Native preview identity storage is missing.");
            size_t size = 0;
            while (size <= maximum && text[size])
                ++size;
            if (size > maximum)
                rejectSourceBudget();
            detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(text), size, maximum);
            return size;
        }
        SourceHintCorner copyCorner(const Dust3DSourceCorner& value)
        {
            return { static_cast<SourceHintKind>(value.kind), value.first_influence, value.influence_count };
        }
        SourceHintInfluence copyInfluence(const Dust3DSourceInfluence& value)
        {
            static_assert(sizeof(size_t) <= sizeof(uint64_t), "Native identity indices must fit UInt64.");
            return { static_cast<SourceHintKind>(value.kind), value.source_index == SIZE_MAX ? noSourceIdentity : static_cast<uint64_t>(value.source_index), value.weight };
        }
    }
    IndexedSourcesPayload copyNativePreviewIndexedSourcedMesh(const Dust3DSourcedMesh* source, const Dust3DIndexedMesh* indexed, PreviewArchiveContext context)
    {
        context = detail::checkedPreviewArchiveContext(context);
        if (!source || !indexed)
            throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::invalidInput, "Native preview pair requires both stable owners.");
        const auto identityCount = dust3d_sourced_mesh_identity_count(source), cornerCount = dust3d_sourced_mesh_corner_count(source), influenceCount = dust3d_sourced_mesh_influence_count(source);
        const auto* corners = dust3d_sourced_mesh_corners(source);
        const auto* influences = dust3d_sourced_mesh_influences(source);
        if ((cornerCount && !corners) || (influenceCount && !influences))
            throw SourceHintPayloadError(SourceHintPayloadErrorCode::invalidInput, "Native preview source records require complete storage.");
        const auto nodes = dust3d_indexed_mesh_node_count(indexed), parts = dust3d_indexed_mesh_part_count(indexed);
        if (identityCount != nodes || cornerCount != dust3d_indexed_mesh_index_count(indexed))
            throw IndexedSourcesPayloadError(IndexedSourcesPayloadErrorCode::countMismatch, "Native preview source/indexed dimensions disagree.");
        if (nodes > context.archiveBytes || parts > context.archiveBytes)
            throw PreviewRangeError(PreviewRangeErrorCode::invalidCounts, "Preview membership exceeds captured archive bytes.");
        PreviewGeometryShape shape { dust3d_indexed_mesh_vertex_count(indexed), cornerCount, nodes, parts, 0,
            PreviewSourceShape { identityCount, cornerCount, influenceCount, 0 } };
        const PreviewNativeBudgets budgets { context.maximumIndexedBytes, context.maximumSourceBytes };
        const auto base = planPreviewRecords(shape, budgets);
        const auto sourceBudget = context.maximumSourceBytes ? context.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES;
        auto remaining = sourceBudget - base.nativeSourceBytes;
        for (size_t index = 0; index < identityCount; ++index) {
            for (const auto* text : { dust3d_sourced_mesh_node_id(source, index), dust3d_sourced_mesh_part_id(source, index) }) {
                const auto size = identitySize(text, remaining);
                remaining -= size;
                shape.sources->textBytes += size;
            }
        }
        (void)planPreviewRecords(shape, budgets);
        for (size_t index = 0; index < influenceCount; ++index)
            detail::validateSourceHintInfluence(copyInfluence(influences[index]), identityCount);
        for (size_t index = 0; index < cornerCount; ++index)
            detail::validateSourceHintCorner(copyCorner(corners[index]), influenceCount, [&](uint64_t offset) { return static_cast<SourceHintKind>(influences[static_cast<size_t>(offset)].kind); });
        IndexedSourcesPayload result;
        result.geometry = copyNativePreviewIndexedMesh(indexed, context);
        result.sources.identities.reserve(identityCount);
        result.sources.corners.reserve(cornerCount);
        result.sources.influences.reserve(influenceCount);
        for (size_t index = 0; index < identityCount; ++index)
            result.sources.identities.push_back({ dust3d_sourced_mesh_node_id(source, index), dust3d_sourced_mesh_part_id(source, index) });
        for (size_t index = 0; index < cornerCount; ++index)
            result.sources.corners.push_back(copyCorner(corners[index]));
        for (size_t index = 0; index < influenceCount; ++index)
            result.sources.influences.push_back(copyInfluence(influences[index]));
        return result;
    }
}
}

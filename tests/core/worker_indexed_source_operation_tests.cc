// SPDX-License-Identifier: MIT
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <native/Engine/WorkerIndexedSourceOperation.h>
#include <native/Engine/WorkerSourceCopy.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Licensed Capsule fixture required");
        std::ifstream input(argv[1], std::ios::binary);
        require(bool(input), "Ordinary fixture opens");
        std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        auto generated = generateIndexedSourcedMesh(archive, {});
        require(std::holds_alternative<IndexedSourcesPayload>(generated), "Known-positive indexed source generation succeeds");
        const auto pair = std::get<IndexedSourcesPayload>(std::move(generated));
        require(pair.geometry.indices.size() == 240 && pair.geometry.vertices.size() <= 240 && pair.sources.corners.size() == 240, "Native index/source corner order retained from one generation");
        const auto bytes = encodeIndexedSourcesPayload(pair, {});
        require(encodeIndexedSourcesPayload(decodeIndexedSourcesPayload(bytes.data(), bytes.size(), {}), {}) == bytes, "Native owners are absent from complete wire round trip");
        Dust3DError error {};
        Dust3DSourceOptions options { sizeof(options), 0 };
        std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> source(
            dust3d_sourced_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_sourced_mesh_destroy);
        require(source && error.status == DUST3D_STATUS_OK, "Known-positive source owner supports copy-only checks");
        std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(
            dust3d_mesh_create_indexed(dust3d_sourced_mesh_geometry(source.get()), 0, &error), dust3d_indexed_mesh_destroy);
        require(indexed && error.status == DUST3D_STATUS_OK, "Indexed owner derives from exactly the same source geometry");
        auto copied = copyNativeIndexedSourcedMesh(source.get(), indexed.get(), {});
        auto previewPair = copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), { archive.size(), 0, 0 });
        require(encodeIndexedSourcesPayload(previewPair, {}) == encodeIndexedSourcesPayload(copied, {}), "Native preview pair preserves exact canonical geometry and source records");
        uint64_t sourceTextBytes = 0;
        for (const auto& identity : copied.sources.identities)
            sourceTextBytes += identity.node.size() + identity.part.size();
        PreviewGeometryShape pairShape { copied.geometry.vertices.size(), copied.geometry.indices.size(), copied.geometry.nodeCount,
            copied.geometry.partCount, copied.geometry.warning.size(), PreviewSourceShape { copied.sources.identities.size(), copied.sources.corners.size(), copied.sources.influences.size(), sourceTextBytes } };
        const auto nativePairPlan = planPreviewRecords(pairShape, {});
        PreviewArchiveContext exactPairContext { archive.size(), nativePairPlan.nativeSourceBytes, nativePairPlan.nativeIndexedBytes };
        require(encodeIndexedSourcesPayload(copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), exactPairContext), {}) == encodeIndexedSourcesPayload(copied, {}), "Independent exact native source and indexed budgets accept");
        --exactPairContext.maximumSourceBytes;
        try {
            (void)copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), exactPairContext);
            throw std::runtime_error("Native preview source budget ignored");
        } catch (const PreviewRangeError& cause) {
            require(cause.code() == PreviewRangeErrorCode::nativeBudgetExceeded, "Source text retains its native budget charge");
        }
        require(copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), { archive.size(), 256ULL * 1024 * 1024, std::numeric_limits<size_t>::max() }).geometry.indices == copied.geometry.indices, "Maximum native policies use only the existing small actual pair");
        try {
            (void)copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), { 1, 0, 0 });
            throw std::runtime_error("Preview pair accepted inconsistent captured membership");
        } catch (const PreviewRangeError& cause) {
            require(cause.code() == PreviewRangeErrorCode::invalidCounts, "Pair metadata rejects independently of native record budgets");
        }
        try {
            (void)copyNativePreviewIndexedSourcedMesh(source.get(), indexed.get(), { archive.size(), 1, 0 });
            throw std::runtime_error("Preview pair ignored source record charges");
        } catch (const PreviewRangeError& cause) {
            require(cause.code() == PreviewRangeErrorCode::nativeBudgetExceeded, "Source record charges reject before scanning identities");
        }
        for (bool missingSource : { false, true }) {
            try {
                (void)copyNativePreviewIndexedSourcedMesh(missingSource ? nullptr : source.get(), missingSource ? indexed.get() : nullptr, { archive.size(), 0, 0 });
                throw std::runtime_error("Missing preview pair owner accepted");
            } catch (const IndexedSourcesPayloadError& cause) {
                require(cause.code() == IndexedSourcesPayloadErrorCode::invalidInput, "Missing stable pair owners reject before borrowed reads");
            }
        }
        PreviewArchiveContext previewContext { archive.size(), 0, 0 };
        auto preview = copyNativePreviewIndexedMesh(indexed.get(), previewContext);
        require(encodeIndexedMeshPayload(preview, {}) == encodeIndexedMeshPayload(copied.geometry, {}), "Native preview copy preserves canonical geometry without legacy copy limits");
        previewContext.maximumIndexedBytes = std::numeric_limits<size_t>::max();
        require(copyNativePreviewIndexedMesh(indexed.get(), previewContext).indices == preview.indices, "Explicit addressable native policy is preserved without a default ceiling");
        previewContext.maximumIndexedBytes = preview.vertices.size() * sizeof(Dust3DVertex) + preview.indices.size() * sizeof(uint32_t);
        require(copyNativePreviewIndexedMesh(indexed.get(), previewContext).warning == preview.warning, "Exact native record budget accepts independent warning storage");
        --previewContext.maximumIndexedBytes;
        try {
            (void)copyNativePreviewIndexedMesh(indexed.get(), previewContext);
            throw std::runtime_error("Native preview record budget ignored");
        } catch (const PreviewRangeError& cause) {
            require(cause.code() == PreviewRangeErrorCode::nativeBudgetExceeded, "Copy-only native budget rejection preserves its range cause");
        }
        previewContext = { archive.size(), 0, 0 };
        try {
            (void)copyNativePreviewIndexedMesh(nullptr, previewContext);
            throw std::runtime_error("Null native preview owner accepted");
        } catch (const IndexedMeshPayloadError& cause) {
            require(cause.code() == IndexedMeshPayloadErrorCode::invalidInput, "Null preview owner rejects before borrowed access");
        }
        try {
            (void)copyNativePreviewIndexedMesh(indexed.get(), { 1, 0, 0 });
            throw std::runtime_error("Inconsistent captured preview membership accepted");
        } catch (const PreviewRangeError& cause) {
            require(cause.code() == PreviewRangeErrorCode::invalidCounts, "Captured metadata cannot understate known Capsule membership");
        }
        try {
            (void)copyNativePreviewIndexedMesh(nullptr, { 0, 0, 0 });
            throw std::runtime_error("Invalid captured context accepted");
        } catch (const PreviewChunkError& cause) {
            require(cause.code() == PreviewChunkErrorCode::invalidLimits, "Captured policy rejects before any native owner access");
        }
        const auto meshWire = encodeIndexedMeshPayload(copied.geometry, {});
        const auto mesh = inspectIndexedMeshPayload(meshWire.data(), meshWire.size(), {});
        const auto hints = measureSourceHintPayload(copied.sources, {});
        IndexedSourcesPayloadLimits exact;
        exact.maximumWireBytes = indexedSourcesPrefixBytes + meshWire.size() + hints.wireBytes;
        exact.maximumDecodedBytes = mesh.logicalBytes + hints.decodedBytes;
        require(encodeIndexedSourcesPayload(copyNativeIndexedSourcedMesh(source.get(), indexed.get(), exact), exact).size() == exact.maximumWireBytes, "Exact aggregate copy quotas accept");
        const auto* actual = dust3d_indexed_mesh_indices(indexed.get());
        const auto* vertices = dust3d_indexed_mesh_vertices(indexed.get());
        for (size_t index = 0; index < copied.geometry.indices.size(); ++index)
            require(copied.geometry.indices.at(index) == actual[index], "Every original native index remains in order");
        for (size_t index = 0; index < copied.geometry.vertices.size(); ++index) {
            const auto& vertex = copied.geometry.vertices.at(index);
            require(!std::memcmp(vertex.position.data(), vertices[index].position, 3 * sizeof(float))
                    && !std::memcmp(vertex.normal.data(), vertices[index].normal, 3 * sizeof(float))
                    && !std::memcmp(vertex.color.data(), vertices[index].color, 4 * sizeof(float)),
                "Every native full-attribute vertex bit is retained");
        }
        const auto* corners = dust3d_sourced_mesh_corners(source.get());
        const auto* influences = dust3d_sourced_mesh_influences(source.get());
        for (size_t index = 0; index < copied.sources.corners.size(); ++index) {
            const auto& value = copied.sources.corners.at(index);
            require(static_cast<uint32_t>(value.kind) == static_cast<uint32_t>(corners[index].kind)
                    && value.firstInfluence == corners[index].first_influence && value.influenceCount == corners[index].influence_count,
                "Every original native source corner range remains in order");
        }
        for (size_t index = 0; index < copied.sources.influences.size(); ++index) {
            const auto& value = copied.sources.influences.at(index);
            require(static_cast<uint32_t>(value.kind) == static_cast<uint32_t>(influences[index].kind)
                    && value.identityIndex == (influences[index].source_index == SIZE_MAX ? noSourceIdentity : influences[index].source_index)
                    && value.weight == influences[index].weight,
                "All native influences retain their original kind, index and weight");
        }
        for (size_t index = 0; index < copied.sources.identities.size(); ++index)
            require(copied.sources.identities.at(index).node == dust3d_sourced_mesh_node_id(source.get(), index)
                    && copied.sources.identities.at(index).part == dust3d_sourced_mesh_part_id(source.get(), index),
                "Original native identity table is copied without normalization");
        for (int field = 0; field < 12; ++field) {
            auto limits = exact;
            if (field == 0)
                --limits.maximumWireBytes;
            if (field == 1)
                --limits.maximumDecodedBytes;
            if (field == 2)
                limits.geometry.maximumVertices = 1;
            if (field == 3)
                limits.sources.maximumCorners = 1;
            if (field == 4)
                limits.maximumDecodedBytes = 1;
            if (field == 5)
                limits.maximumWireBytes = 1;
            if (field == 6)
                limits.geometry.maximumIndices = 1;
            if (field == 7)
                limits.geometry.maximumWireBytes = 1;
            if (field == 8)
                limits.geometry.maximumDecodedBytes = 1;
            if (field == 9)
                limits.sources.maximumTextBytes = 1;
            if (field == 10)
                limits.sources.maximumDecodedBytes = 1;
            if (field == 11)
                limits.sources.maximumWireBytes = 1;
            bool rejected = false;
            try {
                copyNativeIndexedSourcedMesh(source.get(), indexed.get(), limits);
            } catch (const IndexedSourcesPayloadError& cause) {
                rejected = cause.code() == IndexedSourcesPayloadErrorCode::limitExceeded;
            } catch (const IndexedMeshPayloadError& cause) {
                rejected = cause.code() == IndexedMeshPayloadErrorCode::limitExceeded;
            } catch (const SourceHintPayloadError& cause) {
                rejected = cause.code() == SourceHintPayloadErrorCode::limitExceeded;
            }
            require(rejected, "Bounded copy-only quota rejection never regenerates native data");
        }
        try {
            copyNativeIndexedMesh(nullptr, {});
            throw std::runtime_error("Null indexed copy owner accepted");
        } catch (const IndexedMeshPayloadError& cause) {
            require(cause.code() == IndexedMeshPayloadErrorCode::invalidInput, "Null owner rejects before native indexing");
        }
        try {
            copyNativeIndexedSourcedMesh(source.get(), nullptr, {});
            throw std::runtime_error("Null indexed pair owner accepted");
        } catch (const IndexedMeshPayloadError& cause) {
            require(cause.code() == IndexedMeshPayloadErrorCode::invalidInput, "Null pair owner rejects without generation");
        }
        try {
            detail::inspectNativeSourceHints(nullptr, {});
            throw std::runtime_error("Null source inspection owner accepted");
        } catch (const SourceHintPayloadError& cause) {
            require(cause.code() == SourceHintPayloadErrorCode::invalidInput, "Null source preflight rejects before native getters");
        }
        indexed.reset();
        source.reset();
        archive.clear();
        require(encodeIndexedSourcesPayload(previewPair, {}) == encodeIndexedSourcesPayload(copied, {}), "Preview pair owns every identity and record after native release");
        require(encodeIndexedMeshPayload(preview, {}) == encodeIndexedMeshPayload(copied.geometry, {}), "Preview geometry remains owned after native owners and archive are released");
        require(!encodeIndexedSourcesPayload(copied, {}).empty(), "No borrowed source or indexed owner escapes copied pair");
        IndexedSourcesPayloadLimits invalid;
        invalid.maximumDecodedBytes = 0;
        try {
            generateIndexedSourcedMesh({}, invalid);
            throw std::runtime_error("Invalid policy reached native generation");
        } catch (const IndexedSourcesPayloadError& cause) {
            require(cause.code() == IndexedSourcesPayloadErrorCode::invalidLimits, "Pure policy rejection precedes generation");
        }
        std::cout << "Same-generation indexed/source copying passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

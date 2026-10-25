// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerSourceOperation.h>
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
        require(bool(input), "Fixture opens");
        std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        auto generated = generateSourcedMesh(archive, {});
        require(std::holds_alternative<SourcedMeshPayload>(generated), "Ordinary sourced generation succeeds");
        auto pair = std::get<SourcedMeshPayload>(std::move(generated));
        require(pair.geometry.vertices.size() == 240 && pair.sources.corners.size() == 240, "Actual corner order matches expanded geometry");
        require(pair.geometry.nodeCount == pair.sources.identities.size() && !pair.sources.identities.empty(), "Original identities accompany geometry");
        auto wire = encodeSourcedMeshPayload(pair, {});
        auto decoded = decodeSourcedMeshPayload(wire.data(), wire.size(), {});
        require(encodeSourcedMeshPayload(decoded, {}) == wire, "Complete output survives native owner release");

        Dust3DSourceOptions options { sizeof(options), 0 };
        Dust3DError error {};
        std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(
            dust3d_sourced_mesh_load(archive.data(), archive.size(), &options, &error), dust3d_sourced_mesh_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, "Known-positive owner for bounded copy tests");
        auto copied = copyNativeSourcedMesh(owner.get(), {});
        const auto sourceSize = measureSourceHintPayload(copied.sources, {});
        const auto meshWire = encodeMeshPayload(copied.geometry, {});
        const auto meshSize = inspectMeshPayload(meshWire.data(), meshWire.size(), {});
        SourcedMeshPayloadLimits exact;
        exact.maximumDecodedBytes = meshSize.logicalBytes + sourceSize.decodedBytes;
        exact.maximumWireBytes = sourcedMeshPrefixBytes + meshWire.size() + sourceSize.wireBytes;
        require(encodeSourcedMeshPayload(copyNativeSourcedMesh(owner.get(), exact), exact).size() == exact.maximumWireBytes, "Exact aggregate copy quotas accept");
        for (int field = 0; field < 11; ++field) {
            auto limits = exact;
            if (field == 0)
                --limits.maximumDecodedBytes;
            if (field == 1)
                --limits.maximumWireBytes;
            if (field == 2)
                limits.sources.maximumCorners = 1;
            if (field == 3)
                limits.sources.maximumTextBytes = 1;
            if (field == 4)
                limits.sources.maximumIdentityBytes = 1;
            if (field == 5)
                limits.sources.maximumDecodedBytes = 1;
            if (field == 6)
                limits.sources.maximumWireBytes = 1;
            if (field == 7)
                limits.geometry.maximumVertices = 1;
            if (field == 8)
                limits.maximumWireBytes = 1;
            if (field == 9)
                limits.maximumDecodedBytes = 1;
            if (field == 10)
                limits.sources.maximumInfluences = 1;
            bool rejected = false;
            try {
                (void)copyNativeSourcedMesh(owner.get(), limits);
            } catch (const SourceHintPayloadError& cause) {
                rejected = cause.code() == SourceHintPayloadErrorCode::limitExceeded;
            } catch (const MeshPayloadError& cause) {
                rejected = cause.code() == MeshPayloadErrorCode::limitExceeded;
            } catch (const SourcedMeshPayloadError& cause) {
                rejected = cause.code() == SourcedMeshPayloadErrorCode::limitExceeded;
            }
            require(rejected, "Small pure output-copy quota rejects without native regeneration");
        }
        const auto& firstIdentity = copied.sources.identities.at(0);
        require(firstIdentity.node == dust3d_sourced_mesh_node_id(owner.get(), 0)
                && firstIdentity.part == dust3d_sourced_mesh_part_id(owner.get(), 0),
            "Original opaque identity bytes are copied");
        const auto* influences = dust3d_sourced_mesh_influences(owner.get());
        for (size_t index = 0; index < copied.sources.influences.size(); ++index) {
            const auto& value = copied.sources.influences.at(index);
            const auto& original = influences[index];
            require(static_cast<uint32_t>(value.kind) == static_cast<uint32_t>(original.kind)
                    && value.weight == original.weight
                    && value.identityIndex == (original.source_index == SIZE_MAX ? noSourceIdentity : original.source_index),
                "Influence kinds, sentinels and weights retain native values");
        }
        const auto* corners = dust3d_sourced_mesh_corners(owner.get());
        for (size_t index = 0; index < copied.sources.corners.size(); ++index) {
            const auto& value = copied.sources.corners.at(index);
            const auto& original = corners[index];
            require(static_cast<uint32_t>(value.kind) == static_cast<uint32_t>(original.kind)
                    && value.firstInfluence == original.first_influence && value.influenceCount == original.influence_count,
                "Every native corner retains its influence range");
        }
        owner.reset();
        archive.clear();
        require(!encodeSourcedMeshPayload(copied, {}).empty(), "No borrowed storage escapes copied output");
        try {
            (void)copyNativeSourcedMesh(nullptr, {});
            throw std::runtime_error("Missing borrowed owner accepted");
        } catch (const SourceHintPayloadError& cause) {
            require(cause.code() == SourceHintPayloadErrorCode::invalidInput, "Missing owner rejects before access");
        }
        auto invalid = exact;
        invalid.maximumDecodedBytes = 0;
        try {
            (void)generateSourcedMesh({}, invalid);
            throw std::runtime_error("Invalid policy reached native generation");
        } catch (const SourcedMeshPayloadError& cause) {
            require(cause.code() == SourcedMeshPayloadErrorCode::invalidLimits, "Policy rejects before execution");
        }
        std::cout << "Owned native source generation passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

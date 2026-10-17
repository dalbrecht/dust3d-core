#include "GenerationPipeline.h"
#include "SourceProvenance.h"
#include <cmath>
#include <cstring>
#include <dust3d/base/object.h>
#include <dust3d/base/snapshot.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
const std::string node = "{00001000-0000-4000-8000-000000001000}";
const std::string other = "{00001001-0000-4000-8000-000000001001}";
const std::string part = "{00000100-0000-4000-8000-000000000100}";

void identitiesAndWeights()
{
    dust3d::Snapshot snapshot;
    snapshot.nodes[node]["partId"] = part;
    snapshot.nodes[other]["partId"] = part;
    dust3d::native::SourceProvenance source(snapshot, 1024 * 1024);
    snapshot.nodes.clear();
    dust3d::Object object;
    dust3d::Vector3 position(.25, .5, .75);
    object.positionToNodeIdMap.emplace(position, dust3d::Uuid(node));
    source.appendCorner(object, position);
    require(source.corners()[0].kind == DUST3D_SOURCE_NODE_HINT, "Original node must be a hint");
    const auto& influence = source.influences()[0];
    require(influence.kind == DUST3D_SOURCE_NODE_HINT && influence.weight == 1, "Single source needs unit weight");
    require(source.identities()[influence.source_index].node == node, "Original key must remain owned after snapshot destruction");
    require(source.identities()[influence.source_index].part == part, "Original part key must survive");
    object.positionToNodeWeights.emplace(position, std::vector<std::pair<dust3d::Uuid, float>> { { dust3d::Uuid(node), .25f }, { dust3d::Uuid(other), .75f } });
    source.appendCorner(object, position);
    require(source.corners()[1].kind == DUST3D_SOURCE_WEIGHTED_HINT && source.corners()[1].influence_count == 2, "Weights take precedence without choosing one node");
    require(source.influences()[1].weight == .25f && source.influences()[2].weight == .75f, "All weights preserved exactly");
    source.appendCorner(object, dust3d::Vector3(9, 9, 9));
    require(source.corners()[2].kind == DUST3D_SOURCE_UNKNOWN && source.corners()[2].influence_count == 0, "Generated geometry must stay unknown");
}

void aliasesAndGeneratedIdentities()
{
    dust3d::Snapshot snapshot;
    snapshot.nodes[node]["partId"] = part;
    snapshot.nodes[node.substr(1, 36)]["partId"] = "another original part";
    // Inherited Uuid accepts non-hex opaque characters; source export must not tighten legacy parsing.
    const std::string opaque = "{zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz}";
    snapshot.nodes[opaque]["partId"] = part;
    const std::string upper = "{AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA}";
    const std::string lower = "{aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa}";
    snapshot.nodes[upper]["partId"] = part;
    snapshot.nodes[lower]["partId"] = part;
    snapshot.nodes["invalid-internal-key"]["partId"] = part;
    dust3d::native::SourceProvenance source(snapshot, 1024 * 1024);
    dust3d::Object object;
    dust3d::Vector3 position(0, 0, 0);
    object.positionToNodeIdMap.emplace(position, dust3d::Uuid(node));
    source.appendCorner(object, position);
    require(source.corners()[0].kind == DUST3D_SOURCE_AMBIGUOUS, "Engine identity aliases must never choose an arbitrary document key");
    require(source.influences()[0].source_index == SIZE_MAX, "Ambiguity must not reference one alias");
    object.positionToNodeIdMap.clear();
    object.positionToNodeIdMap.emplace(position, dust3d::Uuid(other));
    source.appendCorner(object, position);
    require(source.corners()[1].kind == DUST3D_SOURCE_UNKNOWN, "Mirror/interpolated identities absent from original membership stay unknown");
    object.positionToNodeIdMap.clear();
    object.positionToNodeIdMap.emplace(position, dust3d::Uuid(opaque));
    source.appendCorner(object, position);
    require(source.corners()[2].kind == DUST3D_SOURCE_NODE_HINT, "Opaque legacy IDs remain usable");
    object.positionToNodeWeights[position] = { { dust3d::Uuid(node), .5f }, { dust3d::Uuid(other), .5f } };
    source.appendCorner(object, position);
    const auto corner = source.corners().back();
    require(corner.kind == DUST3D_SOURCE_WEIGHTED_HINT && corner.influence_count == 2, "Partial/ambiguous weighted hints retain all entries");
    require(source.influences()[corner.first_influence].kind == DUST3D_SOURCE_AMBIGUOUS && source.influences()[corner.first_influence + 1].kind == DUST3D_SOURCE_UNKNOWN, "Each unresolved weight needs explicit identity status");
    object.positionToNodeWeights.clear();
    object.positionToNodeIdMap[position] = dust3d::Uuid(upper);
    source.appendCorner(object, position);
    const auto upperIndex = source.influences().back().source_index;
    require(source.identities()[upperIndex].node == upper, "Uppercase legacy identity must retain original spelling");
    object.positionToNodeIdMap[position] = dust3d::Uuid(lower);
    source.appendCorner(object, position);
    const auto lowerIndex = source.influences().back().source_index;
    require(source.identities()[lowerIndex].node == lower && lowerIndex != upperIndex, "Inherited case-sensitive identities must not be normalized together");
}

void boundsAndErrors()
{
    dust3d::Snapshot snapshot;
    dust3d::native::SourceProvenance source(snapshot, 1024);
    dust3d::Object object;
    source.appendCorner(object, dust3d::Vector3(std::numeric_limits<double>::max(), 0, 0));
    source.appendCorner(object, dust3d::Vector3(std::numeric_limits<double>::quiet_NaN(), 0, 0));
    source.appendCorner(object, dust3d::Vector3(std::ldexp(1.0, std::numeric_limits<long>::digits) / 100000, 0, 0));
    require(source.corners().size() == 3 && source.corners()[0].kind == DUST3D_SOURCE_UNKNOWN, "Unrepresentable quantization must not perform undefined casts");
    bool rejected = false;
    try {
        dust3d::native::SourceProvenance tiny(snapshot, sizeof(Dust3DSourceCorner) - 1);
        tiny.appendCorner(object, dust3d::Vector3());
    } catch (const dust3d::native::DocumentError& error) {
        rejected = error.status() == DUST3D_STATUS_LIMIT_EXCEEDED;
    }
    require(rejected, "Provenance budget must bound output before growth");
    for (const auto weight : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f }) {
        object.positionToNodeWeights[dust3d::Vector3()] = { { dust3d::Uuid(node), weight } };
        rejected = false;
        try {
            source.appendCorner(object, dust3d::Vector3());
        } catch (const dust3d::native::DocumentError& error) {
            rejected = error.status() == DUST3D_STATUS_GENERATION_FAILED;
        }
        require(rejected, "Invalid inherited weights cannot cross the C ABI");
    }
    object.positionToNodeWeights[dust3d::Vector3()] = {};
    source.appendCorner(object, dust3d::Vector3());
    require(source.corners().back().kind == DUST3D_SOURCE_UNKNOWN, "Empty weight list must use the ordinary unknown fallback");
}

std::string archive(const std::string& xml)
{
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    return stream.str();
}

void generatedCorpus()
{
    // Independent MIT synthetic cases use the same archive structure as upstream.
    const auto model = [&](const std::string& partProperties, const std::string& groupProperties, const std::string& leafProperties) {
        return "<canvas originX='.5' originY='.5' originZ='.5'><nodes><node id='" + node + "' partId='" + part
            + "' x='.3' y='.5' z='.5' radius='.1'/><node id='" + other + "' partId='" + part
            + "' x='.7' y='.5' z='.5' radius='.1'/></nodes><edges><edge id='{00002000-0000-4000-8000-000000002000}' partId='" + part
            + "' from='" + node + "' to='" + other + "'/></edges><parts><part id='" + part + "' " + partProperties
            + "/></parts><components><component id='{00000300-0000-4000-8000-000000000300}' " + groupProperties
            + "><component id='{00000200-0000-4000-8000-000000000200}' " + leafProperties + " linkDataType='partId' linkData='" + part
            + "'/></component></components></canvas>";
    };
    for (const auto* scenario : { "wrap", "mirror", "interpolation" }) {
        const std::string name(scenario);
        const auto bytes = archive(model(name == "mirror" ? "xMirrored='true'" : "", name == "wrap" ? "wrap='Cloth' wrapFaces='64'" : "", name == "mirror" ? "combineMode='Uncombined'" : ""));
        Dust3DError error {};
        Dust3DSourceOptions options { sizeof(options), 0 };
        std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(dust3d_sourced_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
        if (!owner)
            throw std::runtime_error(name + ": " + error.message);
        const auto count = dust3d_sourced_mesh_corner_count(owner.get());
        require(count == dust3d_mesh_vertex_count(dust3d_sourced_mesh_geometry(owner.get())) && count > 0, "Generated corners align with retained triangles");
        size_t weighted = 0;
        size_t unknown = 0;
        size_t multipleWeights = 0;
        const auto* corners = dust3d_sourced_mesh_corners(owner.get());
        for (size_t index = 0; index < count; ++index) {
            weighted += corners[index].kind == DUST3D_SOURCE_WEIGHTED_HINT;
            unknown += corners[index].kind == DUST3D_SOURCE_UNKNOWN;
            multipleWeights += corners[index].influence_count > 1;
        }
        if (name == "wrap")
            require(weighted > 0 && multipleWeights > 0, "Actual wrapping must export all multi-node weighted associations");
        else
            require(unknown > 0, "Actual mirror/interpolation synthetic sources must not impersonate original nodes");
    }
}

void filteredPrecision()
{
    const std::string secondPart = "{00000101-0000-4000-8000-000000000101}";
    const auto xml = "<canvas originX='0' originY='0' originZ='0'><nodes><node id='" + node + "' partId='" + part
        + "' x='0' y='0' z='0' radius='.001'/><node id='" + other + "' partId='" + secondPart
        + "' x='1000000' y='1000000' z='1000000' radius='.001'/></nodes><edges/><parts><part id='" + part
        + "'/><part id='" + secondPart + "'/></parts><components><component id='{00000200-0000-4000-8000-000000000200}' combineMode='Uncombined' linkDataType='partId' linkData='" + part
        + "'/><component id='{00000201-0000-4000-8000-000000000201}' combineMode='Uncombined' linkDataType='partId' linkData='" + secondPart + "'/></components></canvas>";
    const auto bytes = archive(xml);
    std::string warning;
    auto snapshot = dust3d::native::readDocument(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), warning);
    dust3d::MeshGenerator generator(snapshot.release());
    generator.generate();
    std::unique_ptr<dust3d::Object> original(generator.takeObject());
    require(generator.isSuccessful() && original, "Independent double geometry must generate");
    Dust3DError error {};
    Dust3DSourceOptions options { sizeof(options), 0 };
    std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(dust3d_sourced_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
    require(owner != nullptr, "Mixed representable/collapsed source fixture must generate");
    const auto count = dust3d_sourced_mesh_corner_count(owner.get());
    const auto* geometry = dust3d_sourced_mesh_geometry(owner.get());
    require(count == dust3d_mesh_vertex_count(geometry) && count > 0 && count < original->triangles.size() * 3, "Actual float-filtered triangles must omit their source corners too");
    const auto* corners = dust3d_sourced_mesh_corners(owner.get());
    const auto* influences = dust3d_sourced_mesh_influences(owner.get());
    const auto* vertices = dust3d_mesh_vertices(geometry);
    for (size_t index = 0; index < count; ++index) {
        require(std::abs(vertices[index].position[0]) < 1 && std::abs(vertices[index].position[1]) < 1 && std::abs(vertices[index].position[2]) < 1, "Only near geometry remains representable");
        require(corners[index].kind == DUST3D_SOURCE_NODE_HINT && corners[index].influence_count == 1, "Retained corner must preserve its near-node hint");
        const auto identity = influences[corners[index].first_influence].source_index;
        require(node == dust3d_sourced_mesh_node_id(owner.get(), identity), "Filtered far-node hints cannot shift onto retained corners");
    }
}

void concurrentLoads(const std::vector<uint8_t>& bytes)
{
    std::vector<std::future<size_t>> jobs;
    for (int mode = 0; mode < 3; ++mode) {
        jobs.push_back(std::async(std::launch::async, [&, mode] {
            Dust3DError error {};
            if (mode == 0) {
                Dust3DSourceOptions options { sizeof(options), 0 };
                std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(dust3d_sourced_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_sourced_mesh_destroy);
                require(owner != nullptr, "Concurrent source load must succeed");
                return dust3d_sourced_mesh_corner_count(owner.get());
            }
            std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(dust3d_generation_session_create(0, &error), dust3d_generation_session_destroy);
            std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> mesh(mode == 1 ? dust3d_mesh_load(bytes.data(), bytes.size(), &error) : dust3d_generation_session_load(session.get(), bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
            require(mesh != nullptr, "Concurrent legacy/session load must succeed");
            return dust3d_mesh_vertex_count(mesh.get());
        }));
    }
    const auto expected = jobs[0].get();
    require(expected > 0 && jobs[1].get() == expected && jobs[2].get() == expected, "All generation APIs share serialization and preserve output counts");
}

void malformedGeometry()
{
    for (const auto* value : { "abc", "1e9999" }) {
        const auto bytes = archive("<canvas originX='.5' originY='.5' originZ='.5'><nodes><node id='" + node + "' partId='" + part
            + "' x='" + value + "' y='.5' z='.5' radius='.1'/></nodes><edges/><parts><part id='" + part
            + "'/></parts><components><component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='" + part + "'/></components></canvas>");
        Dust3DError legacyError {};
        std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> legacy(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &legacyError), dust3d_mesh_destroy);
        require(!legacy && legacyError.status == DUST3D_STATUS_INVALID_DOCUMENT, "Legacy loader must classify malformed/overflowing numbers as invalid input");
        Dust3DError sourceError {};
        Dust3DSourceOptions options { sizeof(options), 0 };
        std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)> owner(dust3d_sourced_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &sourceError), dust3d_sourced_mesh_destroy);
        require(!owner && sourceError.status == legacyError.status && sourceError.message[0] != 0, "Source loader must preserve malformed/overflowing numeric input status and cause");
    }
}

void publicContract(const std::string& fixtures)
{
    Dust3DError error {};
    Dust3DSourceOptions options { sizeof(Dust3DSourceOptions), 0 };
    require(!dust3d_sourced_mesh_load(nullptr, 0, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Missing options must reject");
    --options.struct_size;
    require(!dust3d_sourced_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Short options must reject");
    options.struct_size = sizeof(options);
    options.maximum_payload_bytes = SIZE_MAX;
    require(!dust3d_sourced_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Oversized budgets reject before input access");
    options.maximum_payload_bytes = 0;
    require(!dust3d_sourced_mesh_load(nullptr, 0, &options, nullptr), "Omitted diagnostics must be safe");
    require(!dust3d_sourced_mesh_geometry(nullptr) && !dust3d_sourced_mesh_corners(nullptr) && !dust3d_sourced_mesh_influences(nullptr), "Null owners have no borrowed buffers");
    require(dust3d_sourced_mesh_corner_count(nullptr) == 0 && dust3d_sourced_mesh_influence_count(nullptr) == 0 && dust3d_sourced_mesh_identity_count(nullptr) == 0 && dust3d_sourced_mesh_payload_bytes(nullptr) == 0, "Null owners have zero counts");
    require(dust3d_sourced_mesh_node_id(nullptr, 0)[0] == 0 && dust3d_sourced_mesh_part_id(nullptr, 0)[0] == 0, "Null identities are empty");
    dust3d_sourced_mesh_destroy(nullptr);

    for (const auto* name : { "Capsule.ds3", "Sword.ds3", "RoundedCrossingDiff.ds3" }) {
        std::ifstream stream(fixtures + "/" + name, std::ios::binary);
        require(stream.good(), "Fixture must exist");
        const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
        using Owner = std::unique_ptr<Dust3DSourcedMesh, decltype(&dust3d_sourced_mesh_destroy)>;
        auto load = [&](size_t budget) {
            Dust3DSourceOptions request { sizeof(Dust3DSourceOptions), budget };
            return Owner(dust3d_sourced_mesh_load(bytes.data(), bytes.size(), &request, &error), dust3d_sourced_mesh_destroy);
        };
        if (std::string(name) == "Capsule.ds3")
            concurrentLoads(bytes);
        auto owner = load(0);
        require(owner && error.status == DUST3D_STATUS_OK, "Actual fixture source generation must succeed");
        const auto* geometry = dust3d_sourced_mesh_geometry(owner.get());
        const auto count = dust3d_mesh_vertex_count(geometry);
        const auto identities = dust3d_sourced_mesh_identity_count(owner.get());
        require(identities == dust3d_mesh_node_count(geometry), "All original node keys must be owned");
        require(count > 0 && dust3d_sourced_mesh_corner_count(owner.get()) == count, "Exactly one source record per retained expanded corner");
        const auto* corners = dust3d_sourced_mesh_corners(owner.get());
        const auto* influences = dust3d_sourced_mesh_influences(owner.get());
        const auto influenceCount = dust3d_sourced_mesh_influence_count(owner.get());
        size_t hints = 0;
        for (size_t index = 0; index < count; ++index) {
            const auto& corner = corners[index];
            require(corner.first_influence <= influenceCount && corner.influence_count <= influenceCount - corner.first_influence, "Corner influence range must fit without overflow");
            for (size_t offset = 0; offset < corner.influence_count; ++offset) {
                const auto& influence = influences[corner.first_influence + offset];
                require(std::isfinite(influence.weight) && influence.weight >= 0, "All weights must be finite and nonnegative");
                if (influence.kind == DUST3D_SOURCE_NODE_HINT) {
                    require(influence.source_index < identities && dust3d_sourced_mesh_node_id(owner.get(), influence.source_index)[0] != 0, "Hints reference original owned keys");
                    ++hints;
                } else {
                    require(influence.source_index == SIZE_MAX, "Unknown associations cannot invent IDs");
                }
            }
        }
        require(hints > 0, "Fixture must exercise actual engine node hints");
        require(dust3d_sourced_mesh_node_id(owner.get(), SIZE_MAX)[0] == 0 && dust3d_sourced_mesh_part_id(owner.get(), identities)[0] == 0, "Out-of-range identity lookups are safe");
        const auto payload = dust3d_sourced_mesh_payload_bytes(owner.get());
        require(payload > 0 && payload <= DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES, "Exported payload must fit its declared budget");
        auto exact = load(payload);
        require(exact != nullptr, "Exact logical source budget must fit");
        exact.reset();
        require(!load(payload - 1) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && error.message[0] != 0, "One-byte-short budget rejects descriptively");
        std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> legacy(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
        require(legacy && dust3d_mesh_vertex_count(legacy.get()) == count && std::memcmp(dust3d_mesh_vertices(legacy.get()), dust3d_mesh_vertices(geometry), count * sizeof(Dust3DVertex)) == 0, "Opt-in metadata must not change legacy geometry");
        std::unique_ptr<Dust3DIndexedMesh, decltype(&dust3d_indexed_mesh_destroy)> indexed(dust3d_mesh_create_indexed(geometry, 0, &error), dust3d_indexed_mesh_destroy);
        require(indexed && dust3d_indexed_mesh_index_count(indexed.get()) == count, "Source corners remain aligned with indexed triangle indices");
        const std::vector<Dust3DSourceCorner> copied(corners, corners + count);
        const auto savedKind = corners[0].kind;
        owner.reset();
        auto unrelated = load(0);
        require(unrelated && copied[0].kind == savedKind && dust3d_indexed_mesh_index_count(indexed.get()) == count, "Copies and independent indexed geometry survive source destruction and later generation");
    }
}
}
int main(int argc, char** argv)
{
    try {
        identitiesAndWeights();
        aliasesAndGeneratedIdentities();
        boundsAndErrors();
        require(argc == 2, "Expected fixture directory");
        publicContract(argv[1]);
        generatedCorpus();
        filteredPrecision();
        malformedGeometry();
        std::cout << "Source provenance contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

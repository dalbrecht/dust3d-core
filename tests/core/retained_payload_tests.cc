#include "EngineInternal.h"
#include <atomic>
#include <cstdlib>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/mesh/solid_mesh.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
std::atomic<bool> isObservingAllocations { false };
std::atomic<size_t> observedAllocations { 0 };
}

// Standard-container allocations use ordinary C++ new. Observe only inspection,
// after fixtures and real geometry have already been constructed.
void* operator new(std::size_t bytes)
{
    if (void* result = std::malloc(bytes ? bytes : 1)) {
        if (isObservingAllocations.load(std::memory_order_relaxed))
            observedAllocations.fetch_add(1, std::memory_order_relaxed);
        return result;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
class ObserveAllocations {
public:
    ObserveAllocations()
    {
        observedAllocations.store(0, std::memory_order_relaxed);
        isObservingAllocations.store(true, std::memory_order_relaxed);
    }
    ~ObserveAllocations() { isObservingAllocations.store(false, std::memory_order_relaxed); }
    ObserveAllocations(const ObserveAllocations&) = delete;
    ObserveAllocations& operator=(const ObserveAllocations&) = delete;
};
}

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void checkedArithmetic()
{
    dust3d::RetainedPayloadBudget zero(0);
    require(zero.tryAdd(0) && zero.usedBytes() == 0 && !zero.isExceeded(), "Empty payload must fit a zero budget");
    require(!zero.tryAdd(1) && zero.isExceeded() && zero.usedBytes() == 0, "Nonempty payload cannot fit zero");
    dust3d::RetainedPayloadBudget exact(16);
    require(exact.tryAdd(4, 4) && exact.usedBytes() == 16, "An exact byte count must fit");
    require(!exact.tryAdd(1) && exact.usedBytes() == 16, "Rejected additions cannot wrap or change committed usage");
    require(!exact.tryAdd(0), "Exceeded budgets remain failed even if a later member is empty");
    const auto maximum = std::numeric_limits<size_t>::max();
    dust3d::RetainedPayloadBudget overflow(maximum);
    require(!overflow.tryAdd(maximum, 2) && overflow.isExceeded() && overflow.usedBytes() == 0, "Multiplication overflow must reject before arithmetic");
    dust3d::RetainedPayloadBudget full(maximum);
    require(full.tryAdd(maximum) && !full.tryAdd(1), "Addition cannot overflow the largest budget");
    dust3d::RetainedPayloadBudget emptyElements(0);
    require(emptyElements.tryAdd(maximum, 0) && emptyElements.usedBytes() == 0, "Zero-byte elements cannot divide by zero or overflow");
}

void ownedCapacitiesAndMetadata()
{
    std::vector<uint32_t> indices { 0, 1, 2 };
    indices.reserve(128);
    dust3d::RetainedPayloadBudget budget(std::numeric_limits<size_t>::max());
    budget.accountObject(indices);
    require(budget.usedBytes() == sizeof(indices) + indices.capacity() * sizeof(uint32_t), "Retained vector capacity, not just size, must count");
    Dust3DMesh mesh;
    mesh.vertices.reserve(256);
    mesh.warning = std::string(200, 'w');
    dust3d::RetainedPayloadBudget copied(std::numeric_limits<size_t>::max());
    copied.accountObject(mesh);
    require(copied.usedBytes() >= sizeof(mesh) + mesh.vertices.capacity() * sizeof(Dust3DVertex) + mesh.warning.capacity(), "Cached native output must account for vertex and warning storage");
    dust3d::Snapshot source;
    source.nodes[std::string(80, 'n')]["partId"] = std::string(80, 'p');
    dust3d::RetainedPayloadBudget metadata(std::numeric_limits<size_t>::max());
    metadata.accountObject(source);
    require(metadata.usedBytes() > sizeof(source) + 160, "Snapshot keys and nested attribute values must count");
}

void insufficientRetainedBuffersAndMetadata()
{
    std::vector<uint32_t> cleared;
    cleared.reserve(128);
    dust3d::RetainedPayloadBudget vectorBudget(sizeof(cleared));
    {
        ObserveAllocations observation;
        vectorBudget.accountObject(cleared);
    }
    require(observedAllocations.load() == 0 && vectorBudget.isExceeded(), "Retained unused capacity must reject a header-only budget without allocating");
    require(vectorBudget.usedBytes() == sizeof(cleared) && cleared.empty() && cleared.capacity() >= 128, "Rejected inspection cannot mutate the retained source or charge rejected bytes");
    const std::map<std::string, std::string> metadata { { "owner", "part" } };
    dust3d::RetainedPayloadBudget mapBudget(sizeof(metadata));
    {
        ObserveAllocations observation;
        mapBudget.accountObject(metadata);
    }
    require(observedAllocations.load() == 0 && mapBudget.isExceeded(), "Map entries cannot fit a header-only budget or allocate while rejecting");
    require(mapBudget.usedBytes() == sizeof(metadata) && metadata.size() == 1, "Rejected metadata must preserve its source and committed usage");
    const std::vector<std::vector<size_t>> topology { { 0, 1, 2 } };
    dust3d::RetainedPayloadBudget rowHeaders(sizeof(topology));
    rowHeaders.accountObject(topology);
    require(rowHeaders.isExceeded() && rowHeaders.usedBytes() == sizeof(topology), "Nested face headers must be charged before vertex indices");
    const auto rowStorage = sizeof(topology) + topology.capacity() * sizeof(std::vector<size_t>);
    dust3d::RetainedPayloadBudget indicesMissing(rowStorage);
    {
        ObserveAllocations observation;
        indicesMissing.accountObject(topology);
    }
    require(observedAllocations.load() == 0 && indicesMissing.isExceeded(), "A budget for face headers alone cannot hide owned nested indices");
    require(indicesMissing.usedBytes() == rowStorage && topology[0].size() == 3, "Rejected nested indices preserve data and committed ownership charges");
}

void borrowedGeometryIsNotOwnedTwice()
{
    std::vector<dust3d::Vector3> vertices { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    std::vector<std::vector<size_t>> triangles { { 0, 1, 2 } };
    dust3d::SolidMesh mesh;
    mesh.setVertices(&vertices);
    mesh.setTriangles(&triangles);
    mesh.prepare();
    dust3d::RetainedPayloadBudget first(std::numeric_limits<size_t>::max());
    first.accountObject(mesh);
    require(first.usedBytes() > sizeof(mesh), "Owned normals and spatial-index storage must count");
    const auto* tree = mesh.axisAlignedBoundingBoxTree();
    require(tree != nullptr, "Prepared geometry must own a spatial index");
    dust3d::RetainedPayloadBudget smallTree(sizeof(*tree));
    smallTree.accountObject(*tree);
    require(smallTree.isExceeded(), "Spatial-index buffers and nodes cannot fit just their owner header");
    vertices.reserve(4096);
    triangles.reserve(4096);
    dust3d::RetainedPayloadBudget second(std::numeric_limits<size_t>::max());
    second.accountObject(mesh);
    require(first.usedBytes() == second.usedBytes(), "Borrowed source vector capacities cannot be charged to SolidMesh");
}

void nestedWrappingOwnership()
{
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    dust3d::RetainedPayloadBudget empty(std::numeric_limits<size_t>::max());
    empty.accountObject(cache);
    cache.wrapCache.entries.emplace_back();
    auto& wrap = cache.wrapCache.entries.back();
    wrap.vertices.reserve(512);
    wrap.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
    wrap.faces = { { 0, 1, 2, 3 } };
    wrap.vertexSourceVertex = { 0, 1, 2, 3 };
    wrap.resultVertices = wrap.vertices;
    wrap.resultTriangles = { { 0, 1, 2 }, { 0, 2, 3 } };
    wrap.resultTriangleAndQuads = wrap.faces;
    wrap.resultQuadDiagonals = { { 0, 2 } };
    wrap.resultTriangleUvs = { { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(1, 1) },
        { dust3d::Vector2(0, 0), dust3d::Vector2(1, 1), dust3d::Vector2(0, 1) } };
    wrap.resultVertexNodeWeights.resize(4);
    wrap.resultVertexNodeWeights[0].push_back({ dust3d::Uuid("{11111111-1111-4111-8111-111111111111}"), 1.0f });
    const std::vector<dust3d::Vector3> vertices { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    const std::vector<std::vector<size_t>> faces { { 0, 1, 2 } };
    const std::array<dust3d::PositionKey, 3> points { vertices[0], vertices[1], vertices[2] };
    const std::array<dust3d::Vector2, 3> uvs { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    const dust3d::Uuid node("{11111111-1111-4111-8111-111111111111}");
    auto& component = cache.components["cloth"];
    component.wrapOutput = std::make_unique<dust3d::MeshGenerator::GeneratedComponent>();
    component.wrapOutput->mesh = std::make_unique<dust3d::MeshState>(vertices, faces);
    component.wrapOutput->positionToNodeWeights[points[0]] = { { node, 1.0f } };
    component.wrapOutput->positionToVertexAttribute[points[0]] = {};
    component.componentTriangleUvs[node][points] = uvs;
    component.sharedQuadEdges.insert({ points[0], points[1] });
    component.brokenTriangles.insert(points);
    component.noneSeamVertices.insert(points[0]);
    component.positionToNodeIdMap[points[0]] = node;
    component.nodeMap[node] = {};
    component.wrapFolds = std::string(256, 'f');
    auto& part = cache.parts["body"];
    part.vertices = vertices;
    part.faces = faces;
    part.triangleUvs[points] = uvs;
    part.importedVertexColors.push_back(dust3d::Color(0.2, 0.4, 0.8));
    part.importedTriangleNormals[points] = { dust3d::Vector3(0, 0, 1), dust3d::Vector3(0, 0, 1), dust3d::Vector3(0, 0, 1) };
    auto& combination = cache.cachedCombination["union"];
    combination = std::make_unique<dust3d::MeshState>(vertices, faces);
    combination->triangleComponentKeys.push_back(node.toString());
    combination->triangleVertexColors.push_back(std::array<dust3d::Color, 3> {
        dust3d::Color(1, 0, 0), dust3d::Color(0, 1, 0), dust3d::Color(0, 0, 1) });
    combination->brokenTriangles.reserve(64);
    combination->brokenTriangles.push_back(points);
    combination->seamReports.reserve(128);
    combination->seamTriangleUvs.resize(1);
    combination->seamTriangleUvs[0].first.insert(points);
    cache.cachedCombination["failed"] = nullptr;
    cache.partMirrorIdMap[std::string(38, 'm')] = std::string(38, 'p');
    dust3d::RetainedPayloadBudget populated(std::numeric_limits<size_t>::max());
    {
        ObserveAllocations observation;
        populated.accountObject(cache);
    }
    require(observedAllocations.load() == 0, "Nested UV, source, wrapping and imported metadata inspection must not allocate");
    require(populated.usedBytes() >= empty.usedBytes() + 512 * sizeof(dust3d::Vector3), "Wrapping buffers and node-weight ownership must count");
    const auto beforeOwnerReservation = populated.usedBytes();
    combination->triangleComponentKeys.reserve(1000);
    dust3d::RetainedPayloadBudget expandedOwners(std::numeric_limits<size_t>::max());
    expandedOwners.accountObject(cache);
    require(expandedOwners.usedBytes() >= beforeOwnerReservation + 999 * sizeof(std::string),
        "Cached triangle owner capacity must count toward retention");
    combination->triangleVertexColors.reserve(1000);
    dust3d::RetainedPayloadBudget expandedColors(std::numeric_limits<size_t>::max());
    expandedColors.accountObject(cache);
    require(expandedColors.usedBytes() >= expandedOwners.usedBytes()
                + 999 * sizeof(std::optional<std::array<dust3d::Color, 3>>),
        "Cached corner-color capacity must count toward retention");
    const auto beforeWeightReservation = expandedColors.usedBytes();
    wrap.resultVertexNodeWeights[0].reserve(1000);
    dust3d::RetainedPayloadBudget expandedWeights(std::numeric_limits<size_t>::max());
    expandedWeights.accountObject(cache);
    require(!wrap.hasWeights && expandedWeights.usedBytes() > beforeWeightReservation, "Inactive weight flags cannot hide retained weight-buffer capacity");
    populated = expandedWeights;
    dust3d::RetainedPayloadBudget exact(populated.usedBytes());
    exact.accountObject(cache);
    require(!exact.isExceeded() && exact.usedBytes() == populated.usedBytes(), "An exact nested cache payload budget must fit");
    dust3d::RetainedPayloadBudget insufficient(populated.usedBytes() - 1);
    insufficient.accountObject(cache);
    require(insufficient.isExceeded(), "One byte below a real nested footprint must reject");
    cache.wrapCache.entries.clear();
    cache.parts.clear();
    cache.components.clear();
    cache.cachedCombination.clear();
    cache.partMirrorIdMap.clear();
    dust3d::RetainedPayloadBudget cleared(std::numeric_limits<size_t>::max());
    cleared.accountObject(cache);
    require(cleared.usedBytes() == empty.usedBytes(), "Destroyed wrapping entries cannot remain charged");
}

void generatedOwnership(const std::string& directory, const std::string& fixture)
{
    std::ifstream file(directory + "/" + fixture + ".ds3", std::ios::binary);
    require(file.good(), "Native fixture must be readable");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::string warning;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning);
    dust3d::MeshGenerator::GeneratedCacheContext cache;
    {
        dust3d::MeshGenerator generator(snapshot.release());
        generator.setGeneratedCacheContext(&cache);
        generator.generate();
        require(generator.isSuccessful(), "Real native fixture generation must succeed");
    }
    require(!cache.parts.empty() && !cache.components.empty(), "Generator must leave real owned part and component caches");
    if (fixture == "Sword")
        require(!cache.cachedCombination.empty(), "Sword must exercise owned Boolean-combination cache entries");
    dust3d::RetainedPayloadBudget payload(std::numeric_limits<size_t>::max());
    {
        ObserveAllocations observation;
        payload.accountObject(cache);
    }
    require(observedAllocations.load() == 0, "Inspecting real cache ownership must not allocate");
    std::cout << fixture << " accounted cache payload: " << payload.usedBytes() << " bytes\n";
    require(payload.usedBytes() > sizeof(cache), "Real geometry caches must remain inspectable after generator destruction");
    dust3d::RetainedPayloadBudget exact(payload.usedBytes());
    exact.accountObject(cache);
    require(!exact.isExceeded(), "A real cache's measured budget must fit without mutation");
    dust3d::RetainedPayloadBudget tooSmall(payload.usedBytes() - 1);
    tooSmall.accountObject(cache);
    require(tooSmall.isExceeded(), "Real generated geometry must not bypass the cache payload budget");
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> owner(
        dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(owner && error.status == DUST3D_STATUS_OK, "The C ABI must provide an owned fixture mesh");
    dust3d::RetainedPayloadBudget cOwner(std::numeric_limits<size_t>::max());
    {
        ObserveAllocations observation;
        cOwner.accountObject(owner);
    }
    require(observedAllocations.load() == 0 && !cOwner.isExceeded(), "C ABI function-pointer deleters are borrowed code, not allocated payload");
}
void copiedGeometryAndIndexedOutput()
{
    const std::vector<dust3d::Vector3> vertices { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    const std::vector<std::vector<size_t>> faces { { 0, 1, 2 } };
    std::unique_ptr<dust3d::MeshCombiner::Mesh> copy;
    {
        dust3d::MeshCombiner::Mesh source(vertices, faces);
        copy = std::make_unique<dust3d::MeshCombiner::Mesh>(source);
    }
    dust3d::RetainedPayloadBudget copied(std::numeric_limits<size_t>::max());
    copied.accountObject(*copy);
    require(copied.usedBytes() > sizeof(*copy), "Copied geometry must own inspectable buffers after source destruction");
    std::vector<dust3d::Vector3> fetchedVertices;
    std::vector<std::vector<size_t>> fetchedFaces;
    copy->fetch(fetchedVertices, fetchedFaces);
    require(fetchedVertices.size() == vertices.size() && fetchedFaces == faces, "Inspection cannot mutate the copied mesh topology");
    Dust3DIndexedMesh indexed;
    indexed.vertices.reserve(64);
    indexed.indices.reserve(512);
    dust3d::RetainedPayloadBudget native(std::numeric_limits<size_t>::max());
    native.accountObject(indexed);
    require(native.usedBytes() >= sizeof(indexed) + indexed.vertices.capacity() * sizeof(Dust3DVertex) + indexed.indices.capacity() * sizeof(uint32_t), "Native indexed caches must count both owned buffers");
}

}

int main(int argc, char** argv)
{
    try {
        require(argc == 2, "A fixture directory is required");
        checkedArithmetic();
        ownedCapacitiesAndMetadata();
        insufficientRetainedBuffersAndMetadata();
        borrowedGeometryIsNotOwnedTwice();
        nestedWrappingOwnership();
        generatedOwnership(argv[1], "Capsule");
        generatedOwnership(argv[1], "Sword");
        copiedGeometryAndIndexedOutput();
        std::cout << "Retained payload contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

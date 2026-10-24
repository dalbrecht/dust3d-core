// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxAtlasGeometry.h"
#include "FbxGeometrySelection.h"
#include "FbxMeshScene.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>
using namespace dust3d;
using namespace dust3d::native;
static_assert(!std::is_copy_constructible_v<FbxGeometrySelection> && std::is_nothrow_move_constructible_v<FbxGeometrySelection>);
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "FBX selection lost typed rejection");
        return;
    }
    throw std::runtime_error("Invalid FBX selection accepted");
}
Object fixture()
{
    Object source;
    source.vertices = { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 3, 0 }, { 0, 0, 4 } };
    source.triangles = { { 0, 1, 2 }, { 0, 3, 1 } };
    source.setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, { { 0, 1, 0 }, { 0, 1, 0 }, { 0, 1, 0 } } });
    source.setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } }, { { 0.25, 0.25 }, { 0.5, 0.5 }, { 0.75, 0.75 } } });
    return source;
}
std::vector<FbxTriangleUvs> overrides()
{
    return { { Vector2(0.125, 0.25), Vector2(0.5, 0.75), Vector2(0.875, 1) } };
}
}
int main(int argc, char** argv)
{
    try {
        auto source = fixture();
        const auto selection = selectFbxGeometry(source, { 1 });
        require(selection.object().vertices.size() == 4 && selection.object().triangles == std::vector<std::vector<size_t>> { { 0, 3, 1 } }, "Selection remapped shared vertex identities");
        require(selection.sourceTriangleIndices() == std::vector<size_t> { 1 }, "Source face correspondence lost");
        require(selection.object().triangleVertexNormals()->at(0)[0].y() == 1
                && selection.object().triangleVertexUvs()->at(0)[0].x() == 0.25,
            "Selected corner streams lost source alignment");
        const auto plan = planFbxGeometrySelection(4, 2, 1, true, true);
        require(selection.payloadBytes() <= plan.stagingBytes && plan.stagingBytes <= plan.maximumPayloadBytes, "Selection accounting exceeds admitted plan");
        auto precise = fixture();
        precise.vertices[0][0] = 1000000.123456789;
        require(selectFbxGeometry(precise, { 1 }).object().vertices[0].x() == precise.vertices[0].x(), "Selection lost double precision");
        const auto independent = [] {
            auto input = fixture();
            auto uv = overrides();
            return selectFbxGeometry(input, { 1 }, &uv);
        }();
        require(independent.object().triangleVertexUvs()->at(0)[0].x() == 0.125, "Override borrowed caller storage");
        require(source.triangles.size() == 2 && source.triangleVertexUvs()->at(1)[0].x() == 0.25, "Selection mutated source");
        UvAtlas atlas;
        atlas.triangleUvs = { overrides()[0], overrides()[0] };
        atlas.triangleUvKnown = { 1, 1 };
        const auto packed = selectFbxAtlasGeometry(source, atlas);
        require(packed.object().triangles == source.triangles && packed.object().triangleVertexUvs()->at(1)[0].x() == 0.125,
            "Packed atlas UVs lost original face/corner alignment");
        require(packed.sourceTriangleIndices() == std::vector<size_t>({ 0, 1 }), "Atlas export lost source face correspondence");
        atlas.triangleUvKnown = { 1, 0 };
        rejects([&] { selectFbxAtlasGeometry(source, atlas); }, DUST3D_STATUS_INVALID_DOCUMENT);
        atlas.triangleUvKnown = { 1, 2 };
        rejects([&] { selectFbxAtlasGeometry(source, atlas); }, DUST3D_STATUS_INVALID_DOCUMENT);
        atlas.triangleUvKnown = { 1 };
        rejects([&] { selectFbxAtlasGeometry(source, atlas); }, DUST3D_STATUS_INVALID_DOCUMENT);
        atlas.triangleUvKnown = { 1, 1 };
        atlas.triangleUvs.pop_back();
        rejects([&] { selectFbxAtlasGeometry(source, atlas); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { selectFbxGeometry(source, {}); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { selectFbxGeometry(source, { 2 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { selectFbxGeometry(source, { 1, 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { selectFbxGeometry(source, { 1, 0 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { selectFbxGeometry(source, { 0, 1, 1 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxGeometrySelection(0, 2, 1, false, false); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planFbxGeometrySelection(4, 0, 1, false, false); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planFbxGeometrySelection(std::numeric_limits<size_t>::max(), 2, 1, false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxGeometrySelection(4, 5000001, 1, false, false); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxGeometrySelection(1000000, 5000000, 5000000, true, true); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxGeometrySelection(4, 2, 1, false, false, 268435457); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planFbxGeometrySelection(4, 2, 1, false, false, 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        require(planFbxGeometrySelection(1000000, 5000000, 1, false, false).stagingBytes < 67108864, "Numeric-only source count ceiling rejected");
        auto bad = fixture();
        bad.triangles[1].pop_back();
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.triangles[1][1] = 4;
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.vertices[0][0] = std::numeric_limits<double>::infinity();
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.vertices[0][0] = 4000001;
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        bad = fixture();
        bad.setTriangleVertexNormals({});
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexNormals({ { { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 } }, {} });
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexUvs({ { { 0, 0 }, { 1, 0 }, { 0, 1 } }, {} });
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        bad = fixture();
        bad.setTriangleVertexUvs({});
        rejects([&] { selectFbxGeometry(bad, { 1 }); }, DUST3D_STATUS_INVALID_DOCUMENT);
        auto uv = overrides();
        require(selectFbxGeometry(bad, { 1 }, &uv).object().triangleVertexUvs()->size() == 1, "Explicit override did not replace unused source UVs");
        std::vector<FbxTriangleUvs> missing;
        rejects([&] { selectFbxGeometry(source, { 1 }, &missing); }, DUST3D_STATUS_INVALID_DOCUMENT);
        uv[0][0][0] = std::numeric_limits<double>::quiet_NaN();
        rejects([&] { selectFbxGeometry(source, { 1 }, &uv); }, DUST3D_STATUS_INVALID_DOCUMENT);
        uv = overrides();
        uv[0][0][0] = 1000001;
        rejects([&] { selectFbxGeometry(source, { 1 }, &uv); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        Object plain;
        plain.vertices = source.vertices;
        plain.triangles = source.triangles;
        const auto withoutStreams = selectFbxGeometry(plain, { 1 });
        require(!withoutStreams.object().triangleVertexNormals() && !withoutStreams.object().triangleVertexUvs(), "Absent source streams invented");
        auto moved = selectFbxGeometry(source, { 0, 1 });
        const auto retained = std::move(moved);
        require(retained.sourceTriangleIndices() == std::vector<size_t>({ 0, 1 }), "Selection move lost owned indices");
        const auto output = encodeFbxMeshScene(independent.object());
        if (argc == 2) {
            std::ofstream file(argv[1], std::ios::binary | std::ios::trunc);
            file.exceptions(std::ios::badbit | std::ios::failbit);
            file.write(reinterpret_cast<const char*>(output.bytes().data()), static_cast<std::streamsize>(output.bytes().size()));
            file.close();
        }
        std::cout << "FBX source geometry selection passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

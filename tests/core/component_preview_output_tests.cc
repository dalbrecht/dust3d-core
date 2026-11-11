#include "native/Engine/ComponentPreviewOutput.h"
#include <limits>
#include <stdexcept>

using namespace dust3d;
using namespace dust3d::native;

static void require(bool condition, const char* description)
{
    if (!condition)
        throw std::runtime_error(description);
}

#define REQUIRE(condition) require((condition), #condition)

template <typename Action>
static void expectStatus(Dust3DStatus status, const Action& action)
{
    bool rejected = false;
    try {
        action();
    } catch (const DocumentError& error) {
        rejected = true;
        REQUIRE(error.status() == status);
    }
    REQUIRE(rejected);
}

int main()
{
    const std::string authored = "{12345678-1234-1234-1234-123456789abc}";
    const std::string alias = authored.substr(1, authored.size() - 2);
    Snapshot snapshot;
    snapshot.components[authored] = {};
    snapshot.rootComponent["children"] = authored;
    REQUIRE(resolveComponentPreviewId(snapshot, authored) == Uuid(authored));
    expectStatus(DUST3D_STATUS_INVALID_DOCUMENT, [&] { resolveComponentPreviewId(snapshot, alias); });
    snapshot.components[authored]["children"] = authored;
    expectStatus(DUST3D_STATUS_INVALID_DOCUMENT, [&] { resolveComponentPreviewId(snapshot, authored); });
    snapshot.components[authored].clear();
    snapshot.components[alias] = {};
    expectStatus(DUST3D_STATUS_INVALID_DOCUMENT, [&] { resolveComponentPreviewId(snapshot, authored); });
    snapshot.components.erase(authored);
    snapshot.rootComponent["children"] = alias;
    REQUIRE(resolveComponentPreviewId(snapshot, alias) == Uuid(alias));

    MeshGenerator::ComponentPreview generated;
    generated.vertices = { Vector3(1, 2, 3), Vector3(4, 2, 3), Vector3(1, 5, 3) };
    generated.triangles = { { 0, 1, 2 } };
    generated.color = Color(0.25, 0.5, 0.75);
    generated.metalness = 0.125f;
    generated.roughness = 0.75f;
    const std::array<Vector2, 3> indexedUvs { Vector2(0, 0), Vector2(1, 0), Vector2(0, 1) };
    generated.triangleCornerUvs = { indexedUvs };
    generated.triangleUvs[{ PositionKey(generated.vertices[0]), PositionKey(generated.vertices[1]),
        PositionKey(generated.vertices[2]) }]
        = { Vector2(9, 9), Vector2(9, 9), Vector2(9, 9) };
    auto output = copyComponentPreview(generated, "texture", "", 4096);
    REQUIRE(dust3d_component_preview_vertex_count(output.get()) == 3);
    REQUIRE(dust3d_component_preview_triangle_count(output.get()) == 1);
    const auto* points = dust3d_component_preview_vertices(output.get());
    const auto* triangles = dust3d_component_preview_triangles(output.get());
    REQUIRE(points[0].position[0] == 1 && points[2].position[1] == 5);
    REQUIRE(points[0].color[1] == 0.5 && points[1].roughness == 0.75f);
    REQUIRE(triangles[0].indices[2] == 2 && triangles[0].has_uv && triangles[0].uv[2][1] == 1);
    REQUIRE(std::string(dust3d_component_preview_color_image_id(output.get())) == "texture");
    generated.vertices[0] = Vector3(99, 99, 99);
    REQUIRE(points[0].position[0] == 1);

    expectStatus(DUST3D_STATUS_LIMIT_EXCEEDED, [&] { copyComponentPreview(generated, {}, {}, 1); });
    generated.triangles[0][1] = 8;
    expectStatus(DUST3D_STATUS_GENERATION_FAILED, [&] { copyComponentPreview(generated, {}, {}, 4096); });
    generated.triangles.clear();
    expectStatus(DUST3D_STATUS_NO_GEOMETRY, [&] { copyComponentPreview(generated, {}, {}, 4096); });

    generated.cutFaceTemplate = { Vector2(0, 0), Vector2(1, 0), Vector2(0, 1) };
    auto cutFace = copyComponentPreview(generated, {}, {}, 4096);
    REQUIRE(dust3d_component_preview_vertex_count(cutFace.get()) == 0);
    REQUIRE(dust3d_component_preview_cut_face_count(cutFace.get()) == 3);
    REQUIRE(dust3d_component_preview_cut_face(cutFace.get())[2].y == 1);
    generated.cutFaceTemplate[0] = Vector2(std::numeric_limits<double>::infinity(), 0);
    expectStatus(DUST3D_STATUS_GENERATION_FAILED, [&] { copyComponentPreview(generated, {}, {}, 4096); });
}

#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/mesh/mesh_generator.h>
#include <dust3d/mesh/solid_mesh.h>

namespace dust3d {
void SolidMesh::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [vertices, triangles, normals, tree, boxes] = *this;
    budget.accountBorrowedPointer(vertices);
    budget.accountBorrowedPointer(triangles);
    budget.accountOwnedPointer(normals);
    budget.accountOwnedPointer(tree);
    budget.accountOwnedPointer(boxes);
}

void MeshCombiner::Mesh::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [solid, vertices, triangles] = *this;
    budget.accountDynamicFields(solid, vertices, triangles);
}

void MeshState::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [mesh, seamUvs, brokenTriangles, owners, models, colors, cornerUvs, reports] = *this;
    budget.accountDynamicFields(mesh, seamUvs, brokenTriangles, owners, models, colors, cornerUvs, reports);
}

void MeshGenerator::GeneratedPart::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [vertices, sources, nodes, faces, uvs, color, metalness, roughness, successful, joined, importedColors, importedUvs, importedNormals] = *this;
    budget.accountDynamicFields(vertices, sources, nodes, faces, uvs, color, metalness, roughness, successful, joined, importedColors, importedUvs, importedNormals);
}

void MeshGenerator::GeneratedComponent::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [mesh, quadEdges, componentUvs, brokenTriangles, seamVertices, sources, nodes, weights, attributes, wrapOutput, wrapColor, folds] = *this;
    budget.accountDynamicFields(mesh, quadEdges, componentUvs, brokenTriangles, seamVertices, sources, nodes, weights, attributes, wrapOutput, wrapColor, folds);
}

void MeshGenerator::GeneratedCacheContext::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [components, parts, mirrors, combinations, wraps] = *this;
    budget.accountDynamicFields(components, parts, mirrors, combinations, wraps);
}

void WrapMeshBuilder::NodeWeight::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [node, weight] = *this;
    budget.accountDynamicFields(node, weight);
}

void WrapMeshBuilder::CachedResult::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [surfaceKey, weightKey, hasWeights, vertices, faces, sources, cellSize, remeshed, resultVertices, triangles, quads, diagonals, uvs, weights] = *this;
    budget.accountDynamicFields(surfaceKey, weightKey, hasWeights, vertices, faces, sources, cellSize, remeshed, resultVertices, triangles, quads, diagonals, uvs, weights);
}

void WrapMeshBuilder::Cache::accountDynamicPayload(RetainedPayloadBudget& budget) const noexcept
{
    const auto& [entries, capacity] = *this;
    budget.accountDynamicFields(entries, capacity);
}
}

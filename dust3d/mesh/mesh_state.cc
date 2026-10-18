/*
 *  Copyright (c) 2016-2022 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <dust3d/base/debug.h>
#include <dust3d/mesh/mesh_recombiner.h>
#include <dust3d/mesh/mesh_state.h>
#include <memory>

namespace dust3d {

namespace {
    std::optional<std::array<Color, 3>> transferCornerColors(const std::vector<Vector3>& sourceVertices,
        const std::vector<size_t>& sourceFace, const std::array<Color, 3>& colors,
        const std::vector<Vector3>& outputVertices, const std::vector<size_t>& outputFace)
    {
        if (sourceFace.size() != 3 || outputFace.size() != 3)
            return std::nullopt;
        const auto& a = sourceVertices.at(sourceFace[0]);
        const auto& b = sourceVertices.at(sourceFace[1]);
        const auto& c = sourceVertices.at(sourceFace[2]);
        std::array<Color, 3> result;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& position = outputVertices.at(outputFace[corner]);
            bool matches = false;
            for (size_t original = 0; original < 3; ++original) {
                const auto& source = sourceVertices.at(sourceFace[original]);
                if (position.x() == source.x() && position.y() == source.y() && position.z() == source.z()) {
                    result[corner] = colors[original];
                    matches = true;
                    break;
                }
            }
            if (matches)
                continue;
            auto first = b - a;
            auto second = c - a;
            auto offset = position - a;
            double scale = 0;
            for (size_t axis = 0; axis < 3; ++axis)
                scale = std::max({ scale, std::abs(first[axis]), std::abs(second[axis]) });
            if (!std::isfinite(scale) || scale <= 0)
                return std::nullopt;
            first /= scale;
            second /= scale;
            offset /= scale;
            // The cross form avoids subtracting nearly equal squared dot products.
            const auto normal = Vector3::crossProduct(first, second);
            const double denominator = normal.lengthSquared();
            if (!std::isfinite(denominator) || denominator <= 0)
                return std::nullopt;
            double u = Vector3::dotProduct(Vector3::crossProduct(offset, second), normal) / denominator;
            double v = Vector3::dotProduct(Vector3::crossProduct(first, offset), normal) / denominator;
            if (!std::isfinite(u) || !std::isfinite(v) || u < -1e-6 || v < -1e-6 || u + v > 1 + 1e-6)
                return std::nullopt;
            double w = std::max(0.0, 1 - u - v);
            u = std::max(0.0, u);
            v = std::max(0.0, v);
            const double total = w + u + v;
            w /= total;
            u /= total;
            v /= total;
            result[corner] = Color(colors[0].r() * w + colors[1].r() * u + colors[2].r() * v,
                colors[0].g() * w + colors[1].g() * u + colors[2].g() * v,
                colors[0].b() * w + colors[1].b() * u + colors[2].b() * v,
                colors[0].alpha() * w + colors[1].alpha() * u + colors[2].alpha() * v);
        }
        return result;
    }
}

MeshState::MeshState(const std::vector<Vector3>& vertices, const std::vector<std::vector<size_t>>& faces)
{
    mesh = std::make_unique<MeshCombiner::Mesh>(vertices, faces);
}

MeshState::MeshState(const MeshState& other)
{
    if (nullptr != other.mesh)
        mesh = std::make_unique<MeshCombiner::Mesh>(*other.mesh);
    seamTriangleUvs = other.seamTriangleUvs;
    seamReports = other.seamReports;
    triangleComponentKeys = other.triangleComponentKeys;
    triangleVertexColors = other.triangleVertexColors;
}

void MeshState::fetch(std::vector<Vector3>& vertices, std::vector<std::vector<size_t>>& faces) const
{
    if (mesh)
        mesh->fetch(vertices, faces);
}

bool MeshState::isNull() const
{
    if (nullptr == mesh)
        return true;
    return mesh->isNull();
}

std::unique_ptr<MeshState> MeshState::combine(const MeshState& first, const MeshState& second,
    MeshCombiner::Method method, bool recombine)
{
    if (first.mesh->isNull() || second.mesh->isNull())
        return nullptr;
    auto newMeshState = std::make_unique<MeshState>();
    std::vector<std::pair<MeshCombiner::Source, size_t>> combinedVerticesSources;
    std::vector<MeshCombiner::TriangleSource> combinedTriangleSources;
    const bool shouldTrackOwners = !first.triangleComponentKeys.empty() || !second.triangleComponentKeys.empty();
    const bool shouldTrackColors = !first.triangleVertexColors.empty() || !second.triangleVertexColors.empty();
    auto newMesh = std::unique_ptr<MeshCombiner::Mesh>(MeshCombiner::combine(*first.mesh,
        *second.mesh,
        method,
        &combinedVerticesSources, (shouldTrackOwners || shouldTrackColors) ? &combinedTriangleSources : nullptr));
    if (nullptr == newMesh)
        return nullptr;
    if (shouldTrackOwners) {
        newMeshState->triangleComponentKeys.reserve(combinedTriangleSources.size());
        for (const auto& source : combinedTriangleSources) {
            const auto& owners = source.source == MeshCombiner::Source::First ? first.triangleComponentKeys : second.triangleComponentKeys;
            newMeshState->triangleComponentKeys.push_back(source.triangleIndex < owners.size() ? owners[source.triangleIndex] : std::string());
        }
    }
    if (shouldTrackColors) {
        std::vector<Vector3> sourceVertices[2], outputVertices;
        std::vector<std::vector<size_t>> sourceFaces[2], outputFaces;
        first.fetch(sourceVertices[0], sourceFaces[0]);
        second.fetch(sourceVertices[1], sourceFaces[1]);
        newMesh->fetch(outputVertices, outputFaces);
        for (size_t index = 0; index < combinedTriangleSources.size(); ++index) {
            const auto& source = combinedTriangleSources[index];
            const bool isSecond = source.source == MeshCombiner::Source::Second;
            const auto& colors = isSecond ? second.triangleVertexColors : first.triangleVertexColors;
            if (source.triangleIndex < colors.size() && colors[source.triangleIndex])
                newMeshState->triangleVertexColors.push_back(transferCornerColors(sourceVertices[isSecond],
                    sourceFaces[isSecond].at(source.triangleIndex), *colors[source.triangleIndex], outputVertices, outputFaces.at(index)));
            else
                newMeshState->triangleVertexColors.push_back(std::nullopt);
        }
    }
    if (recombine && !newMesh->isNull()) {
        MeshRecombiner recombiner;
        std::vector<Vector3> combinedVertices;
        std::vector<std::vector<size_t>> combinedFaces;
        newMesh->fetch(combinedVertices, combinedFaces);
        recombiner.setVertices(&combinedVertices, &combinedVerticesSources);
        recombiner.setFaces(&combinedFaces);
        if (recombiner.recombine()) {
            newMeshState->seamReports = recombiner.seamReports();
            auto reMesh = std::make_unique<MeshCombiner::Mesh>(recombiner.regeneratedVertices(), recombiner.regeneratedFaces());
            if (!reMesh->isNull()) {
                for (const auto& bridgingTriangles : recombiner.generatedBridgingTriangles()) {
                    std::pair<std::set<std::array<PositionKey, 3>>, std::set<std::array<PositionKey, 3>>> triangles;
                    for (const auto& tri : bridgingTriangles.first) {
                        triangles.first.insert(std::array<PositionKey, 3> {
                            PositionKey(tri[0]),
                            PositionKey(tri[1]),
                            PositionKey(tri[2]) });
                    }
                    for (const auto& tri : bridgingTriangles.second) {
                        triangles.second.insert(std::array<PositionKey, 3> {
                            PositionKey(tri[0]),
                            PositionKey(tri[1]),
                            PositionKey(tri[2]) });
                    }
                    if (triangles.first.empty() && triangles.second.empty())
                        continue;
                    newMeshState->seamTriangleUvs.push_back(triangles);
                }
                if (shouldTrackOwners) {
                    std::vector<std::string> regeneratedOwners;
                    regeneratedOwners.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedOwners.push_back(source ? newMeshState->triangleComponentKeys.at(*source) : std::string());
                    newMeshState->triangleComponentKeys = std::move(regeneratedOwners);
                }
                if (shouldTrackColors) {
                    std::vector<std::optional<std::array<Color, 3>>> regeneratedColors;
                    regeneratedColors.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedColors.push_back(source ? newMeshState->triangleVertexColors.at(*source) : std::nullopt);
                    newMeshState->triangleVertexColors = std::move(regeneratedColors);
                }
                newMesh = std::move(reMesh);
            }
            for (const auto& faceIt : recombiner.inputFacesInSeamArea()) {
                const auto& face = combinedFaces[faceIt.first];
                newMeshState->brokenTriangles.push_back(std::array<PositionKey, 3> {
                    PositionKey(combinedVertices[face[0]]),
                    PositionKey(combinedVertices[face[1]]),
                    PositionKey(combinedVertices[face[2]]) });
            }
        }
    }
    if (newMesh->isNull()) {
        return nullptr;
    }
    newMeshState->mesh = std::move(newMesh);
    for (const auto& it : first.seamTriangleUvs)
        newMeshState->seamTriangleUvs.push_back(it);
    for (const auto& it : second.seamTriangleUvs)
        newMeshState->seamTriangleUvs.push_back(it);
    return newMeshState;
}

bool MeshState::isWatertight(const std::vector<std::vector<size_t>>& faces)
{
    std::set<std::pair<size_t, size_t>> halfEdges;
    for (const auto& face : faces) {
        for (size_t i = 0; i < face.size(); ++i) {
            size_t j = (i + 1) % face.size();
            auto insertResult = halfEdges.insert({ face[i], face[j] });
            if (!insertResult.second)
                return false;
        }
    }
    for (const auto& it : halfEdges) {
        if (halfEdges.find({ it.second, it.first }) == halfEdges.end())
            return false;
    }
    return true;
}

}

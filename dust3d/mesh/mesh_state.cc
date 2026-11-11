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
#include <limits>
#include <memory>

namespace dust3d {

namespace {
    Color interpolateCorner(const std::array<Color, 3>& values, double w, double u, double v)
    {
        return Color(values[0].r() * w + values[1].r() * u + values[2].r() * v,
            values[0].g() * w + values[1].g() * u + values[2].g() * v,
            values[0].b() * w + values[1].b() * u + values[2].b() * v,
            values[0].alpha() * w + values[1].alpha() * u + values[2].alpha() * v);
    }

    Vector2 interpolateCorner(const std::array<Vector2, 3>& values, double w, double u, double v)
    {
        const auto result = values[0] * w + values[1] * u + values[2] * v;
        // Normalized barycentric arithmetic can overshoot a unit boundary by an
        // ulp. Preserve invalid/tiled source input rather than silently repairing it.
        const bool unitSource = std::all_of(values.begin(), values.end(), [](const Vector2& value) {
            return std::isfinite(value.x()) && std::isfinite(value.y())
                && value.x() >= 0 && value.x() <= 1 && value.y() >= 0 && value.y() <= 1;
        });
        return unitSource ? Vector2(std::clamp(result.x(), 0.0, 1.0), std::clamp(result.y(), 0.0, 1.0)) : result;
    }

    using SourceWeights = std::map<Uuid, double>;
    // Keep duplicate input entries valid, accumulating only in double precision.
    std::optional<SourceWeights> sourceWeights(const SourceCorner& source)
    {
        if (source.influences().empty())
            return std::nullopt;
        if (!source.weighted && (source.influences().size() != 1 || source.influences()[0].second != 1))
            return std::nullopt;
        SourceWeights weights;
        for (const auto& influence : source.influences()) {
            if (influence.first.isNull() || !std::isfinite(influence.second) || influence.second < 0)
                return std::nullopt;
            if (influence.second > 0) {
                auto& weight = weights[influence.first];
                weight += influence.second;
                if (!std::isfinite(weight))
                    return std::nullopt;
            }
        }
        return weights.empty() ? std::nullopt : std::optional<SourceWeights>(std::move(weights));
    }

    SourceCorner interpolateCorner(const std::array<SourceCorner, 3>& values, double w, double u, double v)
    {
        const std::array<double, 3> coefficients { w, u, v };
        SourceWeights weights;
        bool weighted = false;
        for (size_t corner = 0; corner < 3; ++corner) {
            if (coefficients[corner] <= 0)
                continue;
            const auto source = sourceWeights(values[corner]);
            // A positive unknown contributor invalidates the whole derived hint.
            if (!source)
                return {};
            weighted = weighted || values[corner].weighted;
            for (const auto& influence : *source) {
                const double contribution = coefficients[corner] * influence.second;
                auto& weight = weights[influence.first];
                weight += contribution;
                if (!std::isfinite(contribution) || contribution <= 0 || !std::isfinite(weight))
                    return {};
            }
        }
        SourceCorner result;
        result.weighted = weighted || weights.size() > 1;
        SourceCorner::Influences influences;
        influences.reserve(weights.size());
        for (const auto& influence : weights) {
            if (influence.second <= 0)
                continue;
            if (influence.second > std::numeric_limits<float>::max())
                return {};
            const float stored = result.weighted ? float(influence.second) : 1.0f;
            if (!std::isfinite(stored) || stored <= 0)
                return {}; // Positive ownership must never silently underflow away.
            influences.emplace_back(influence.first, stored);
        }
        result.setInfluences(std::move(influences));
        return result;
    }

    template <typename Attribute>
    std::optional<std::array<Attribute, 3>> transferCornerAttributes(const std::vector<Vector3>& sourceVertices,
        const std::vector<size_t>& sourceFace, const std::array<Attribute, 3>& values,
        const std::vector<Vector3>& outputVertices, const std::vector<size_t>& outputFace)
    {
        if (sourceFace.size() != 3 || outputFace.size() != 3)
            return std::nullopt;
        const auto& a = sourceVertices.at(sourceFace[0]);
        const auto& b = sourceVertices.at(sourceFace[1]);
        const auto& c = sourceVertices.at(sourceFace[2]);
        std::array<Attribute, 3> result;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& position = outputVertices.at(outputFace[corner]);
            bool matches = false;
            for (size_t original = 0; original < 3; ++original) {
                const auto& source = sourceVertices.at(sourceFace[original]);
                if (position.x() == source.x() && position.y() == source.y() && position.z() == source.z()) {
                    result[corner] = values[original];
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
            result[corner] = interpolateCorner(values, w, u, v);
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
    hasHollowWalls = other.hasHollowWalls;
    seamTriangleUvs = other.seamTriangleUvs;
    seamReports = other.seamReports;
    triangleComponentKeys = other.triangleComponentKeys;
    triangleModelKeys = other.triangleModelKeys;
    triangleVertexColors = other.triangleVertexColors;
    triangleCornerUvs = other.triangleCornerUvs;
    triangleProceduralUvs = other.triangleProceduralUvs;
    triangleSourceCorners = other.triangleSourceCorners;
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
    newMeshState->hasHollowWalls = first.hasHollowWalls || second.hasHollowWalls;
    std::vector<std::pair<MeshCombiner::Source, size_t>> combinedVerticesSources;
    std::vector<MeshCombiner::TriangleSource> combinedTriangleSources;
    const bool shouldTrackOwners = !first.triangleComponentKeys.empty() || !second.triangleComponentKeys.empty();
    const bool shouldTrackModels = !first.triangleModelKeys.empty() || !second.triangleModelKeys.empty();
    const bool shouldTrackColors = !first.triangleVertexColors.empty() || !second.triangleVertexColors.empty();
    const bool shouldTrackProceduralUvs = !first.triangleProceduralUvs.empty() || !second.triangleProceduralUvs.empty();
    const bool shouldTrackSources = !first.triangleSourceCorners.empty() || !second.triangleSourceCorners.empty();
    const bool shouldTrackUvs = !first.triangleCornerUvs.empty() || !second.triangleCornerUvs.empty();
    auto newMesh = std::unique_ptr<MeshCombiner::Mesh>(MeshCombiner::combine(*first.mesh,
        *second.mesh,
        method,
        &combinedVerticesSources, (shouldTrackOwners || shouldTrackModels || shouldTrackColors || shouldTrackUvs || shouldTrackProceduralUvs || shouldTrackSources) ? &combinedTriangleSources : nullptr,
        newMeshState->hasHollowWalls));
    if (nullptr == newMesh)
        return nullptr;
    if (shouldTrackOwners) {
        newMeshState->triangleComponentKeys.reserve(combinedTriangleSources.size());
        for (const auto& source : combinedTriangleSources) {
            const auto& owners = source.source == MeshCombiner::Source::First ? first.triangleComponentKeys : second.triangleComponentKeys;
            newMeshState->triangleComponentKeys.push_back(source.triangleIndex < owners.size() ? owners[source.triangleIndex] : std::string());
        }
    }
    if (shouldTrackModels) {
        newMeshState->triangleModelKeys.reserve(combinedTriangleSources.size());
        for (const auto& source : combinedTriangleSources) {
            const auto& models = source.source == MeshCombiner::Source::First ? first.triangleModelKeys : second.triangleModelKeys;
            newMeshState->triangleModelKeys.push_back(source.triangleIndex < models.size() ? models[source.triangleIndex] : std::string());
        }
    }
    if (shouldTrackColors || shouldTrackUvs || shouldTrackProceduralUvs || shouldTrackSources) {
        std::vector<Vector3> sourceVertices[2], outputVertices;
        std::vector<std::vector<size_t>> sourceFaces[2], outputFaces;
        first.fetch(sourceVertices[0], sourceFaces[0]);
        second.fetch(sourceVertices[1], sourceFaces[1]);
        newMesh->fetch(outputVertices, outputFaces);
        for (size_t index = 0; index < combinedTriangleSources.size(); ++index) {
            const auto& source = combinedTriangleSources[index];
            const bool isSecond = source.source == MeshCombiner::Source::Second;
            if (shouldTrackSources) {
                const auto& sources = isSecond ? second.triangleSourceCorners : first.triangleSourceCorners;
                SourceTriangleCorners record;
                if (source.triangleIndex < sources.size()) {
                    const auto transferred = transferCornerAttributes(sourceVertices[isSecond], sourceFaces[isSecond].at(source.triangleIndex),
                        sources[source.triangleIndex].corners, outputVertices, outputFaces.at(index));
                    if (transferred)
                        record.corners = *transferred;
                }
                newMeshState->triangleSourceCorners.push_back(std::move(record));
            }
            if (shouldTrackColors) {
                const auto& colors = isSecond ? second.triangleVertexColors : first.triangleVertexColors;
                if (source.triangleIndex < colors.size() && colors[source.triangleIndex])
                    newMeshState->triangleVertexColors.push_back(transferCornerAttributes(sourceVertices[isSecond],
                        sourceFaces[isSecond].at(source.triangleIndex), *colors[source.triangleIndex], outputVertices, outputFaces.at(index)));
                else
                    newMeshState->triangleVertexColors.push_back(std::nullopt);
            }
            if (shouldTrackProceduralUvs) {
                const auto& records = isSecond ? second.triangleProceduralUvs : first.triangleProceduralUvs;
                std::optional<ProceduralTriangleUv> record;
                if (source.triangleIndex < records.size() && records[source.triangleIndex]) {
                    const auto& original = *records[source.triangleIndex];
                    const auto corners = transferCornerAttributes(sourceVertices[isSecond], sourceFaces[isSecond].at(source.triangleIndex),
                        original.uv, outputVertices, outputFaces.at(index));
                    if (corners) {
                        record = original;
                        record->uv = *corners;
                    }
                }
                newMeshState->triangleProceduralUvs.push_back(std::move(record));
            }
            if (shouldTrackUvs) {
                const auto& uvs = isSecond ? second.triangleCornerUvs : first.triangleCornerUvs;
                if (source.triangleIndex < uvs.size() && uvs[source.triangleIndex])
                    newMeshState->triangleCornerUvs.push_back(transferCornerAttributes(sourceVertices[isSecond],
                        sourceFaces[isSecond].at(source.triangleIndex), *uvs[source.triangleIndex], outputVertices, outputFaces.at(index)));
                else
                    newMeshState->triangleCornerUvs.push_back(std::nullopt);
            }
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
                if (shouldTrackSources) {
                    // Resolve consensus by actual combined vertex identity, never by a
                    // positional node hint or a coincident point from the other mesh.
                    std::vector<SourceCorner> vertexSources(combinedVertices.size());
                    std::vector<SourceWeights> vertexWeights(combinedVertices.size());
                    std::vector<bool> seen(combinedVertices.size(), false);
                    std::vector<bool> conflicts(combinedVertices.size(), false);
                    for (size_t face = 0; face < combinedFaces.size(); ++face) {
                        for (size_t corner = 0; corner < combinedFaces[face].size(); ++corner) {
                            const auto vertex = combinedFaces[face][corner];
                            const auto distribution = face < newMeshState->triangleSourceCorners.size() && corner < 3
                                ? newMeshState->triangleSourceCorners[face][corner]
                                : SourceCorner {};
                            const auto weights = sourceWeights(distribution);
                            if (!weights)
                                conflicts.at(vertex) = true;
                            else if (!seen.at(vertex)) {
                                vertexSources.at(vertex) = distribution;
                                vertexWeights.at(vertex) = *weights;
                            } else if (vertexSources.at(vertex).weighted != distribution.weighted
                                || vertexWeights.at(vertex) != *weights)
                                conflicts.at(vertex) = true;
                            seen.at(vertex) = true;
                        }
                    }
                    std::vector<SourceTriangleCorners> regeneratedSources;
                    const auto& faces = recombiner.regeneratedFaces();
                    const auto& vertexInputs = recombiner.regeneratedVertexInputIndices();
                    for (size_t index = 0; index < faces.size(); ++index) {
                        const auto& source = recombiner.regeneratedFaceSources().at(index);
                        SourceTriangleCorners record;
                        if (source)
                            record = newMeshState->triangleSourceCorners.at(*source);
                        else if (faces[index].size() == 3) {
                            for (size_t corner = 0; corner < 3; ++corner) {
                                const auto originalVertex = vertexInputs.at(faces[index][corner]);
                                if (seen.at(originalVertex) && !conflicts.at(originalVertex))
                                    record[corner] = vertexSources.at(originalVertex);
                            }
                        }
                        regeneratedSources.push_back(std::move(record));
                    }
                    newMeshState->triangleSourceCorners = std::move(regeneratedSources);
                }
                if (shouldTrackOwners) {
                    std::vector<std::string> regeneratedOwners;
                    regeneratedOwners.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedOwners.push_back(source ? newMeshState->triangleComponentKeys.at(*source) : std::string());
                    newMeshState->triangleComponentKeys = std::move(regeneratedOwners);
                }
                if (shouldTrackModels) {
                    std::vector<std::string> regeneratedModels;
                    regeneratedModels.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedModels.push_back(source ? newMeshState->triangleModelKeys.at(*source) : std::string());
                    newMeshState->triangleModelKeys = std::move(regeneratedModels);
                }
                if (shouldTrackColors) {
                    std::vector<std::optional<std::array<Color, 3>>> regeneratedColors;
                    regeneratedColors.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedColors.push_back(source ? newMeshState->triangleVertexColors.at(*source) : std::nullopt);
                    newMeshState->triangleVertexColors = std::move(regeneratedColors);
                }
                if (shouldTrackProceduralUvs) {
                    const auto originalUvs = newMeshState->triangleProceduralUvs;
                    std::vector<std::optional<ProceduralTriangleUv>> regeneratedUvs;
                    regeneratedUvs.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedUvs.push_back(source ? newMeshState->triangleProceduralUvs.at(*source) : std::nullopt);
                    // Resolve exact vertex ownership once; bridge groups borrow these stable
                    // original records rather than repeatedly scanning every source face.
                    std::vector<const Uuid*> vertexOwners(combinedVertices.size(), nullptr);
                    std::vector<bool> ambiguousVertexOwners(combinedVertices.size(), false);
                    for (size_t index = 0; index < combinedFaces.size(); ++index) {
                        const auto* record = index < originalUvs.size() && originalUvs[index] ? &*originalUvs[index] : nullptr;
                        for (const auto vertex : combinedFaces[index]) {
                            // One mapped incident face cannot prove ownership when
                            // another contributor is unmapped or has mixed owners.
                            if (!record || record->gradient || record->componentId.isNull()) {
                                ambiguousVertexOwners.at(vertex) = true;
                                continue;
                            }
                            const auto& owner = record->componentId;
                            auto& known = vertexOwners.at(vertex);
                            if (known && *known != owner)
                                ambiguousVertexOwners.at(vertex) = true;
                            else if (!known)
                                known = &owner;
                        }
                    }
                    auto boundaryOwner = [&](const std::vector<size_t>& boundary) {
                        const Uuid* candidate = nullptr;
                        for (const auto vertex : boundary) {
                            const auto* owner = vertexOwners.at(vertex);
                            // Every endpoint sample must have one proven owner.
                            if (!owner || ambiguousVertexOwners.at(vertex) || (candidate && *candidate != *owner))
                                return Uuid();
                            candidate = owner;
                        }
                        return candidate ? *candidate : Uuid();
                    };
                    const auto& groups = recombiner.generatedBridgingFaceIndices();
                    const auto& boundaries = recombiner.generatedBridgingBoundaryVertices();
                    const double minimum = .5 / 512;
                    const double maximum = 1 - minimum;
                    const double middle = (minimum + maximum) * .5;
                    for (size_t group = 0; group < groups.size() && group < boundaries.size(); ++group) {
                        const ProceduralUvGradient gradient { Uuid::createUuid(), boundaryOwner(boundaries[group].first), boundaryOwner(boundaries[group].second) };
                        auto assign = [&](const std::vector<size_t>& indices, const std::array<Vector2, 3>& corners) {
                            for (const auto index : indices) {
                                if (index >= regeneratedUvs.size())
                                    continue;
                                ProceduralTriangleUv record;
                                record.uv = corners;
                                record.gradient = gradient;
                                regeneratedUvs[index] = std::move(record);
                            }
                        };
                        assign(groups[group].first, { Vector2(minimum, minimum), Vector2(minimum, maximum), Vector2(maximum, middle) });
                        assign(groups[group].second, { Vector2(maximum, minimum), Vector2(maximum, maximum), Vector2(minimum, middle) });
                    }
                    newMeshState->triangleProceduralUvs = std::move(regeneratedUvs);
                }
                if (shouldTrackUvs) {
                    std::vector<std::optional<std::array<Vector2, 3>>> regeneratedUvs;
                    regeneratedUvs.reserve(recombiner.regeneratedFaceSources().size());
                    for (const auto& source : recombiner.regeneratedFaceSources())
                        regeneratedUvs.push_back(source ? newMeshState->triangleCornerUvs.at(*source) : std::nullopt);
                    newMeshState->triangleCornerUvs = std::move(regeneratedUvs);
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

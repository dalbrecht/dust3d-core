/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
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

#include <GuigueDevillers03/tri_tri_intersect.h>
#include <dust3d/base/debug.h>
#include <dust3d/base/position_key.h>
#include <dust3d/mesh/mesh_state.h>
#include <dust3d/mesh/re_triangulator.h>
#include <dust3d/mesh/solid_mesh_boolean_operation.h>
#include <numeric>
#include <optional>
#include <queue>
#include <set>
#include <stdio.h>
#include <utility>

namespace dust3d {

// Intersection duplicates differ by arithmetic roundoff, while distinct hollow walls can be closer than PositionKey buckets.
static double intersectionTolerance(double coordinateMagnitude)
{
    return std::max(1e-12, coordinateMagnitude * (4 * std::numeric_limits<double>::epsilon()));
}

static double intersectionTolerance(const Vector3& point)
{
    return intersectionTolerance(std::max({ std::abs(point.x()), std::abs(point.y()), std::abs(point.z()) }));
}

static const std::vector<Vector3> g_testAxisList = {
    { std::numeric_limits<double>::max(), std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::epsilon() },
    { std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::max(), std::numeric_limits<double>::epsilon() },
    { std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::epsilon(), std::numeric_limits<double>::max() },
};

SolidMeshBooleanOperation::SolidMeshBooleanOperation(const SolidMesh* m_firstMesh,
    const SolidMesh* m_secondMesh, bool preserveThinWalls)
    : m_firstMesh(m_firstMesh)
    , m_secondMesh(m_secondMesh)
    , m_preserveThinWalls(preserveThinWalls)
{
}

SolidMeshBooleanOperation::~SolidMeshBooleanOperation()
{
}

bool SolidMeshBooleanOperation::isPointInMesh(const Vector3& testPosition,
    const SolidMesh* targetMesh,
    const AxisAlignedBoudingBoxTree* meshBoxTree,
    size_t rayAxis)
{
    Vector3 testEnd = testPosition + g_testAxisList[rayAxis];
    bool inside = false;
    std::vector<AxisAlignedBoudingBox> rayBox(1);
    auto& box = rayBox[0];
    box.update(testPosition);
    box.update(testEnd);
    AxisAlignedBoudingBoxTree testTree(&rayBox,
        { 0 },
        rayBox[0]);
    std::vector<std::pair<size_t, size_t>> pairs;
    meshBoxTree->test(meshBoxTree->root(), testTree.root(), &rayBox, &pairs);
    std::vector<double> hits;
    std::set<PositionKey> ordinaryHits;

    for (const auto& it : pairs) {
        const auto& triangle = (*targetMesh->triangles())[it.first];
        std::vector<Vector3> trianglePositions = {
            (*targetMesh->vertices())[triangle[0]],
            (*targetMesh->vertices())[triangle[1]],
            (*targetMesh->vertices())[triangle[2]]
        };
        Vector3 intersection;
        if (Vector3::intersectSegmentAndPlane(testPosition, testEnd,
                trianglePositions[0],
                (*targetMesh->triangleNormals())[it.first],
                &intersection)) {
            std::vector<Vector3> normals;
            for (size_t i = 0; i < 3; ++i) {
                size_t j = (i + 1) % 3;
                normals.push_back(m_preserveThinWalls
                        ? Vector3::crossProduct(trianglePositions[i] - intersection, trianglePositions[j] - intersection)
                        : Vector3::normal(intersection, trianglePositions[i], trianglePositions[j]));
            }
            if (Vector3::dotProduct(normals[0], normals[1]) > 0 && Vector3::dotProduct(normals[0], normals[2]) > 0) {
                if (m_preserveThinWalls)
                    hits.push_back(intersection[rayAxis]);
                else
                    ordinaryHits.insert(PositionKey(intersection));
            }
        }
    }
    if (!m_preserveThinWalls)
        return 0 != ordinaryHits.size() % 2;

    std::sort(hits.begin(), hits.end());
    size_t distinctHitCount = 0;
    double lastHit = -std::numeric_limits<double>::infinity();
    for (const double hit : hits) {
        const double tolerance = std::max(intersectionTolerance(testPosition), intersectionTolerance(std::abs(hit)));
        if (hit - lastHit <= tolerance)
            continue;
        lastHit = hit;
        ++distinctHitCount;
    }
    inside = 0 != distinctHitCount % 2;

    return inside;
}

void SolidMeshBooleanOperation::searchPotentialIntersectedPairs()
{
    const AxisAlignedBoudingBoxTree* leftTree = m_firstMesh->axisAlignedBoundingBoxTree();
    const AxisAlignedBoudingBoxTree* rightTree = m_secondMesh->axisAlignedBoundingBoxTree();

    leftTree->test(leftTree->root(), rightTree->root(), m_secondMesh->triangleAxisAlignedBoundingBoxes(),
        &m_potentialIntersectedPairs);
}

bool SolidMeshBooleanOperation::intersectTwoFaces(size_t firstIndex, size_t secondIndex, std::pair<Vector3, Vector3>& newEdge)
{
    const auto& firstFace = (*m_firstMesh->triangles())[firstIndex];
    const auto& secondFace = (*m_secondMesh->triangles())[secondIndex];
    int coplanar = 0;
    if (!tri_tri_intersection_test_3d((double*)(*m_firstMesh->vertices())[firstFace[0]].constData(),
            (double*)(*m_firstMesh->vertices())[firstFace[1]].constData(),
            (double*)(*m_firstMesh->vertices())[firstFace[2]].constData(),
            (double*)(*m_secondMesh->vertices())[secondFace[0]].constData(),
            (double*)(*m_secondMesh->vertices())[secondFace[1]].constData(),
            (double*)(*m_secondMesh->vertices())[secondFace[2]].constData(),
            &coplanar,
            (double*)newEdge.first.constData(),
            (double*)newEdge.second.constData())) {
        return false;
    }
    if (coplanar) {
        m_hasCoplanarContact = true;
        return false;
    }
    return true;
}

bool SolidMeshBooleanOperation::buildPolygonsFromEdges(const std::map<size_t, std::set<size_t>>& edges,
    std::vector<std::vector<size_t>>& polygons)
{
    for (const auto& entry : edges) {
        if (entry.second.size() % 2 != 0 || entry.second.count(entry.first) != 0) {
            dust3dDebug << "Intersection boundary has an open or self edge";
            return false;
        }
        for (size_t neighbor : entry.second) {
            auto other = edges.find(neighbor);
            if (other == edges.end() || other->second.count(entry.first) == 0) {
                dust3dDebug << "Intersection boundary has asymmetric edges";
                return false;
            }
        }
    }

    auto remainingEdges = edges;
    std::vector<std::vector<size_t>> result;
    for (const auto& entry : edges) {
        while (!remainingEdges.at(entry.first).empty()) {
            std::vector<size_t> path { entry.first };
            std::map<size_t, size_t> pathIndices { { entry.first, 0 } };
            size_t current = entry.first;
            for (;;) {
                auto& neighbors = remainingEdges.at(current);
                if (neighbors.empty()) {
                    if (path.size() != 1) {
                        dust3dDebug << "Intersection boundary did not close";
                        return false;
                    }
                    break;
                }
                size_t next = *neighbors.begin();
                neighbors.erase(neighbors.begin());
                remainingEdges.at(next).erase(current);
                auto repeated = pathIndices.find(next);
                if (repeated != pathIndices.end()) {
                    // Touching intersection loops share vertices, but each boundary edge belongs to one loop.
                    size_t loopStart = repeated->second;
                    result.emplace_back(path.begin() + loopStart, path.end());
                    for (size_t index = loopStart + 1; index < path.size(); ++index)
                        pathIndices.erase(path[index]);
                    path.resize(loopStart + 1);
                } else {
                    pathIndices.insert({ next, path.size() });
                    path.push_back(next);
                }
                current = next;
            }
        }
    }
    polygons.insert(polygons.end(), result.begin(), result.end());
    return true;
}

void SolidMeshBooleanOperation::buildFaceGroups(const std::vector<std::vector<size_t>>& intersections,
    const std::map<std::uint64_t, size_t>& halfEdges,
    const std::vector<std::vector<size_t>>& triangles,
    size_t remainingStartTriangleIndex,
    size_t remainingTriangleCount,
    std::vector<std::vector<size_t>>& triangleGroups)
{
    std::map<std::uint64_t, size_t> halfEdgeGroupMap;
    size_t groupIndex = 0;
    std::queue<std::pair<size_t, size_t>> waitQ;
    for (const auto& intersection : intersections) {
        for (size_t i = 0; i < intersection.size(); ++i) {
            size_t j = (i + 1) % intersection.size();
            {
                auto halfEdge = makeHalfEdgeKey(intersection[i], intersection[j]);
                halfEdgeGroupMap.insert({ halfEdge, groupIndex });
                auto halfEdgeIt = halfEdges.find(halfEdge);
                if (halfEdgeIt != halfEdges.end()) {
                    waitQ.push({ halfEdgeIt->second, groupIndex });
                }
            }
            {
                auto halfEdge = makeHalfEdgeKey(intersection[j], intersection[i]);
                halfEdgeGroupMap.insert({ halfEdge, groupIndex + 1 });
                auto halfEdgeIt = halfEdges.find(halfEdge);
                if (halfEdgeIt != halfEdges.end()) {
                    waitQ.push({ halfEdgeIt->second, groupIndex + 1 });
                }
            }
        }
        groupIndex += 2;
    }

    triangleGroups.resize(groupIndex);
    std::set<size_t> visitedTriangles;

    auto processQueue = [&](std::queue<std::pair<size_t, size_t>>& q) {
        while (!q.empty()) {
            auto triangleAndGroupIndex = q.front();
            q.pop();
            if (visitedTriangles.find(triangleAndGroupIndex.first) != visitedTriangles.end())
                continue;
            visitedTriangles.insert(triangleAndGroupIndex.first);
            triangleGroups[triangleAndGroupIndex.second].push_back(triangleAndGroupIndex.first);
            const auto& indicies = triangles[triangleAndGroupIndex.first];
            for (size_t i = 0; i < 3; ++i) {
                size_t j = (i + 1) % 3;
                auto halfEdge = makeHalfEdgeKey(indicies[i], indicies[j]);
                if (halfEdgeGroupMap.find(halfEdge) != halfEdgeGroupMap.end())
                    continue;
                halfEdgeGroupMap.insert({ halfEdge, triangleAndGroupIndex.second });
                auto halfEdgeIt = halfEdges.find(makeHalfEdgeKey(indicies[j], indicies[i]));
                if (halfEdgeIt != halfEdges.end()) {
                    q.push({ halfEdgeIt->second, triangleAndGroupIndex.second });
                } else {
                    //dust3dDebug << "HalfEdge not found:" << halfEdge.second << " " << halfEdge.first;
                }
            }
        }
    };

    processQueue(waitQ);

    size_t endIndex = remainingStartTriangleIndex + remainingTriangleCount;
    for (size_t triangleIndex = remainingStartTriangleIndex; triangleIndex < endIndex; ++triangleIndex) {
        if (visitedTriangles.find(triangleIndex) != visitedTriangles.end())
            continue;
        triangleGroups.push_back(std::vector<size_t>());
        waitQ.push({ triangleIndex, groupIndex });
        ++groupIndex;
        processQueue(waitQ);
    }
}

size_t SolidMeshBooleanOperation::addNewPoint(const Vector3& position, bool weldNearby)
{
    PositionKey key(position);
    // Truncated PositionKey buckets can split numerically identical cut/original vertices at a grid boundary.
    const double tolerance = weldNearby ? (m_preserveThinWalls ? intersectionTolerance(position) : 0.00001) : 0.0;
    const int neighborBuckets = weldNearby ? 1 : 0;
    size_t closestIndex = m_newVertices.size();
    double closestDistanceSquared = std::numeric_limits<double>::max();
    for (int x = -neighborBuckets; x <= neighborBuckets; ++x) {
        for (int y = -neighborBuckets; y <= neighborBuckets; ++y) {
            for (int z = -neighborBuckets; z <= neighborBuckets; ++z) {
                auto candidate = m_newPositionMap.find(PositionKey(position + Vector3(x * tolerance, y * tolerance, z * tolerance)));
                if (candidate == m_newPositionMap.end())
                    continue;
                // A zero-centered truncated bucket spans twice the resolution; every candidate still needs a distance check.
                for (size_t index : candidate->second) {
                    const auto& point = m_newVertices.at(index);
                    double dx = point.x() - position.x();
                    double dy = point.y() - position.y();
                    double dz = point.z() - position.z();
                    if (std::abs(dx) > tolerance || std::abs(dy) > tolerance || std::abs(dz) > tolerance)
                        continue;
                    double distanceSquared = dx * dx + dy * dy + dz * dz;
                    if (distanceSquared < closestDistanceSquared) {
                        closestDistanceSquared = distanceSquared;
                        closestIndex = index;
                    }
                }
            }
        }
    }
    if (closestIndex != m_newVertices.size()) {
        return closestIndex;
    }
    size_t index = m_newVertices.size();
    m_newPositionMap[key].push_back(index);
    m_newVertices.push_back(position);
    return index;
}

bool SolidMeshBooleanOperation::addUnintersectedTriangles(const SolidMesh* mesh,
    bool isFirst,
    const std::set<size_t>& usedFaces,
    std::vector<size_t>& vertexIndices,
    std::map<std::uint64_t, size_t>* halfEdges)
{
    const auto& vertices = *mesh->vertices();
    vertexIndices.reserve(vertices.size());
    // Preserve distinct vertices on hollow walls and use exact welding when no cuts are possible.
    const bool weldNearby = !m_preserveThinWalls && !m_potentialIntersectedPairs.empty();
    for (const auto& vertex : vertices)
        vertexIndices.push_back(addNewPoint(vertex, weldNearby));
    size_t triangleCount = mesh->triangles()->size();
    m_newTriangles.reserve(m_newTriangles.size() + triangleCount - usedFaces.size());
    for (size_t i = 0; i < triangleCount; ++i) {
        if (usedFaces.find(i) != usedFaces.end())
            continue;
        const auto& oldTriangle = (*mesh->triangles())[i];
        const std::vector<size_t> mappedTriangle { vertexIndices.at(oldTriangle[0]),
            vertexIndices.at(oldTriangle[1]), vertexIndices.at(oldTriangle[2]) };
        if (mappedTriangle[0] == mappedTriangle[1] || mappedTriangle[1] == mappedTriangle[2] || mappedTriangle[2] == mappedTriangle[0])
            continue;
        size_t newInsertedIndex = m_newTriangles.size();
        m_newTriangles.push_back(mappedTriangle);
        m_newTriangleSources.push_back({ isFirst, i });
        const auto& newInsertedTriangle = m_newTriangles.back();
        if (!halfEdges->insert({ makeHalfEdgeKey(newInsertedTriangle[0], newInsertedTriangle[1]), newInsertedIndex }).second) {
            dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[0] << "," << newInsertedTriangle[1];
            continue;
        }
        if (!halfEdges->insert({ makeHalfEdgeKey(newInsertedTriangle[1], newInsertedTriangle[2]), newInsertedIndex }).second) {
            dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[1] << "," << newInsertedTriangle[2];
            continue;
        }
        if (!halfEdges->insert({ makeHalfEdgeKey(newInsertedTriangle[2], newInsertedTriangle[0]), newInsertedIndex }).second) {
            dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[2] << "," << newInsertedTriangle[0];
            continue;
        }
    }
    return true;
}

namespace {

constexpr double capTolerance = 2e-11;
constexpr double floatBoundaryTolerance = 2 * std::numeric_limits<float>::epsilon();

// Bound extra work on a failed primary Boolean without altering the primary path.
bool chargeCapWork(size_t amount, size_t& remaining)
{
    if (amount > remaining)
        return false;
    remaining -= amount;
    return true;
}
using CapPolygon = std::vector<Vector3>;

struct CapPlane {
    Vector3 normal;
    double offset;
};

double distanceTo(const CapPlane& plane, const Vector3& point)
{
    return Vector3::dotProduct(plane.normal, point) - plane.offset;
}

bool convexPlanes(const std::vector<Vector3>& vertices, const std::vector<std::vector<size_t>>& faces,
    std::vector<CapPlane>& planes, size_t& remainingWork)
{
    for (const auto& face : faces) {
        if (!chargeCapWork(planes.size() + 1, remainingWork))
            return false;
        const auto& origin = vertices.at(face[0]);
        auto normal = Vector3::crossProduct(vertices.at(face[1]) - origin, vertices.at(face[2]) - origin);
        const double length = normal.length();
        if (!(length > 0) || !std::isfinite(length))
            return false;
        normal = normal / length;
        const double offset = Vector3::dotProduct(normal, origin);
        const auto equivalent = [&](const auto& plane) {
            return (plane.normal - normal).length() <= capTolerance && std::abs(plane.offset - offset) <= capTolerance;
        };
        if (std::none_of(planes.begin(), planes.end(), equivalent))
            planes.push_back({ normal, offset });
    }
    if (planes.size() < 4)
        return false;
    for (const auto& plane : planes) {
        if (!chargeCapWork(vertices.size(), remainingWork))
            return false;
        if (std::any_of(vertices.begin(), vertices.end(), [&](const auto& point) {
                return distanceTo(plane, point) > capTolerance;
            }))
            return false;
    }
    return true;
}

Vector3 capCenter(const CapPolygon& polygon)
{
    Vector3 center;
    for (const auto& point : polygon)
        center += point;
    return center / polygon.size();
}

CapPolygon clipCap(CapPolygon& inside, const CapPlane& plane, double boundaryTolerance)
{
    std::vector<double> distances;
    for (const auto& point : inside) {
        const double value = distanceTo(plane, point);
        distances.push_back(std::abs(value) <= boundaryTolerance ? 0 : value);
    }
    const auto [least, greatest] = std::minmax_element(distances.begin(), distances.end());
    if (*greatest <= 0)
        return {};
    if (*least >= 0)
        return std::exchange(inside, {});

    CapPolygon kept;
    CapPolygon outside;
    for (size_t i = 0; i < inside.size(); ++i) {
        const size_t j = (i + 1) % inside.size();
        const double a = distances[i];
        const double b = distances[j];
        if (a <= 0)
            kept.push_back(inside[i]);
        if (a >= 0)
            outside.push_back(inside[i]);
        if (a * b < 0) {
            const auto split = inside[i] + (inside[j] - inside[i]) * (a / (a - b));
            kept.push_back(split);
            outside.push_back(split);
        }
    }
    inside = std::move(kept);
    return outside;
}

bool occupiesCapSide(const std::vector<CapPlane>& planes, const Vector3& point, const Vector3& direction)
{
    return std::all_of(planes.begin(), planes.end(), [&](const auto& plane) {
        const double distance = distanceTo(plane, point);
        return distance <= capTolerance
            && (distance < -capTolerance || Vector3::dotProduct(plane.normal, direction) <= capTolerance);
    });
}

std::optional<bool> insideClippedOperand(const Vector3& point, const std::vector<Vector3>& vertices,
    const std::vector<std::vector<size_t>>& faces, const std::vector<CapPlane>& planes, size_t& remainingWork)
{
    for (int y = 1; y <= 8; ++y) {
        for (int z = 1; z <= 8; ++z) {
            if (!chargeCapWork(faces.size(), remainingWork))
                return std::nullopt;
            const Vector3 ray(1, y, z);
            size_t hits = 0;
            bool ambiguous = false;
            for (size_t i = 0; i < faces.size(); ++i) {
                const auto& plane = planes[i];
                const double projection = Vector3::dotProduct(plane.normal, ray);
                if (std::abs(projection) <= capTolerance)
                    continue;
                const double distance = -distanceTo(plane, point) / projection;
                if (distance <= capTolerance)
                    continue;
                const auto& face = faces[i];
                const auto& origin = vertices[face[0]];
                const auto u = vertices[face[1]] - origin;
                const auto v = vertices[face[2]] - origin;
                const auto w = (point - origin) + ray * distance;
                const double area = Vector3::dotProduct(Vector3::crossProduct(u, v), plane.normal);
                const double s = Vector3::dotProduct(Vector3::crossProduct(w, v), plane.normal) / area;
                const double t = Vector3::dotProduct(Vector3::crossProduct(u, w), plane.normal) / area;
                if (s < -capTolerance || t < -capTolerance || s + t > 1 + capTolerance)
                    continue;
                if (s <= capTolerance || t <= capTolerance || s + t >= 1 - capTolerance) {
                    ambiguous = true;
                    break;
                }
                ++hits;
            }
            if (!ambiguous)
                return hits % 2 != 0;
        }
    }
    return std::nullopt;
}

std::optional<std::pair<bool, bool>> clippedOccupancy(const Vector3& point, const Vector3& normal,
    const std::vector<Vector3>& vertices, const std::vector<std::vector<size_t>>& faces,
    const std::vector<CapPlane>& planes, size_t& remainingWork)
{
    if (!chargeCapWork(planes.size(), remainingWork))
        return std::nullopt;
    double step = 1;
    for (const auto& plane : planes) {
        const double distance = distanceTo(plane, point);
        const double projection = Vector3::dotProduct(plane.normal, normal);
        if (std::abs(distance) > capTolerance && std::abs(projection) > capTolerance)
            step = std::min(step, std::abs(distance / projection));
    }
    step *= 0.5;
    if (step <= 8 * capTolerance)
        return std::nullopt;
    const auto below = insideClippedOperand(point - normal * step, vertices, faces, planes, remainingWork);
    const auto above = insideClippedOperand(point + normal * step, vertices, faces, planes, remainingWork);
    if (!below || !above)
        return std::nullopt;
    return std::pair { *below, *above };
}

std::array<bool, 3> capOccupancy(bool first, bool second)
{
    return { first || second, first && !second, first && second };
}

struct CapFragment {
    CapPolygon polygon;
    SolidMeshBooleanOperation::TriangleSource source;
};

bool triangulateCaps(const std::vector<CapFragment>& fragments, std::vector<Vector3>& vertices,
    std::vector<std::vector<size_t>>& triangles, std::vector<SolidMeshBooleanOperation::TriangleSource>& sources,
    size_t& remainingWork, double boundaryTolerance)
{
    const double squaredTolerance = boundaryTolerance * boundaryTolerance;
    const size_t offset = vertices.size();
    std::vector<std::vector<size_t>> polygons;
    std::multimap<double, size_t> pointIndices;
    for (const auto& fragment : fragments) {
        if (!chargeCapWork(fragment.polygon.size(), remainingWork))
            return false;
        auto& polygon = polygons.emplace_back();
        for (const auto& point : fragment.polygon) {
            auto begin = pointIndices.lower_bound(point.x() - boundaryTolerance);
            auto end = pointIndices.upper_bound(point.x() + boundaryTolerance);
            size_t index = vertices.size();
            for (auto match = begin; match != end; ++match) {
                if (!chargeCapWork(1, remainingWork))
                    return false;
                if ((vertices[match->second] - point).lengthSquared() <= squaredTolerance)
                    index = std::min(index, match->second);
            }
            if (index == vertices.size()) {
                vertices.push_back(point);
                pointIndices.emplace(point.x(), index);
            }
            if (polygon.empty() || polygon.back() != index)
                polygon.push_back(index);
        }
        if (polygon.size() > 1 && polygon.front() == polygon.back())
            polygon.pop_back();
    }

    const size_t boundaryEnd = vertices.size();
    std::array<std::vector<size_t>, 3> byAxis;
    for (size_t axis = 0; axis < byAxis.size(); ++axis) {
        auto& sorted = byAxis[axis];
        sorted.resize(boundaryEnd - offset);
        std::iota(sorted.begin(), sorted.end(), offset);
        std::sort(sorted.begin(), sorted.end(), [&](size_t a, size_t b) {
            return vertices[a][axis] < vertices[b][axis];
        });
    }
    auto emit = [&](const std::vector<size_t>& triangle, const auto& source) {
        const auto& a = vertices[triangle[0]];
        const auto& b = vertices[triangle[1]];
        const auto& c = vertices[triangle[2]];
        if (Vector3::crossProduct(b - a, c - a).lengthSquared() <= 0)
            return false;
        triangles.push_back(triangle);
        sources.push_back(source);
        return true;
    };
    for (size_t index = 0; index < polygons.size(); ++index) {
        const auto& polygon = polygons[index];
        if (polygon.size() < 3)
            continue;
        std::vector<size_t> boundary;
        for (size_t i = 0; i < polygon.size(); ++i) {
            const auto a = polygon[i];
            const auto b = polygon[(i + 1) % polygon.size()];
            const auto line = vertices[b] - vertices[a];
            const double length = line.length();
            if (length <= boundaryTolerance)
                return false;
            const auto direction = line / length;
            std::vector<std::pair<double, size_t>> onEdge { { 0, a } };
            size_t axis = 0;
            for (size_t candidate = 1; candidate < byAxis.size(); ++candidate) {
                if (std::abs(line[candidate]) > std::abs(line[axis]))
                    axis = candidate;
            }
            const auto& sorted = byAxis[axis];
            const double minimum = std::min(vertices[a][axis], vertices[b][axis]) - boundaryTolerance;
            const double maximum = std::max(vertices[a][axis], vertices[b][axis]) + boundaryTolerance;
            const auto begin = std::lower_bound(sorted.begin(), sorted.end(), minimum, [&](size_t index, double value) {
                return vertices[index][axis] < value;
            });
            const auto end = std::upper_bound(begin, sorted.end(), maximum, [&](double value, size_t index) {
                return value < vertices[index][axis];
            });
            if (!chargeCapWork(static_cast<size_t>(end - begin), remainingWork))
                return false;
            for (auto candidate = begin; candidate != end; ++candidate) {
                const auto point = *candidate;
                if (point == a || point == b)
                    continue;
                const auto delta = vertices[point] - vertices[a];
                const double along = Vector3::dotProduct(delta, direction);
                if (along > boundaryTolerance && along < length - boundaryTolerance
                    && Vector3::crossProduct(delta, direction).lengthSquared() <= squaredTolerance)
                    onEdge.emplace_back(along, point);
            }
            std::sort(onEdge.begin(), onEdge.end());
            for (const auto& point : onEdge)
                boundary.push_back(point.second);
        }
        const auto& source = fragments[index].source;
        if (boundary.size() == 3) {
            if (!emit(boundary, source))
                return false;
            continue;
        }
        Vector3 center;
        for (const auto vertex : polygon)
            center += vertices[vertex];
        const size_t centerIndex = vertices.size();
        vertices.push_back(center / polygon.size());
        for (size_t i = 0; i < boundary.size(); ++i) {
            if (!emit({ centerIndex, boundary[i], boundary[(i + 1) % boundary.size()] }, source))
                return false;
        }
    }
    return MeshState::isWatertight(triangles);
}

}

bool SolidMeshBooleanOperation::combineClippedFaces(bool convex, double boundaryTolerance)
{
    size_t remainingWork = 4'000'000;
    const std::array<const SolidMesh*, 2> meshes { m_firstMesh, m_secondMesh };
    for (const auto* mesh : meshes) {
        const auto& faces = *mesh->triangles();
        if (!chargeCapWork(faces.size(), remainingWork)
            || !chargeCapWork(mesh->vertices()->size(), remainingWork)
            || mesh->vertices()->empty() || faces.empty()
            || std::any_of(faces.begin(), faces.end(), [](const auto& face) { return face.size() != 3; })
            || !MeshState::isWatertight(faces))
            return false;
    }
    auto origin = meshes[0]->vertices()->front();
    auto maximum = origin;
    for (const auto* mesh : meshes) {
        for (const auto& point : *mesh->vertices()) {
            if (!std::isfinite(point.x()) || !std::isfinite(point.y()) || !std::isfinite(point.z()))
                return false;
            for (size_t axis = 0; axis < 3; ++axis) {
                origin[axis] = std::min(origin[axis], point[axis]);
                maximum[axis] = std::max(maximum[axis], point[axis]);
            }
        }
    }
    const auto span = maximum - origin;
    const double scale = std::max({ span.x(), span.y(), span.z() });
    if (!(scale > 0) || !std::isfinite(scale))
        return false;

    std::array<std::vector<Vector3>, 2> positions;
    std::array<std::vector<CapPlane>, 2> planes;
    for (size_t owner = 0; owner < meshes.size(); ++owner) {
        for (const auto& point : *meshes[owner]->vertices())
            positions[owner].push_back((point - origin) / scale);
        if (convex) {
            if (!convexPlanes(positions[owner], *meshes[owner]->triangles(), planes[owner], remainingWork))
                return false;
        } else {
            for (const auto& face : *meshes[owner]->triangles()) {
                const auto& origin = positions[owner][face[0]];
                auto normal = Vector3::crossProduct(positions[owner][face[1]] - origin, positions[owner][face[2]] - origin);
                const double length = normal.length();
                if (!(length > 0) || !std::isfinite(length))
                    return false;
                normal = normal / length;
                planes[owner].push_back({ normal, Vector3::dotProduct(normal, origin) });
            }
        }
    }

    std::array<std::vector<std::vector<size_t>>, 2> candidates;
    if (!convex) {
        candidates[0].resize(meshes[0]->triangles()->size());
        candidates[1].resize(meshes[1]->triangles()->size());
        if (!chargeCapWork(m_potentialIntersectedPairs.size(), remainingWork))
            return false;
        for (const auto& [first, second] : m_potentialIntersectedPairs) {
            candidates[0][first].push_back(second);
            candidates[1][second].push_back(first);
        }
    }

    std::array<std::vector<CapFragment>, 3> fragments;
    for (size_t owner = 0; owner < meshes.size(); ++owner) {
        const auto& faces = *meshes[owner]->triangles();
        const auto& otherPlanes = planes[1 - owner];
        for (size_t index = 0; index < faces.size(); ++index) {
            const auto& face = faces[index];
            const CapPolygon triangle { positions[owner][face[0]], positions[owner][face[1]], positions[owner][face[2]] };
            auto normal = Vector3::crossProduct(triangle[1] - triangle[0], triangle[2] - triangle[0]);
            normal = normal / normal.length();
            std::vector<CapPolygon> polygons;
            if (convex) {
                auto covered = triangle;
                for (const auto& plane : otherPlanes) {
                    if (!chargeCapWork(covered.size(), remainingWork))
                        return false;
                    auto outside = clipCap(covered, plane, boundaryTolerance);
                    if (!outside.empty())
                        polygons.push_back(std::move(outside));
                    if (covered.empty())
                        break;
                }
                if (covered.empty())
                    polygons = { triangle };
                else
                    polygons.push_back(std::move(covered));
            } else {
                polygons = { triangle };
                for (const auto candidate : candidates[owner][index]) {
                    const size_t count = polygons.size();
                    for (size_t p = 0; p < count; ++p) {
                        if (!chargeCapWork(polygons[p].size(), remainingWork))
                            return false;
                        auto outside = clipCap(polygons[p], otherPlanes[candidate], boundaryTolerance);
                        if (polygons[p].empty())
                            polygons[p] = std::move(outside);
                        else if (!outside.empty())
                            polygons.push_back(std::move(outside));
                    }
                }
            }

            for (const auto& polygon : polygons) {
                if (!chargeCapWork(polygon.size() + 2 * otherPlanes.size(), remainingWork))
                    return false;
                const auto center = capCenter(polygon);
                std::pair<bool, bool> occupancy;
                if (convex) {
                    occupancy = { occupiesCapSide(otherPlanes, center, -normal), occupiesCapSide(otherPlanes, center, normal) };
                } else {
                    const auto classified = clippedOccupancy(center, normal, positions[1 - owner],
                        *meshes[1 - owner]->triangles(), otherPlanes, remainingWork);
                    if (!classified)
                        return false;
                    occupancy = *classified;
                }
                const auto [otherBelow, otherAbove] = occupancy;
                if (owner == 1 && otherBelow != otherAbove)
                    continue;
                const auto below = owner == 0 ? capOccupancy(true, otherBelow) : capOccupancy(otherBelow, true);
                const auto above = owner == 0 ? capOccupancy(false, otherAbove) : capOccupancy(otherAbove, false);
                for (size_t mode = 0; mode < fragments.size(); ++mode) {
                    if (below[mode] == above[mode])
                        continue;
                    auto& fragment = fragments[mode].emplace_back(CapFragment { polygon, { owner == 0, index, !below[mode] } });
                    if (!below[mode])
                        std::reverse(fragment.polygon.begin(), fragment.polygon.end());
                }
            }
        }
    }

    std::vector<Vector3> vertices;
    std::array<std::vector<std::vector<size_t>>, 3> triangles;
    std::array<std::vector<TriangleSource>, 3> sources;
    for (size_t mode = 0; mode < fragments.size(); ++mode) {
        if (!triangulateCaps(fragments[mode], vertices, triangles[mode], sources[mode], remainingWork, boundaryTolerance))
            return false;
    }
    if (triangles[0].empty())
        return false;
    for (auto& point : vertices)
        point = origin + point * scale;
    m_newVertices = std::move(vertices);
    m_capTriangles = std::move(triangles);
    m_capSources = std::move(sources);
    return true;
}

bool SolidMeshBooleanOperation::combine()
{
    searchPotentialIntersectedPairs();

    struct IntersectedContext {
        std::vector<Vector3> points;
        std::multimap<double, size_t> pointIndicesByX;
        std::map<PositionKey, size_t> ordinaryPointIndices;
        std::map<size_t, std::set<size_t>> neighborMap;
    };

    std::map<size_t, IntersectedContext> firstTriangleIntersectedContext;
    std::map<size_t, IntersectedContext> secondTriangleIntersectedContext;

    auto addIntersectedPoint = [&](IntersectedContext& context, const Vector3& position) {
        if (!m_preserveThinWalls) {
            const auto inserted = context.ordinaryPointIndices.emplace(PositionKey(position), context.points.size());
            if (inserted.second)
                context.points.push_back(position);
            return inserted.first->second;
        }
        const double tolerance = intersectionTolerance(position);
        const auto begin = context.pointIndicesByX.lower_bound(position.x() - tolerance);
        const auto end = context.pointIndicesByX.upper_bound(position.x() + tolerance);
        size_t matched = context.points.size();
        for (auto candidate = begin; candidate != end; ++candidate) {
            if ((position - context.points[candidate->second]).lengthSquared() <= tolerance * tolerance)
                matched = std::min(matched, candidate->second);
        }
        if (matched == context.points.size()) {
            context.points.push_back(position);
            context.pointIndicesByX.emplace(position.x(), matched);
        }
        return matched;
    };

    for (const auto& pair : m_potentialIntersectedPairs) {
        std::pair<Vector3, Vector3> newEdge;
        if (intersectTwoFaces(pair.first, pair.second, newEdge)) {
            m_firstIntersectedFaces.insert(pair.first);
            m_secondIntersectedFaces.insert(pair.second);

            {
                auto& context = firstTriangleIntersectedContext[pair.first];
                size_t firstPointIndex = 3 + addIntersectedPoint(context, newEdge.first);
                size_t secondPointIndex = 3 + addIntersectedPoint(context, newEdge.second);
                if (firstPointIndex != secondPointIndex) {
                    context.neighborMap[firstPointIndex].insert(secondPointIndex);
                    context.neighborMap[secondPointIndex].insert(firstPointIndex);
                }
            }

            {
                auto& context = secondTriangleIntersectedContext[pair.second];
                size_t firstPointIndex = 3 + addIntersectedPoint(context, newEdge.first);
                size_t secondPointIndex = 3 + addIntersectedPoint(context, newEdge.second);
                if (firstPointIndex != secondPointIndex) {
                    context.neighborMap[firstPointIndex].insert(secondPointIndex);
                    context.neighborMap[secondPointIndex].insert(firstPointIndex);
                }
            }
        }
    }

    std::map<size_t, std::set<size_t>> firstEdges;
    std::map<size_t, std::set<size_t>> secondEdges;
    std::vector<std::vector<size_t>> firstIntersections;
    std::vector<std::vector<size_t>> secondIntersections;
    std::map<std::uint64_t, size_t> firstHalfEdges;
    std::map<std::uint64_t, size_t> secondHalfEdges;

    auto reTriangulate = [&](const std::map<size_t, IntersectedContext>& context,
                             const SolidMesh* mesh,
                             bool isFirst,
                             const std::vector<size_t>& vertexIndices,
                             std::map<size_t, std::set<size_t>>& edges,
                             std::map<std::uint64_t, size_t>& halfEdges) {
        for (const auto& [contextKey, it] : context) {
            const auto& triangle = (*mesh->triangles())[contextKey];
            ReTriangulator reTriangulator({ (*mesh->vertices())[triangle[0]],
                                              (*mesh->vertices())[triangle[1]],
                                              (*mesh->vertices())[triangle[2]] },
                (*mesh->triangleNormals())[contextKey]);
            reTriangulator.setEdges(it.points,
                &it.neighborMap);
            if (!reTriangulator.reTriangulate(m_preserveThinWalls)) {
                dust3dDebug << "Retriangle failed";
                return false;
            }
            std::vector<size_t> newIndices;
            newIndices.reserve(3 + it.points.size());
            newIndices.push_back(vertexIndices.at(triangle[0]));
            newIndices.push_back(vertexIndices.at(triangle[1]));
            newIndices.push_back(vertexIndices.at(triangle[2]));
            for (const auto& point : it.points)
                newIndices.push_back(addNewPoint(point));
            for (const auto& triangle : reTriangulator.triangles()) {
                const std::vector<size_t> mappedTriangle { newIndices.at(triangle[0]),
                    newIndices.at(triangle[1]), newIndices.at(triangle[2]) };
                if (mappedTriangle[0] == mappedTriangle[1] || mappedTriangle[1] == mappedTriangle[2] || mappedTriangle[2] == mappedTriangle[0])
                    continue;
                size_t newInsertedIndex = m_newTriangles.size();
                m_newTriangles.push_back(mappedTriangle);
                m_newTriangleSources.push_back({ isFirst, contextKey });
                const auto& newInsertedTriangle = m_newTriangles.back();
                if (!halfEdges.insert({ makeHalfEdgeKey(newInsertedTriangle[0], newInsertedTriangle[1]), newInsertedIndex }).second) {
                    dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[0] << "," << newInsertedTriangle[1];
                }
                if (!halfEdges.insert({ makeHalfEdgeKey(newInsertedTriangle[1], newInsertedTriangle[2]), newInsertedIndex }).second) {
                    dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[1] << "," << newInsertedTriangle[2];
                }
                if (!halfEdges.insert({ makeHalfEdgeKey(newInsertedTriangle[2], newInsertedTriangle[0]), newInsertedIndex }).second) {
                    dust3dDebug << "Found repeated halfedge:" << newInsertedTriangle[2] << "," << newInsertedTriangle[0];
                }
            }
            for (const auto& neighborIt : it.neighborMap) {
                auto from = newIndices[neighborIt.first];
                for (const auto& neighborIt2 : neighborIt.second) {
                    auto to = newIndices[neighborIt2];
                    if (from == to)
                        continue;
                    edges[from].insert(to);
                    edges[to].insert(from);
                }
            }
        }
        return true;
    };

    std::vector<size_t> firstVertexIndices;
    std::vector<size_t> secondVertexIndices;
    size_t firstRemainingStartTriangleIndex = m_newTriangles.size();
    if (!addUnintersectedTriangles(m_firstMesh, true, m_firstIntersectedFaces, firstVertexIndices, &firstHalfEdges)) {
        dust3dDebug << "Add first mesh remaining triangles failed";
    }
    size_t firstRemainingTriangleCount = m_newTriangles.size() - firstRemainingStartTriangleIndex;

    size_t secondRemainingStartTriangleIndex = m_newTriangles.size();
    if (!addUnintersectedTriangles(m_secondMesh, false, m_secondIntersectedFaces, secondVertexIndices, &secondHalfEdges)) {
        dust3dDebug << "Add second mesh remaining triangles failed";
    }
    size_t secondRemainingTriangleCount = m_newTriangles.size() - secondRemainingStartTriangleIndex;

    if (!reTriangulate(firstTriangleIntersectedContext,
            m_firstMesh, true, firstVertexIndices, firstEdges, firstHalfEdges)) {
        dust3dDebug << "Retriangulate first mesh failed";
        return false;
    }
    if (!reTriangulate(secondTriangleIntersectedContext,
            m_secondMesh, false, secondVertexIndices, secondEdges, secondHalfEdges)) {
        dust3dDebug << "Retriangulate second mesh failed";
        return false;
    }

    if (!buildPolygonsFromEdges(firstEdges, firstIntersections)) {
        if ((m_hasCoplanarContact && combineClippedFaces(true, capTolerance)) || combineClippedFaces(false, capTolerance))
            return true;
        dust3dDebug << "Build polygons from edges failed";
        return false;
    }

    buildFaceGroups(firstIntersections,
        firstHalfEdges,
        m_newTriangles,
        firstRemainingStartTriangleIndex,
        firstRemainingTriangleCount,
        m_firstTriangleGroups);
    buildFaceGroups(firstIntersections,
        secondHalfEdges,
        m_newTriangles,
        secondRemainingStartTriangleIndex,
        secondRemainingTriangleCount,
        m_secondTriangleGroups);

    decideGroupSide(m_firstTriangleGroups,
        m_secondMesh,
        m_secondMesh->axisAlignedBoundingBoxTree(),
        m_firstGroupSides);
    decideGroupSide(m_secondTriangleGroups,
        m_firstMesh,
        m_firstMesh->axisAlignedBoundingBoxTree(),
        m_secondGroupSides);

    if (!firstEdges.empty()) {
        std::vector<std::vector<size_t>> triangles;
        fetchUnion(triangles);
        bool valid = MeshState::isWatertight(triangles);
        triangles.clear();
        fetchDiff(triangles);
        valid = valid && MeshState::isWatertight(triangles);
        triangles.clear();
        fetchIntersect(triangles);
        valid = valid && MeshState::isWatertight(triangles);
        if (!valid) {
            // Generated solid operands can carry float-scale offsets at a shared seam.
            // Keep thin-wall geometry on the precise path and require closed clipped results.
            if (m_preserveThinWalls || !combineClippedFaces(false, floatBoundaryTolerance))
                combineClippedFaces(false, capTolerance);
        }
    }
    return true;
}

void SolidMeshBooleanOperation::decideGroupSide(const std::vector<std::vector<size_t>>& groups,
    const SolidMesh* mesh,
    const AxisAlignedBoudingBoxTree* tree,
    std::vector<bool>& groupSides)
{
    groupSides.resize(groups.size());
    for (size_t i = 0; i < groups.size(); ++i) {
        const auto& group = groups[i];
        if (group.empty())
            continue;
        size_t insideCount = 0;
        size_t totalCount = 0;
        for (size_t pickIndex = 0; pickIndex < 1 && pickIndex < group.size(); ++pickIndex) {
            for (size_t axisIndex = 0; axisIndex < g_testAxisList.size(); ++axisIndex) {
                const auto& pickedTriangle = m_newTriangles[group[pickIndex]];
                bool inside = isPointInMesh((m_newVertices[pickedTriangle[0]] + m_newVertices[pickedTriangle[1]] + m_newVertices[pickedTriangle[2]]) / 3.0,
                    mesh,
                    tree,
                    axisIndex);
                if (inside)
                    ++insideCount;
                ++totalCount;
            }
        }
        groupSides[i] = (float)insideCount / totalCount > 0.5;
    }
}

void SolidMeshBooleanOperation::fetchUnion(std::vector<std::vector<size_t>>& resultTriangles,
    std::vector<TriangleSource>* sources)
{
    if (!m_capTriangles[0].empty()) {
        const auto& triangles = m_capTriangles[0];
        resultTriangles.insert(resultTriangles.end(), triangles.begin(), triangles.end());
        if (sources) {
            const auto& records = m_capSources[0];
            sources->insert(sources->end(), records.begin(), records.end());
        }
        return;
    }
    for (size_t i = 0; i < m_firstGroupSides.size(); ++i) {
        if (m_firstGroupSides[i])
            continue;
        for (const auto& it : m_firstTriangleGroups[i]) {
            resultTriangles.push_back(m_newTriangles[it]);
            if (sources)
                sources->push_back(m_newTriangleSources.at(it));
        }
    }

    for (size_t i = 0; i < m_secondGroupSides.size(); ++i) {
        if (m_secondGroupSides[i])
            continue;
        for (const auto& it : m_secondTriangleGroups[i]) {
            resultTriangles.push_back(m_newTriangles[it]);
            if (sources)
                sources->push_back(m_newTriangleSources.at(it));
        }
    }
}

void SolidMeshBooleanOperation::fetchDiff(std::vector<std::vector<size_t>>& resultTriangles,
    std::vector<TriangleSource>* sources)
{
    if (!m_capTriangles[0].empty()) {
        const auto& triangles = m_capTriangles[1];
        resultTriangles.insert(resultTriangles.end(), triangles.begin(), triangles.end());
        if (sources) {
            const auto& records = m_capSources[1];
            sources->insert(sources->end(), records.begin(), records.end());
        }
        return;
    }
    for (size_t i = 0; i < m_firstGroupSides.size(); ++i) {
        if (m_firstGroupSides[i])
            continue;
        for (const auto& it : m_firstTriangleGroups[i]) {
            resultTriangles.push_back(m_newTriangles[it]);
            if (sources)
                sources->push_back(m_newTriangleSources.at(it));
        }
    }

    for (size_t i = 0; i < m_secondGroupSides.size(); ++i) {
        if (!m_secondGroupSides[i])
            continue;
        for (const auto& it : m_secondTriangleGroups[i]) {
            auto triangle = m_newTriangles[it];
            resultTriangles.push_back({ triangle[2], triangle[1], triangle[0] });
            if (sources) {
                auto source = m_newTriangleSources.at(it);
                source.isReversed = true;
                sources->push_back(source);
            }
        }
    }
}

void SolidMeshBooleanOperation::fetchIntersect(std::vector<std::vector<size_t>>& resultTriangles,
    std::vector<TriangleSource>* sources)
{
    if (!m_capTriangles[0].empty()) {
        const auto& triangles = m_capTriangles[2];
        resultTriangles.insert(resultTriangles.end(), triangles.begin(), triangles.end());
        if (sources) {
            const auto& records = m_capSources[2];
            sources->insert(sources->end(), records.begin(), records.end());
        }
        return;
    }
    for (size_t i = 0; i < m_firstGroupSides.size(); ++i) {
        if (!m_firstGroupSides[i])
            continue;
        for (const auto& it : m_firstTriangleGroups[i]) {
            resultTriangles.push_back(m_newTriangles[it]);
            if (sources)
                sources->push_back(m_newTriangleSources.at(it));
        }
    }

    for (size_t i = 0; i < m_secondGroupSides.size(); ++i) {
        if (!m_secondGroupSides[i])
            continue;
        for (const auto& it : m_secondTriangleGroups[i]) {
            resultTriangles.push_back(m_newTriangles[it]);
            if (sources)
                sources->push_back(m_newTriangleSources.at(it));
        }
    }
}

const std::vector<Vector3>& SolidMeshBooleanOperation::resultVertices()
{
    return m_newVertices;
}

}

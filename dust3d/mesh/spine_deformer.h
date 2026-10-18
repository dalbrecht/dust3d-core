/*
 *  Copyright (c) 2016-2026 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved.
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

#ifndef DUST3D_MESH_SPINE_DEFORMER_H_
#define DUST3D_MESH_SPINE_DEFORMER_H_

#include <algorithm>
#include <cmath>
#include <dust3d/base/math.h>
#include <dust3d/base/vector3.h>
#include <dust3d/mesh/base_normal.h>
#include <dust3d/mesh/mesh_node.h>
#include <vector>

namespace dust3d {

// Sweeps a source mesh along a tube spine. The source mesh is treated as a
// straight reference shape whose primary axis is Y: every source vertex's Y
// position selects where along the spine it lands, while its X/Z offset from
// the source centre becomes the cross-section offset perpendicular to the
// spine. Cross-sections are scaled to the interpolated node radius, squashed by
// deformWidth (X) and deformThickness (Z), and spun by the cut rotation.
//
// This is the transformation used by the ImportedModel part target to sweep an
// imported mesh along the part's tube spine. Circular topology includes the
// closing segment and interpolates periodic node frames; the default preserves
// existing open-spine behavior.
class SpineDeformer {
public:
    SpineDeformer(const std::vector<MeshNode>& meshNodes,
        const Vector3& sourceMin, const Vector3& sourceMax,
        float deformWidth, float deformThickness, float cutRotation, bool isCircle = false)
        : m_meshNodes(meshNodes)
        , m_isCircle(isCircle)
        , m_deformWidth(deformWidth)
        , m_deformThickness(deformThickness)
        , m_cutRotation(cutRotation)
    {
        m_sourceMin = sourceMin;
        m_sourceCenter = (sourceMin + sourceMax) * 0.5;
        Vector3 sourceSize = sourceMax - sourceMin;

        // Use Y as the primary axis of the source model (spine direction).
        m_sourceLength = sourceSize.y();
        if (m_sourceLength < 0.0001)
            m_sourceLength = 0.0001;

        m_crossSize = std::max(sourceSize.x(), sourceSize.z());
        if (m_crossSize < 0.0001)
            m_crossSize = 0.0001;

        // Cumulative spine distance at each node.
        m_spineDistances.assign(meshNodes.size() + (m_isCircle && !meshNodes.empty() ? 1 : 0), 0.0);
        for (size_t i = 1; i < m_spineDistances.size(); ++i)
            m_spineDistances[i] = m_spineDistances[i - 1] + (meshNodes[i % meshNodes.size()].origin - meshNodes[i - 1].origin).length();
        m_totalSpineLength = meshNodes.empty() ? 0.0 : m_spineDistances.back();
        if (m_totalSpineLength < 0.0001)
            m_totalSpineLength = 0.0001;

        // Forward direction at each node.
        m_spineForwards.assign(meshNodes.size(), Vector3(0, 1, 0));
        for (size_t i = 0; i + 1 < meshNodes.size(); ++i)
            m_spineForwards[i] = (meshNodes[i + 1].origin - meshNodes[i].origin).normalized();
        if (meshNodes.size() > 1)
            m_spineForwards.back() = m_isCircle ? (meshNodes.front().origin - meshNodes.back().origin).normalized()
                                                : m_spineForwards[meshNodes.size() - 2];
        else if (!meshNodes.empty())
            m_spineForwards[0] = Vector3(0, 1, 0);

        // Periodic node frames prevent a circular cross-section seam; segment
        // directions remain separate for the centreline's Jacobian derivative.
        if (m_isCircle) {
            m_nodeForwards.resize(meshNodes.size());
            for (size_t i = 0; i < meshNodes.size(); ++i) {
                m_nodeForwards[i] = (m_spineForwards[(i + meshNodes.size() - 1) % meshNodes.size()] + m_spineForwards[i]).normalized();
                if (m_nodeForwards[i].isZero())
                    m_nodeForwards[i] = m_spineForwards[i];
            }
        }

        // A single global base normal for the whole spine, computed the same way
        // as TubeMeshBuilder. Deriving every cross-section frame from this
        // consistent reference (instead of a per-segment up-vector guess) keeps
        // the swept cross-sections from twisting or flipping as the spine bends.
        std::vector<Vector3> positions(meshNodes.size());
        for (size_t i = 0; i < meshNodes.size(); ++i)
            positions[i] = meshNodes[i].origin;
        m_baseNormal = m_isCircle ? BaseNormal::calculateCircleBaseNormal(positions) : BaseNormal::calculateTubeBaseNormal(positions);
        if (m_baseNormal.isZero())
            m_baseNormal = Vector3(1, 0, 0);
    }

    // Map a source vertex onto the spine.
    Vector3 deformVertex(const Vector3& sourceVertex) const
    {
        // Normalize the source position along the primary axis to [0, 1].
        double t = (sourceVertex.y() - m_sourceMin.y()) / m_sourceLength;
        t = std::max(0.0, std::min(1.0, t));

        // Local offset in the cross-section (relative to the source centre).
        double localU = sourceVertex.x() - m_sourceCenter.x();
        double localV = sourceVertex.z() - m_sourceCenter.z();

        double targetDist = t * m_totalSpineLength;
        size_t segIdx = segmentForDistance(targetDist);
        const size_t nextNode = (segIdx + 1) % m_meshNodes.size();

        double segLen = (m_meshNodes.size() >= 2) ? (m_spineDistances[segIdx + 1] - m_spineDistances[segIdx]) : 0.0;
        double segT = (segLen > 0.0001) ? (targetDist - m_spineDistances[segIdx]) / segLen : 0.0;

        Vector3 spinePos = (m_meshNodes.size() >= 2)
            ? m_meshNodes[segIdx].origin * (1.0 - segT) + m_meshNodes[nextNode].origin * segT
            : m_meshNodes[0].origin;
        double radius = (m_meshNodes.size() >= 2)
            ? m_meshNodes[segIdx].radius * (1.0 - segT) + m_meshNodes[nextNode].radius * segT
            : m_meshNodes[0].radius;

        Vector3 right;
        Vector3 realUp;
        crossSectionBasis(segIdx, segT, &right, &realUp);

        // Apply the cut rotation to the cross-section basis.
        double rotationAngle = m_cutRotation * Math::Pi;
        double cosR = std::cos(rotationAngle);
        double sinR = std::sin(rotationAngle);
        Vector3 rotatedRight = right * cosR + realUp * sinR;
        Vector3 rotatedUp = realUp * cosR - right * sinR;

        // Scale the cross-section by the node radius relative to the source size.
        double scale = (radius * 2.0) / m_crossSize;
        // Width may be zero while the independent thickness remains nonzero.
        double nodeWidth = m_meshNodes.size() >= 2
            ? m_meshNodes[segIdx].deformWidth * (1.0 - segT) + m_meshNodes[nextNode].deformWidth * segT
            : m_meshNodes[0].deformWidth;
        double nodeThickness = m_meshNodes.size() >= 2
            ? m_meshNodes[segIdx].deformThickness * (1.0 - segT) + m_meshNodes[nextNode].deformThickness * segT
            : m_meshNodes[0].deformThickness;
        double widthScale = scale * m_deformWidth * nodeWidth;
        double thicknessScale = scale * m_deformThickness * nodeThickness;

        return spinePos + rotatedRight * (localU * widthScale) + rotatedUp * (localV * thicknessScale);
    }

    // Apply the inverse transpose of the local vertex-deformation Jacobian.
    // Singular transforms return zero so callers can retain geometry-derived normals.
    Vector3 deformNormal(const Vector3& sourceNormal, const Vector3& sourceVertex) const
    {
        if (m_meshNodes.size() < 2)
            return Vector3();
        double t = (sourceVertex.y() - m_sourceMin.y()) / m_sourceLength;
        if (!std::isfinite(t) || t < 0 || t > 1)
            return Vector3();
        t = std::max(0.0, std::min(1.0, t));
        double targetDist = t * m_totalSpineLength;
        size_t segIdx = segmentForDistance(targetDist);
        const size_t nextNode = (segIdx + 1) % m_meshNodes.size();
        double segLen = m_spineDistances[segIdx + 1] - m_spineDistances[segIdx];
        if (segLen <= 0.0001)
            return Vector3();
        double segT = (targetDist - m_spineDistances[segIdx]) / segLen;
        double radius = m_meshNodes[segIdx].radius * (1.0 - segT) + m_meshNodes[nextNode].radius * segT;
        Vector3 right;
        Vector3 realUp;
        Vector3 rightDerivative;
        Vector3 upDerivative;
        crossSectionBasis(segIdx, segT, &right, &realUp, &rightDerivative, &upDerivative);
        double rotationAngle = m_cutRotation * Math::Pi;
        double cosR = std::cos(rotationAngle);
        double sinR = std::sin(rotationAngle);
        Vector3 rotatedRight = right * cosR + realUp * sinR;
        Vector3 rotatedUp = realUp * cosR - right * sinR;
        const double frameDerivativeScale = m_totalSpineLength / m_sourceLength / segLen;
        const Vector3 rotatedRightDerivative = (rightDerivative * cosR + upDerivative * sinR) * frameDerivativeScale;
        const Vector3 rotatedUpDerivative = (upDerivative * cosR - rightDerivative * sinR) * frameDerivativeScale;
        double scale = radius * 2.0 / m_crossSize;
        double longitudinalScale = m_totalSpineLength / m_sourceLength;
        double scaleDerivative = 2.0 / m_crossSize
            * (m_meshNodes[nextNode].radius - m_meshNodes[segIdx].radius) / segLen * longitudinalScale;
        double nodeWidth = m_meshNodes[segIdx].deformWidth * (1.0 - segT) + m_meshNodes[nextNode].deformWidth * segT;
        double nodeThickness = m_meshNodes[segIdx].deformThickness * (1.0 - segT) + m_meshNodes[nextNode].deformThickness * segT;
        double widthDerivative = m_deformWidth * (scaleDerivative * nodeWidth + scale * (m_meshNodes[nextNode].deformWidth - m_meshNodes[segIdx].deformWidth) / segLen * longitudinalScale);
        double thicknessDerivative = m_deformThickness * (scaleDerivative * nodeThickness + scale * (m_meshNodes[nextNode].deformThickness - m_meshNodes[segIdx].deformThickness) / segLen * longitudinalScale);
        Vector3 derivativeX = rotatedRight * (scale * m_deformWidth * nodeWidth);
        Vector3 derivativeZ = rotatedUp * (scale * m_deformThickness * nodeThickness);
        Vector3 derivativeY = m_spineForwards[segIdx] * longitudinalScale
            + rotatedRight * ((sourceVertex.x() - m_sourceCenter.x()) * widthDerivative)
            + rotatedUp * ((sourceVertex.z() - m_sourceCenter.z()) * thicknessDerivative)
            + rotatedRightDerivative * ((sourceVertex.x() - m_sourceCenter.x()) * scale * m_deformWidth * nodeWidth)
            + rotatedUpDerivative * ((sourceVertex.z() - m_sourceCenter.z()) * scale * m_deformThickness * nodeThickness);
        Vector3 cofactorX = Vector3::crossProduct(derivativeY, derivativeZ);
        Vector3 cofactorY = Vector3::crossProduct(derivativeZ, derivativeX);
        Vector3 cofactorZ = Vector3::crossProduct(derivativeX, derivativeY);
        double determinant = Vector3::dotProduct(derivativeX, cofactorX);
        if (!std::isfinite(determinant) || determinant == 0)
            return Vector3();
        Vector3 normal = cofactorX * sourceNormal.x() + cofactorY * sourceNormal.y() + cofactorZ * sourceNormal.z();
        if (determinant < 0)
            normal = -normal;
        double maximumComponent = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
            if (!std::isfinite(normal[axis]))
                return Vector3();
            maximumComponent = std::max(maximumComponent, std::abs(normal[axis]));
        }
        if (maximumComponent == 0)
            return Vector3();
        // Normalize after rescaling to avoid squared-length overflow/underflow.
        return Vector3(normal.x() / maximumComponent, normal.y() / maximumComponent, normal.z() / maximumComponent).normalized();
    }

    // Compatibility overload evaluated at the source cross-section centre.
    Vector3 deformNormal(const Vector3& sourceNormal, double sourceY) const
    {
        return deformNormal(sourceNormal, Vector3(m_sourceCenter.x(), sourceY, m_sourceCenter.z()));
    }

private:
    size_t segmentForDistance(double targetDist) const
    {
        size_t segIdx = 0;
        for (size_t i = 1; i < m_spineDistances.size(); ++i) {
            if (m_spineDistances[i] >= targetDist) {
                segIdx = i - 1;
                break;
            }
            segIdx = i - 1;
        }
        if (m_spineDistances.size() < 2)
            segIdx = 0;
        else if (segIdx + 1 >= m_spineDistances.size())
            segIdx = m_spineDistances.size() - 2;
        return segIdx;
    }

    static Vector3 normalizedDerivative(const Vector3& value, const Vector3& derivative)
    {
        const double length = value.length();
        if (length <= 0.000000000001)
            return Vector3();
        const Vector3 direction = value / length;
        return (derivative - direction * Vector3::dotProduct(direction, derivative)) / length;
    }

    void crossSectionBasis(size_t segIdx, double segT, Vector3* right, Vector3* realUp,
        Vector3* rightDerivative = nullptr, Vector3* upDerivative = nullptr) const
    {
        Vector3 forward = m_spineForwards[segIdx];
        Vector3 forwardDerivative;
        if (m_isCircle && m_meshNodes.size() >= 2) {
            const size_t nextNode = (segIdx + 1) % m_meshNodes.size();
            const Vector3 rawForward = m_nodeForwards[segIdx] * (1 - segT) + m_nodeForwards[nextNode] * segT;
            if (!rawForward.isZero()) {
                forward = rawForward.normalized();
                forwardDerivative = normalizedDerivative(rawForward, m_nodeForwards[nextNode] - m_nodeForwards[segIdx]);
            }
        }
        // Re-orthogonalise the global base normal against the local forward
        // direction to obtain the cross-section frame. m_baseNormal plays the
        // role of the width axis (right); realUp is the thickness axis. The
        // cross-product order is chosen so (right, forward, realUp) keeps the
        // same handedness as the source mesh: for a straight +Y spine this is
        // right=+X, realUp=+Z (an identity frame), so an imported model is swept
        // without being mirrored. (TubeMeshBuilder uses the opposite order, but
        // it builds fresh geometry and has no source chirality to preserve.)
        const Vector3 rawUp = Vector3::crossProduct(m_baseNormal, forward);
        const Vector3 v = rawUp.normalized();
        const Vector3 derivativeV = normalizedDerivative(rawUp, Vector3::crossProduct(m_baseNormal, forwardDerivative));
        const Vector3 rawRight = Vector3::crossProduct(forward, v);
        const Vector3 u = rawRight.normalized();
        *right = u;
        *realUp = v;
        if (rightDerivative)
            *rightDerivative = normalizedDerivative(rawRight,
                Vector3::crossProduct(forwardDerivative, v) + Vector3::crossProduct(forward, derivativeV));
        if (upDerivative)
            *upDerivative = derivativeV;
    }

    const std::vector<MeshNode>& m_meshNodes;
    bool m_isCircle;
    std::vector<Vector3> m_nodeForwards;
    float m_deformWidth;
    float m_deformThickness;
    float m_cutRotation;
    Vector3 m_sourceMin;
    Vector3 m_sourceCenter;
    double m_sourceLength = 0.0001;
    double m_crossSize = 0.0001;
    std::vector<double> m_spineDistances;
    double m_totalSpineLength = 0.0001;
    std::vector<Vector3> m_spineForwards;
    Vector3 m_baseNormal;
};

}

#endif

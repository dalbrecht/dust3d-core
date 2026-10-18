// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/mesh/spine_deformer.h>
#include <iostream>
#include <stdexcept>

namespace {
void requireVector(const dust3d::Vector3& actual, const dust3d::Vector3& expected)
{
    for (size_t axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(actual[axis]) || std::abs(actual[axis] - expected[axis]) > 1e-10)
            throw std::runtime_error("Spine deformation lost independent width/thickness scaling");
    }
}
void testDirectionalNodeScales()
{
    std::vector<dust3d::MeshNode> nodes(2);
    nodes[0].origin = dust3d::Vector3(0, 0, 0);
    nodes[1].origin = dust3d::Vector3(0, 2, 0);
    nodes[0].radius = nodes[1].radius = 1;
    nodes[0].deformWidth = 1;
    nodes[1].deformWidth = 3;
    nodes[0].deformThickness = 2;
    nodes[1].deformThickness = 4;
    const dust3d::Vector3 minimum(-1, 0, -1), maximum(1, 2, 1);
    dust3d::SpineDeformer deformer(nodes, minimum, maximum, 2, 3, 0);
    requireVector(deformer.deformVertex(dust3d::Vector3(.5, 0, .25)), dust3d::Vector3(1, 0, 1.5));
    requireVector(deformer.deformVertex(dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(2, 1, 2.25));
    requireVector(deformer.deformVertex(dust3d::Vector3(.5, 2, .25)), dust3d::Vector3(3, 2, 3));
    requireVector(deformer.deformNormal(dust3d::Vector3(1, 0, 0), dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(1, -1, 0).normalized());
    requireVector(deformer.deformNormal(dust3d::Vector3(0, 0, 1), dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(0, -.75, 1).normalized());
    nodes[0].radius = .5;
    dust3d::SpineDeformer radiusAndScales(nodes, minimum, maximum, 2, 3, 0);
    requireVector(radiusAndScales.deformNormal(dust3d::Vector3(1, 0, 0), dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(1, -1.25, 0).normalized());
    requireVector(radiusAndScales.deformNormal(dust3d::Vector3(0, 0, 1), dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(0, -1.125, 1).normalized());
    nodes.insert(nodes.begin() + 1, nodes[0]);
    nodes[1].origin = dust3d::Vector3(1, 1, 0);
    nodes[1].radius = .75;
    nodes[1].deformWidth = 2;
    nodes[1].deformThickness = 3;
    dust3d::SpineDeformer bent(nodes, minimum, maximum, 2, 3, .25);
    const dust3d::Vector3 point(.2, .6, .3);
    const dust3d::Vector3 tangentA(1, -1, 0), tangentB(1, 1, -2);
    const double step = 1e-5;
    const auto deformedA = (bent.deformVertex(point + tangentA * step) - bent.deformVertex(point - tangentA * step)).normalized();
    const auto deformedB = (bent.deformVertex(point + tangentB * step) - bent.deformVertex(point - tangentB * step)).normalized();
    const auto normal = bent.deformNormal(dust3d::Vector3(1, 1, 1).normalized(), point);
    if (std::abs(dust3d::Vector3::dotProduct(normal, deformedA)) > 1e-7
        || std::abs(dust3d::Vector3::dotProduct(normal, deformedB)) > 1e-7 || std::abs(normal.length() - 1) > 1e-10)
        throw std::runtime_error("Node-scale normal derivatives disagree with finite-difference tangents");
}

void testNormalJacobian()
{
    std::vector<dust3d::MeshNode> nodes(2);
    nodes[0].origin = dust3d::Vector3(0, 0, 0);
    nodes[1].origin = dust3d::Vector3(0, 2, 0);
    nodes[0].radius = .5;
    nodes[1].radius = 1;
    const dust3d::Vector3 minimum(-1, 0, -1), maximum(1, 2, 1);
    dust3d::SpineDeformer tapered(nodes, minimum, maximum, 2, 3, 0);
    requireVector(tapered.deformNormal(dust3d::Vector3(1, 0, 0), dust3d::Vector3(.5, 1, .25)), dust3d::Vector3(1, -.25, 0).normalized());
    nodes.insert(nodes.begin() + 1, nodes[0]);
    nodes[1].origin = dust3d::Vector3(1, 1, 0);
    nodes[1].radius = .75;
    dust3d::SpineDeformer bent(nodes, minimum, maximum, 2, 3, .25);
    const dust3d::Vector3 point(.2, .6, .3);
    const dust3d::Vector3 tangentA(1, -1, 0), tangentB(1, 1, -2);
    const double step = 1e-5;
    const auto deformedA = (bent.deformVertex(point + tangentA * step) - bent.deformVertex(point - tangentA * step)).normalized();
    const auto deformedB = (bent.deformVertex(point + tangentB * step) - bent.deformVertex(point - tangentB * step)).normalized();
    const auto normal = bent.deformNormal(dust3d::Vector3(1, 1, 1).normalized(), point);
    if (std::abs(dust3d::Vector3::dotProduct(normal, deformedA)) > 1e-7 || std::abs(dust3d::Vector3::dotProduct(normal, deformedB)) > 1e-7 || std::abs(normal.length() - 1) > 1e-10
        || dust3d::Vector3::dotProduct(normal, dust3d::Vector3::crossProduct(deformedA, deformedB).normalized()) < .999999)
        throw std::runtime_error("Imported normal does not match finite-difference surface tangents");
}
}
int main()
{
    try {
        std::vector<dust3d::MeshNode> nodes(2);
        nodes[0].origin = dust3d::Vector3(0, 0, 0);
        nodes[1].origin = dust3d::Vector3(0, 2, 0);
        nodes[0].radius = nodes[1].radius = 1;
        const dust3d::Vector3 minimum(-1, 0, -1), maximum(1, 2, 1);
        const dust3d::Vector3 point(.5, 1, .25);
        for (float width : { 0.0f, 1.0f, 2.0f, -2.0f }) {
            for (float thickness : { 0.0f, 1.0f, 3.0f }) {
                dust3d::SpineDeformer deformer(nodes, minimum, maximum, width, thickness, 0);
                requireVector(deformer.deformVertex(point), dust3d::Vector3(.5 * width, 1, .25 * thickness));
            }
        }
        dust3d::SpineDeformer rotatedNormals(nodes, minimum, maximum, 2, 1, .5);
        requireVector(rotatedNormals.deformNormal(dust3d::Vector3(1, 0, 0), 1), dust3d::Vector3(0, 0, 1));
        dust3d::SpineDeformer scaledNormals(nodes, minimum, maximum, 2, 1, 0);
        requireVector(scaledNormals.deformNormal(dust3d::Vector3(1, 0, 1).normalized(), 1), dust3d::Vector3(1 / std::sqrt(5.0), 0, 2 / std::sqrt(5.0)));
        dust3d::SpineDeformer reflected(nodes, minimum, maximum, -2, 1, 0);
        requireVector(reflected.deformNormal(dust3d::Vector3(1, 0, 0), point), dust3d::Vector3(-1, 0, 0));
        dust3d::SpineDeformer thin(nodes, minimum, maximum, 1e-18f, 1, 0);
        requireVector(thin.deformNormal(dust3d::Vector3(0, 1, 0), point), dust3d::Vector3(0, 1, 0));
        requireVector(thin.deformNormal(dust3d::Vector3(), point), dust3d::Vector3());
        requireVector(thin.deformNormal(dust3d::Vector3(0, 1, 0), dust3d::Vector3(0, -1, 0)), dust3d::Vector3());
        dust3d::SpineDeformer rotated(nodes, minimum, maximum, 0, 3, .5);
        requireVector(rotated.deformVertex(point), dust3d::Vector3(-.75, 1, 0));
        requireVector(rotated.deformNormal(dust3d::Vector3(1, 0, 0), point), dust3d::Vector3());
        nodes[0].radius = nodes[1].radius = 0;
        dust3d::SpineDeformer zeroRadius(nodes, minimum, maximum, 0, 1, 0);
        requireVector(zeroRadius.deformVertex(point), dust3d::Vector3(0, 1, 0));
        requireVector(zeroRadius.deformNormal(dust3d::Vector3(0, 1, 0), point), dust3d::Vector3());
        nodes[0].radius = .5;
        nodes[1].radius = 1;
        nodes.insert(nodes.begin() + 1, nodes[0]);
        nodes[1].origin = dust3d::Vector3(1, 1, 0);
        dust3d::SpineDeformer flatBent(nodes, minimum, maximum, 0, 3, .25);
        dust3d::SpineDeformer wideBent(nodes, minimum, maximum, 2, 3, .25);
        requireVector(flatBent.deformVertex(dust3d::Vector3(0, 1, .25)), wideBent.deformVertex(dust3d::Vector3(0, 1, .25)));
        testNormalJacobian();
        testDirectionalNodeScales();
        std::cout << "Independent spine scale/normal contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

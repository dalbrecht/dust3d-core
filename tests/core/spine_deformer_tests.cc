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
        dust3d::SpineDeformer rotated(nodes, minimum, maximum, 0, 3, .5);
        requireVector(rotated.deformVertex(point), dust3d::Vector3(-.75, 1, 0));
        nodes[0].radius = nodes[1].radius = 0;
        dust3d::SpineDeformer zeroRadius(nodes, minimum, maximum, 0, 1, 0);
        requireVector(zeroRadius.deformVertex(point), dust3d::Vector3(0, 1, 0));
        nodes[0].radius = .5;
        nodes[1].radius = 1;
        nodes.insert(nodes.begin() + 1, nodes[0]);
        nodes[1].origin = dust3d::Vector3(1, 1, 0);
        dust3d::SpineDeformer flatBent(nodes, minimum, maximum, 0, 3, .25);
        dust3d::SpineDeformer wideBent(nodes, minimum, maximum, 2, 3, .25);
        requireVector(flatBent.deformVertex(dust3d::Vector3(0, 1, .25)), wideBent.deformVertex(dust3d::Vector3(0, 1, .25)));
        std::cout << "Independent spine scale contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

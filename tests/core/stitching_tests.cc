#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;
using Vertices = std::vector<Dust3DVertex>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string identity(unsigned int value)
{
    std::ostringstream stream;
    stream << '{' << std::setfill('0') << std::setw(8) << value << "-0000-4000-8000-" << std::setw(12) << value << '}';
    return stream.str();
}

// Independent synthetic equivalents of the Apple editor's stitching fixtures.
std::string model(bool loops, const std::string& attributes, bool isolated = false, bool filled = true)
{
    std::ostringstream nodes;
    std::ostringstream edges;
    std::ostringstream parts;
    std::ostringstream children;
    nodes << std::setprecision(17);
    const unsigned int partCount = loops ? (isolated ? 1 : 2) : 3;
    const unsigned int nodeCount = loops ? 4 : 3;
    const double pi = std::acos(-1.0);
    for (unsigned int part = 0; part < partCount; ++part) {
        for (unsigned int node = 0; node < nodeCount; ++node) {
            const double angle = loops ? 2.0 * pi * node / nodeCount : pi * node / (nodeCount - 1);
            const double radius = loops ? (part == 0 ? .4 : .16) : .25 + part * .05;
            const double x = .5 + std::cos(angle) * radius;
            const double y = .5 + std::sin(angle) * radius;
            const double z = loops ? (part == 0 ? .5 : .6) : part * .5;
            nodes << "<node id='" << identity(1000 + part * 10 + node) << "' partId='" << identity(100 + part)
                  << "' x='" << x << "' y='" << y << "' z='" << z << "' radius='.05'/>";
            if (node > 0)
                edges << "<edge id='" << identity(2000 + part * 10 + node) << "' partId='" << identity(100 + part)
                      << "' from='" << identity(1000 + part * 10 + node - 1) << "' to='" << identity(1000 + part * 10 + node) << "'/>";
        }
        if (loops)
            edges << "<edge id='" << identity(2000 + part * 10) << "' partId='" << identity(100 + part)
                  << "' from='" << identity(1000 + part * 10 + nodeCount - 1) << "' to='" << identity(1000 + part * 10) << "'/>";
        // Connected-loop coverage clears only the inner fill; isolated coverage clears its only loop.
        const bool fill = filled || (!isolated && part == 0);
        parts << "<part id='" << identity(100 + part) << "' target='" << (loops ? "StitchingLoop" : "StitchingLine")
              << "' fillLoopInterior='" << (fill ? "true" : "false") << "'/>";
        children << "<component id='" << identity(200 + part) << "' linkDataType='partId' linkData='" << identity(100 + part) << "'/>";
    }
    if (isolated) {
        // This separate model makes the unfilled scene nonempty without stitching or boolean interference.
        nodes << "<node id='" << identity(1999) << "' partId='" << identity(199) << "' x='2' y='2' z='2' radius='.05'/>";
        parts << "<part id='" << identity(199) << "' target='Model'/>";
        children << "<component id='" << identity(299) << "' linkDataType='partId' linkData='" << identity(199) << "' combineMode='Uncombined'/>";
    }
    return "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes.str() + "</nodes><edges>" + edges.str()
        + "</edges><parts>" + parts.str() + "</parts><components><component id='" + identity(300) + "' color='#3366cc' "
        + attributes + ">" + children.str() + "</component></components></canvas>";
}

std::string archive(const std::string& xml)
{
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    return stream.str();
}

Vertices load(const std::string& xml, size_t nodeCount, size_t partCount)
{
    const auto bytes = archive(xml);
    Dust3DError error {};
    MeshOwner owner(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &error), dust3d_mesh_destroy);
    require(owner && error.status == DUST3D_STATUS_OK, std::string("Configured surface failed: ") + error.message);
    require(dust3d_mesh_node_count(owner.get()) == nodeCount && dust3d_mesh_part_count(owner.get()) == partCount, "Source graph counts changed during generation");
    const auto count = dust3d_mesh_vertex_count(owner.get());
    const auto* data = dust3d_mesh_vertices(owner.get());
    require(count > 0 && count % 3 == 0 && data, "Configured surface must contain triangles");
    Vertices result(data, data + count);
    for (const auto& vertex : result) {
        double lengthSquared = 0;
        for (size_t axis = 0; axis < 3; ++axis) {
            require(std::isfinite(vertex.position[axis]) && std::isfinite(vertex.normal[axis]), "Nonfinite position or normal");
            lengthSquared += static_cast<double>(vertex.normal[axis]) * vertex.normal[axis];
        }
        require(std::abs(std::sqrt(lengthSquared) - 1.0) < .0001, "Generated normal must have unit length");
        for (const auto channel : vertex.color)
            require(std::isfinite(channel) && channel >= 0 && channel <= 1, "Generated color must be finite and bounded");
    }
    return result;
}

using Position = std::array<float, 3>;
using Triangle = std::array<Position, 3>;

std::vector<Triangle> canonicalTriangles(const Vertices& vertices)
{
    std::vector<Triangle> triangles;
    for (size_t index = 0; index < vertices.size(); index += 3) {
        Triangle triangle;
        for (size_t corner = 0; corner < 3; ++corner)
            std::copy_n(vertices[index + corner].position, 3, triangle[corner].begin());
        std::sort(triangle.begin(), triangle.end());
        triangles.push_back(triangle);
    }
    std::sort(triangles.begin(), triangles.end());
    return triangles;
}

bool samePositions(const Vertices& first, const Vertices& second)
{
    if (first.size() != second.size() || first.size() % 3 != 0)
        return false;
    constexpr double tolerance = .000001;
    auto coordinateSum = [](const Triangle& triangle) {
        return static_cast<double>(triangle[0][0]) + triangle[1][0] + triangle[2][0];
    };
    std::multimap<double, Triangle> remaining;
    for (const auto& triangle : canonicalTriangles(second))
        remaining.emplace(coordinateSum(triangle), triangle);
    for (const auto& triangle : canonicalTriangles(first)) {
        const auto sum = coordinateSum(triangle);
        bool matched = false;
        // Tolerance can reorder almost-equal corners; test permutations rather than
        // treating their lexicographic order as a geometric distinction.
        for (auto candidate = remaining.lower_bound(sum - 3 * tolerance); candidate != remaining.end() && candidate->first <= sum + 3 * tolerance; ++candidate) {
            std::array<size_t, 3> permutation { 0, 1, 2 };
            do {
                bool equal = true;
                for (size_t corner = 0; corner < 3; ++corner) {
                    for (size_t axis = 0; axis < 3; ++axis)
                        equal = equal && std::abs(static_cast<double>(triangle[corner][axis]) - candidate->second[permutation[corner]][axis]) <= tolerance;
                }
                if (equal) {
                    remaining.erase(candidate);
                    matched = true;
                    break;
                }
            } while (std::next_permutation(permutation.begin(), permutation.end()));
            if (matched)
                break;
        }
        if (!matched)
            return false;
    }
    return true;
}

void verifyPositionComparison()
{
    Vertices original(6);
    original[0].position[0] = 1;
    original[1].position[1] = 1;
    original[2].position[2] = 1;
    original[3].position[0] = 2;
    original[4].position[1] = 2;
    original[5].position[2] = 2;
    const Vertices reordered { original[5], original[3], original[4], original[1], original[2], original[0] };
    require(samePositions(original, reordered), "Triangle and corner emission order must not change the comparison");
    auto moved = reordered;
    moved[0].position[0] += .0000005f;
    require(samePositions(original, moved), "Sub-tolerance coordinate changes must compare equal");
    moved[0].position[0] += .00001f;
    require(!samePositions(original, moved), "Above-tolerance coordinate changes must compare unequal");
    auto topology = original;
    std::swap(topology[1], topology[4]);
    require(!samePositions(original, topology), "Reconnecting the same vertices must change the triangles");
    auto missing = original;
    missing.pop_back();
    require(!samePositions(original, missing), "Different vertex counts must compare unequal");
}

void lines()
{
    const auto baseline = load(model(false, "sideClosed='true'"), 9, 3);
    for (const auto& attributes : { "sideClosed='true' frontClosed='true'", "sideClosed='true' backClosed='true'", "sideClosed='false'", "sideClosed='true' targetSegments='12'" })
        require(!samePositions(baseline, load(model(false, attributes), 9, 3)), std::string("Line setting did not affect geometry: ") + attributes);
    for (const auto segments : { 0, 1, 100 })
        load(model(false, "sideClosed='true' targetSegments='" + std::to_string(segments) + "'"), 9, 3);
}

void loops()
{
    const auto baseline = load(model(true, "backClosed='true'"), 8, 2);
    for (const auto& attributes : { "backClosed='false'", "backClosed='true' backCloseDepthRatio='.5'", "backClosed='true' backCloseSharpness='.8'" })
        require(!samePositions(baseline, load(model(true, attributes), 8, 2)), std::string("Loop setting did not affect geometry: ") + attributes);
    for (const auto& attributes : { "backClosed='true' backCloseDepthRatio='0'", "backClosed='true' backCloseDepthRatio='2'", "backClosed='true' backCloseSharpness='0'", "backClosed='true' backCloseSharpness='1'" })
        load(model(true, attributes), 8, 2);
}

void fill()
{
    const auto isolated = load(model(true, "", true, true), 5, 2);
    const auto unfilled = load(model(true, "", true, false), 5, 2);
    require(isolated.size() > unfilled.size() && !samePositions(isolated, unfilled), "Isolated fill must add actual surface triangles");
    require(samePositions(load(model(true, "", false, true), 8, 2), load(model(true, "", false, false), 8, 2)), "Connected loop fill must retain its existing surface");
    // Repeated loads also verify that copied geometry does not depend on an earlier handle's lifetime.
    require(samePositions(isolated, load(model(true, "", true, true), 5, 2)), "Refilling must restore deterministic geometry");
}
}

int main(int argc, char** argv)
{
    try {
        verifyPositionComparison();
        require(argc == 2, "Expected lines, loops or fill scenario");
        const std::string scenario = argv[1];
        if (scenario == "lines")
            lines();
        else if (scenario == "loops")
            loops();
        else if (scenario == "fill")
            fill();
        else
            throw std::runtime_error("Unknown stitching scenario");
        std::cout << "Stitching contract passed: " << scenario << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

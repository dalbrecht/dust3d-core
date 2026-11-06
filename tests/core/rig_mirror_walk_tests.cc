// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/rig/rig_generator.h>
#include <iostream>
#include <stdexcept>
using namespace dust3d;
namespace {
const std::string a = "{00000000-0000-0000-0000-000000000001}";
const std::string b = "{00000000-0000-0000-0000-000000000002}";
const std::string c = "{00000000-0000-0000-0000-000000000003}";
const std::string d = "{00000000-0000-0000-0000-000000000004}";
Snapshot fixture()
{
    Snapshot s;
    s.canvas = { { "originX", "0" }, { "originY", "0" }, { "originZ", "0" } };
    s.parts["body"] = { { "target", "Model" } };
    s.parts["reference"] = { { "target", "CutFace" } };
    s.nodes[a] = { { "partId", "body" }, { "x", "1" }, { "y", "-1" }, { "z", "0" }, { "radius", "0.2" } };
    s.nodes[b] = { { "partId", "body" }, { "x", "0" }, { "y", "-3" }, { "z", "0" }, { "radius", "0.2" } };
    s.nodes[c] = { { "partId", "reference" }, { "x", "2" }, { "y", "-1" }, { "z", "0" }, { "radius", "0.2" } };
    s.nodes[d] = { { "partId", "reference" }, { "x", "2" }, { "y", "-1" }, { "z", "0" }, { "radius", "0.2" } };
    s.edges["edge"] = { { "from", a }, { "to", b }, { "partId", "body" }, { "boneName", "Body" } };
    return s;
}
void check(const Snapshot& s, float x)
{
    RigStructure templ;
    templ.type = "Test";
    RigNode body;
    body.name = "Body";
    templ.bones = { body };
    RigGenerator generator;
    RigStructure actual;
    if (!generator.generateRig(&s, templ, actual))
        throw std::runtime_error("Ordinary mirror generation failed");
    const auto& result = actual.bones.at(0);
    if (std::abs(result.posX - x) > 1e-5 || std::abs(result.posY - 1) > 1e-5 || std::abs(result.posZ) > 1e-5 || std::abs(result.endX) > 1e-5 || std::abs(result.endY - 3) > 1e-5)
        throw std::runtime_error("Mirror parity/fallback/frame differs");
}
}
int main()
{
    try {
        auto s = fixture();
        check(s, 1);
        s.nodes[a]["__mirrorFromNodeId"] = c;
        check(s, -2);
        s.nodes[c]["__mirrorFromNodeId"] = d;
        check(s, 2);
        s.nodes[a].erase("x");
        s.nodes[c].erase("x");
        check(s, 2); // successful mirrors never parse intermediate coordinates
        s = fixture();
        s.nodes[a]["__mirrorFromNodeId"] = d;
        s.nodes.erase(d);
        check(s, 1);
        s = fixture();
        s.nodes[a]["__mirrorFromNodeId"] = c;
        s.nodes[c]["__mirrorFromNodeId"] = d;
        s.nodes.erase(d);
        check(s, -2);
        s = fixture();
        s.nodes[a]["__mirrorFromNodeId"] = a;
        check(s, 1);
        s = fixture();
        s.nodes[a]["__mirrorFromNodeId"] = c;
        s.nodes[c]["__mirrorFromNodeId"] = a;
        check(s, -2);
        s.nodes[c]["__mirrorFromNodeId"] = d;
        s.nodes[d]["__mirrorFromNodeId"] = a;
        check(s, 2);
        std::cout << "Rig mirror walk contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

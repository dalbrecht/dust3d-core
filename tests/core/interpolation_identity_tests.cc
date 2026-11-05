// SPDX-License-Identifier: MIT
#include <algorithm>
#include <dust3d/mesh/interpolation_identities.h>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
const std::string from = "{11111111-1111-4111-8111-111111111111}";
const std::string to = "{22222222-2222-4222-8222-222222222222}";
const std::string edge = "{33333333-3333-4333-8333-333333333333}";
const std::array<std::string, 2> legacyNodes = {
    "{11111111-1111-4111-8333-333333333333}", "{22222222-2222-4222-8333-333333333333}"
};
const std::array<std::string, 3> legacyEdges = {
    "{33333333-3333-4333-8111-111111111111}", "{11111111-1111-4111-8222-222222222222}",
    "{33333333-3333-4333-8222-222222222222}"
};
void fresh(const std::vector<dust3d::InterpolatedEdgeIdentities>& planned,
    const std::set<std::string>& nodes, const std::set<std::string>& edges)
{
    auto occupied = nodes;
    occupied.insert(edges.begin(), edges.end());
    for (const auto& ids : planned) {
        for (const auto& id : ids.nodeIds)
            require(occupied.insert(id).second, "Generated node identity aliases original or generated data");
        for (const auto& id : ids.edgeIds)
            require(occupied.insert(id).second, "Generated edge identity aliases original or generated data");
    }
}
void legacyCompatibility()
{
    const auto planned = dust3d::planInterpolationIdentities({ { edge, from, to } }, { from, to }, { edge });
    require(planned.size() == 1, "One edge must have one identity plan");
    require(planned[0].nodeIds == legacyNodes && planned[0].edgeIds == legacyEdges,
        "Collision-free interpolation identities changed");
    fresh(planned, { from, to }, { edge });
}
void sharedPrefixAndOriginalCollisions()
{
    const std::string a = "{20000000-0000-4000-8000-000000000011}";
    const std::string b = "{20000000-0000-4000-8000-000000000012}";
    const std::string e = "{30000000-0000-4000-8000-000000000021}";
    const auto first = dust3d::planInterpolationIdentities({ { e, a, b } }, { a, b }, { e });
    fresh(first, { a, b }, { e });
    const auto repeated = dust3d::planInterpolationIdentities({ { e, a, b } }, { a, b }, { e });
    require(first[0].nodeIds == repeated[0].nodeIds && first[0].edgeIds == repeated[0].edgeIds,
        "Collision resolution must be deterministic");
    const std::set<std::string> nodes = { from, to, legacyNodes[0], legacyEdges[0] };
    const std::set<std::string> edges = { edge, legacyNodes[1], legacyEdges[2] };
    const auto guarded = dust3d::planInterpolationIdentities({ { edge, from, to } }, nodes, edges);
    fresh(guarded, nodes, edges);
    require(guarded[0].edgeIds[1] == legacyEdges[1], "Uncontested legacy identities should remain stable");
}
void allInputsPreflightAndStableOrder()
{
    const std::string other = "{44444444-4444-4444-8333-333333333333}";
    const dust3d::InterpolationEdge a { edge, from, to }, b { other, from, to };
    const auto forward = dust3d::planInterpolationIdentities({ a, b }, { from, to }, { edge, other });
    const auto reverse = dust3d::planInterpolationIdentities({ b, a }, { from, to }, { edge, other });
    fresh(forward, { from, to }, { edge, other });
    require(forward[0].nodeIds == reverse[1].nodeIds && forward[0].edgeIds == reverse[1].edgeIds
            && forward[1].nodeIds == reverse[0].nodeIds && forward[1].edgeIds == reverse[0].edgeIds,
        "Identity admission must follow stable edge order, not caller iteration order");

    const std::string sharedA = "{20000000-0000-4000-8000-000000000011}";
    const std::string sharedB = "{20000000-0000-4000-8000-000000000012}";
    const std::string sharedEdge = "{30000000-0000-4000-8000-000000000021}";
    const std::string c = "{00000000-0000-4000-8111-111111111111}";
    const std::string d = "{66666666-6666-4666-8666-666666666666}";
    const std::string later = "{55555555-5555-4555-8000-000000000001}";
    const std::string reservedLegacy = "{00000000-0000-4000-8000-000000000001}";
    const std::set<std::string> nodes = { sharedA, sharedB, c, d };
    const std::set<std::string> edges = { sharedEdge, later };
    const auto planned = dust3d::planInterpolationIdentities(
        { { sharedEdge, sharedA, sharedB }, { later, c, d } }, nodes, edges);
    fresh(planned, nodes, edges);
    require(planned[1].nodeIds[0] == reservedLegacy, "Fallback stole a later uncontested legacy identity");
}
void originalUuidSpellingsRemainReserved()
{
    const std::string a = "{abcdefab-cdef-4abc-8def-abcdefabcdef}";
    const std::string legacy = "{abcdefab-cdef-4abc-8333-333333333333}";
    const std::string alias = "{ABCDEFAB-CDEF-4ABC-8333-333333333333}";
    const auto planned = dust3d::planInterpolationIdentities({ { edge, a, to } }, { a, to, alias }, { edge });
    require(planned[0].nodeIds[0] != legacy, "Generated node aliases an original UUID spelling");
    const auto rawAlias = legacyEdges[1].substr(1, 36);
    const auto raw = dust3d::planInterpolationIdentities({ { edge, from, to } }, { from, to }, { edge, rawAlias });
    require(raw[0].edgeIds[1] != legacyEdges[1], "Generated edge aliases an unbraced original UUID");
}
void invalidRequestsAreValuesOnly()
{
    const auto rejects = [](const auto& inputs, const auto& nodes, const auto& edges) {
        bool rejected = false;
        try {
            (void)dust3d::planInterpolationIdentities(inputs, nodes, edges);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected, "Invalid identity request was admitted");
    };
    const std::vector<dust3d::InterpolationEdge> duplicated = { { edge, from, to }, { edge, from, to } };
    rejects(duplicated, std::set<std::string> { from, to }, std::set<std::string> { edge });
    rejects(std::vector<dust3d::InterpolationEdge> { { edge, from, to } }, std::set<std::string> { from }, std::set<std::string> { edge });
    rejects(std::vector<dust3d::InterpolationEdge> { { edge, "short", to } }, std::set<std::string> { "short", to }, std::set<std::string> { edge });
    rejects(std::vector<dust3d::InterpolationEdge> { { edge, from, from } }, std::set<std::string> { from }, std::set<std::string> { edge });
    rejects(std::vector<dust3d::InterpolationEdge> { { edge, from, to } }, std::set<std::string> { from, to }, std::set<std::string> {});
    const std::string invalidHex = "{gggggggg-3333-4333-8333-333333333333}";
    rejects(std::vector<dust3d::InterpolationEdge> { { invalidHex, from, to } }, std::set<std::string> { from, to }, std::set<std::string> { invalidHex });
    const auto unbraced = to.substr(1, 36);
    rejects(std::vector<dust3d::InterpolationEdge> { { edge, from, unbraced } }, std::set<std::string> { from, unbraced }, std::set<std::string> { edge });
    require(dust3d::planInterpolationIdentities({}, {}, {}).empty(), "Empty interpolation changed identities");
}
}
int main()
{
    try {
        legacyCompatibility();
        sharedPrefixAndOriginalCollisions();
        allInputsPreflightAndStableOrder();
        originalUuidSpellingsRemainReserved();
        invalidRequestsAreValuesOnly();
        std::cout << "Interpolation identity admission contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/base/source_corner.h>
#include <dust3d/mesh/mesh_state.h>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
namespace {
const dust3d::Uuid nodeA("{00000000-0000-4000-8000-000000000001}");
const dust3d::Uuid nodeB("{00000000-0000-4000-8000-000000000002}");
const dust3d::Uuid nodeC("{00000000-0000-4000-8000-000000000003}");
const dust3d::Uuid nodeD("{00000000-0000-4000-8000-000000000004}");
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void near(double actual, double expected)
{
    require(std::abs(actual - expected) < 1e-5, "Interpolated source weights changed their original scale");
}
dust3d::SourceCorner weighted(const dust3d::Vector3& point, bool second)
{
    dust3d::SourceCorner result;
    result.weighted = true;
    result.setInfluences({ { second ? nodeC : nodeA, float(2 + point.x()) }, { second ? nodeD : nodeB, float(4 + point.y()) } });
    return result;
}
std::unique_ptr<dust3d::MeshState> box(double offset, bool second = false)
{
    std::vector<dust3d::Vector3> vertices;
    for (const auto& point : std::vector<std::array<int, 3>> { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } })
        vertices.emplace_back(point[0] + offset, point[1] + (offset ? .2 : 0), point[2] + (offset ? .3 : 0));
    const std::vector<std::vector<size_t>> faces { { 0, 2, 1 }, { 0, 3, 2 }, { 4, 5, 6 }, { 4, 6, 7 }, { 0, 1, 5 }, { 0, 5, 4 }, { 3, 7, 6 }, { 3, 6, 2 }, { 0, 4, 7 }, { 0, 7, 3 }, { 1, 2, 6 }, { 1, 6, 5 } };
    auto result = std::make_unique<dust3d::MeshState>(vertices, faces);
    for (const auto& face : faces) {
        dust3d::SourceTriangleCorners record;
        for (size_t corner = 0; corner < 3; ++corner)
            record[corner] = weighted(vertices[face[corner]], second);
        result->triangleSourceCorners.push_back(std::move(record));
    }
    return result;
}
void copyAndBooleanInterpolation()
{
    auto first = box(0), second = box(.4, true);
    dust3d::MeshState copy(*first);
    first->triangleSourceCorners[0][0].setInfluences({ { nodeA, 99 }, { nodeB, 4 } });
    near(copy.triangleSourceCorners[0][0].influences()[0].second, 2);
    first = box(0);
    std::vector<dust3d::Vector3> originalVertices[2];
    std::vector<std::vector<size_t>> originalFaces;
    first->fetch(originalVertices[0], originalFaces);
    second->fetch(originalVertices[1], originalFaces);
    size_t interpolated = 0;
    for (const auto method : { dust3d::MeshCombiner::Method::Union, dust3d::MeshCombiner::Method::Diff }) {
        auto output = dust3d::MeshState::combine(*first, *second, method, false);
        require(output && !output->isNull(), "Ordinary source Boolean fixture failed");
        std::vector<dust3d::Vector3> vertices;
        std::vector<std::vector<size_t>> faces;
        output->fetch(vertices, faces);
        require(output->triangleSourceCorners.size() == faces.size(), "Boolean source records lost triangle alignment");
        for (size_t face = 0; face < faces.size(); ++face) {
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& source = output->triangleSourceCorners[face][corner];
                require(source.weighted && source.influences().size() == 2, "Known Boolean source distribution was discarded");
                const auto& point = vertices[faces[face][corner]];
                const bool fromSecond = source.influences()[0].first == nodeC;
                require(source.influences()[0].first == (fromSecond ? nodeC : nodeA) && source.influences()[1].first == (fromSecond ? nodeD : nodeB), "Boolean source borrowed a coincident node identity");
                near(source.influences()[0].second, 2 + point.x());
                near(source.influences()[1].second, 4 + point.y());
                bool isOriginalCorner = false;
                for (const auto& original : originalVertices[fromSecond])
                    isOriginalCorner = isOriginalCorner || (point - original).lengthSquared() < 1e-16;
                interpolated += !isOriginalCorner;
            }
        }
    }
    require(interpolated > 0, "No newly interpolated Boolean corner was exercised");
    dust3d::RetainedPayloadBudget before(std::numeric_limits<size_t>::max());
    before.accountObject(copy);
    auto larger = copy.triangleSourceCorners[0][0].influences();
    larger.reserve(128);
    copy.triangleSourceCorners[0][0].setInfluences(std::move(larger));
    dust3d::RetainedPayloadBudget after(std::numeric_limits<size_t>::max());
    after.accountObject(copy);
    require(after.usedBytes() >= before.usedBytes() + 126 * sizeof(std::pair<dust3d::Uuid, float>), "Cached source influence capacity is not accounted");
}
void unknownAndMissingSources()
{
    auto first = box(0), second = box(.4, true);
    for (auto& record : second->triangleSourceCorners)
        for (auto& corner : record.corners)
            corner = {};
    auto result = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    size_t known = 0, unknown = 0;
    for (const auto& record : result->triangleSourceCorners)
        for (const auto& corner : record.corners)
            corner.influences().empty() ? ++unknown : ++known;
    require(known && unknown, "Unknown Boolean source data must not borrow a neighbouring source");
    second->triangleSourceCorners.clear();
    result = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    unknown = 0;
    for (const auto& record : result->triangleSourceCorners)
        for (const auto& corner : record.corners)
            unknown += corner.influences().empty();
    require(unknown > 0, "Missing source channel fabricated positional ownership");
}
void unitHintInterpolationAndPartialUnknown()
{
    auto first = box(0), second = box(.4, true);
    std::vector<dust3d::Vector3> inputVertices;
    std::vector<std::vector<size_t>> inputFaces;
    first->fetch(inputVertices, inputFaces);
    for (size_t face = 0; face < inputFaces.size(); ++face)
        for (size_t corner = 0; corner < 3; ++corner) {
            auto& source = first->triangleSourceCorners[face][corner];
            source.weighted = false;
            source.setInfluences({ { inputVertices[inputFaces[face][corner]].x() == 0 ? nodeA : nodeB, 1 } });
        }
    auto output = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    size_t blends = 0;
    for (const auto& record : output->triangleSourceCorners)
        for (const auto& source : record.corners)
            if (source.influences().size() == 2 && source.influences()[0].first == nodeA) {
                require(source.weighted, "Distinct node hints must become an explicitly weighted hint");
                near(source.influences()[0].second + source.influences()[1].second, 1);
                ++blends;
            }
    require(blends > 0, "No distinct unit node hints were interpolated");
    for (auto& record : first->triangleSourceCorners)
        record[0] = {};
    output = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    size_t known = 0, unknown = 0;
    for (const auto& record : output->triangleSourceCorners)
        for (const auto& source : record.corners)
            source.influences().empty() ? ++unknown : ++known;
    require(known && unknown, "A positive unknown contributor must not be replaced by normalized known contributions");
}
void derivedNumericAdmission()
{
    auto first = box(0), second = box(.4, true);
    for (auto& record : first->triangleSourceCorners)
        for (auto& corner : record.corners) {
            corner.weighted = true;
            corner.setInfluences({ { nodeA, std::numeric_limits<float>::max() }, { nodeA, std::numeric_limits<float>::max() } });
        }
    auto output = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    size_t unknown = 0, retainedDuplicates = 0;
    for (const auto& record : output->triangleSourceCorners)
        for (const auto& corner : record.corners) {
            unknown += corner.influences().empty();
            retainedDuplicates += corner.influences().size() == 2 && corner.influences()[0].first == nodeA && corner.influences()[1].first == nodeA;
            for (const auto& influence : corner.influences())
                require(std::isfinite(influence.second) && influence.second > 0, "Numeric source transfer emitted an invalid weight");
        }
    require(unknown > 0 && retainedDuplicates > 0, "Overflowing derived distribution must be unknown while exact duplicate entries remain unchanged");
    for (auto& record : first->triangleSourceCorners)
        for (size_t corner = 0; corner < 3; ++corner) {
            record[corner].weighted = true;
            record[corner].setInfluences({ { corner == 0 ? nodeA : corner == 1 ? nodeB
                                                                               : nodeC,
                std::numeric_limits<float>::denorm_min() } });
        }
    output = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, false);
    unknown = 0;
    for (const auto& record : output->triangleSourceCorners)
        for (const auto& corner : record.corners) {
            unknown += corner.influences().empty();
            for (const auto& influence : corner.influences())
                require(influence.second > 0, "Positive derived source contribution silently underflowed");
        }
    require(unknown > 0, "Unrepresentable positive derived hints must remain explicitly unknown");
}
void bridgeAgreement()
{
    auto first = box(0), second = box(.4);
    auto uniform = [](dust3d::MeshState& mesh, bool unknown) {
        for (auto& record : mesh.triangleSourceCorners)
            for (auto& corner : record.corners) {
                corner = {};
                if (!unknown) {
                    corner.weighted = true;
                    corner.setInfluences({ { nodeA, 2 } });
                }
            }
    };
    uniform(*first, false);
    uniform(*second, false);
    std::vector<std::pair<dust3d::MeshCombiner::Source, size_t>> positionalHints;
    std::unique_ptr<dust3d::MeshCombiner::Mesh> combined(dust3d::MeshCombiner::combine(*first->mesh, *second->mesh,
        dust3d::MeshCombiner::Method::Union, &positionalHints));
    std::vector<dust3d::Vector3> combinedVertices;
    std::vector<std::vector<size_t>> combinedFaces;
    combined->fetch(combinedVertices, combinedFaces);
    dust3d::MeshRecombiner recombiner;
    recombiner.setVertices(&combinedVertices, &positionalHints);
    recombiner.setFaces(&combinedFaces);
    require(recombiner.recombine(), "Ordinary vertex remap fixture failed");
    const auto& inputIndices = recombiner.regeneratedVertexInputIndices();
    require(inputIndices.size() == recombiner.regeneratedVertices().size(), "Regenerated vertex remap lost alignment");
    for (size_t index = 0; index < inputIndices.size(); ++index) {
        require(inputIndices[index] < combinedVertices.size(), "Regenerated vertex input identity is out of bounds");
        near((recombiner.regeneratedVertices()[index] - combinedVertices[inputIndices[index]]).length(), 0);
    }
    auto result = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, true);
    require(result && !result->isNull(), "Ordinary bridge source fixture failed");
    size_t bridges = 0;
    for (const auto& report : result->seamReports)
        bridges += report.bridgeTriangles;
    require(bridges > 0, "No generated bridge source corner was exercised");
    for (const auto& record : result->triangleSourceCorners)
        for (const auto& corner : record.corners) {
            require(corner.weighted && corner.influences().size() == 1 && corner.influences()[0].first == nodeA, "Agreeing explicit incident source did not survive generated bridge transport");
            near(corner.influences()[0].second, 2);
        }
    uniform(*second, true);
    result = dust3d::MeshState::combine(*first, *second, dust3d::MeshCombiner::Method::Union, true);
    size_t unknown = 0;
    for (size_t index = 0; index < recombiner.regeneratedFaceSources().size(); ++index) {
        if (recombiner.regeneratedFaceSources()[index])
            continue;
        for (const auto& corner : result->triangleSourceCorners[index].corners)
            unknown += corner.influences().empty();
    }
    require(unknown > 0, "Generated bridge borrowed a known distribution despite unknown incident ownership");
}
}
int main()
{
    try {
        copyAndBooleanInterpolation();
        unknownAndMissingSources();
        unitHintInterpolationAndPartialUnknown();
        derivedNumericAdmission();
        bridgeAgreement();
        std::cout << "Indexed source transport contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

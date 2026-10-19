// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/source_uv_atlas.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <native/Engine/GenerationPipeline.h>
#include <native/Engine/ImportedModels.h>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void close(double left, double right, const char* message)
{
    require(std::abs(left - right) < 1e-9, message);
}
template <typename Action>
void rejects(Action action, dust3d::UvAtlasErrorCode code)
{
    try {
        action();
    } catch (const dust3d::UvAtlasError& error) {
        require(error.code() == code, "wrong atlas error classification");
        return;
    }
    throw std::runtime_error("expected rejected atlas input");
}
void populate(dust3d::Object& object)
{
    object.vertices = { { 0, 0, 0 }, { 2, 0, 0 }, { 0, 1, 0 } };
    object.triangles = { { 0, 1, 2 }, { 0, 1, 2 }, { 0, 2, 1 } };
    object.triangleModelKeys = { "model", "{model}", "" };
    object.triangleComponentKeys = { "component", "{component}", "legacy" };
    object.triangleCornerUvs = { std::array<dust3d::Vector2, 3> { dust3d::Vector2(-2, 3), dust3d::Vector2(4, 3), dust3d::Vector2(-2, 5) }, std::nullopt, std::nullopt };
}
dust3d::UvAtlas legacy()
{
    dust3d::UvAtlas result;
    result.triangleUvs.resize(3);
    result.triangleUvs[2] = { dust3d::Vector2(.1, .2), dust3d::Vector2(.8, .2), dust3d::Vector2(.1, .9) };
    result.triangleUvKnown = { 0, 0, 1 };
    result.packedTextureSize = 128;
    return result;
}
bool overlaps(const dust3d::SourceUvChart& first, const dust3d::SourceUvChart& second)
{
    return first.left < second.left + second.width && second.left < first.left + first.width
        && first.top < second.top + second.height && second.top < first.top + first.height;
}
void mixedOwnership()
{
    dust3d::Object object;
    populate(object);
    auto original = legacy();
    const auto result = dust3d::SourceUvAtlasBuilder::build(object, original);
    require(result.triangles.size() == 3 && result.legacyChart.has_value(), "mixed source ownership");
    require(result.triangles[0].importedChart && result.triangles[1].importedChart, "coincident imports retain separate charts");
    require(!overlaps(*result.triangles[0].importedChart, *result.triangles[1].importedChart), "coincident chart overlap");
    for (size_t index = 0; index < 2; ++index) {
        require(!overlaps(*result.legacyChart, *result.triangles[index].importedChart), "legacy/import chart overlap");
        require(result.triangles[index].modelKey == object.triangleModelKeys[index], "byte-exact model ownership");
        require(result.triangles[index].componentKey == object.triangleComponentKeys[index], "byte-exact component ownership");
        require(result.triangles[index].known, "packed imported triangle is known");
    }
    const std::array<dust3d::Vector2, 3> expectedLocal { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    for (size_t corner = 0; corner < 3; ++corner) {
        auto mapped = result.triangles[0].importedChart->map(expectedLocal[corner]);
        close(result.triangles[0].packedUvs[corner].x(), mapped.x(), "geometric chart corner x");
        close(result.triangles[0].packedUvs[corner].y(), mapped.y(), "geometric chart corner y");
    }
    require(result.triangles[0].authoredUvs && !result.triangles[1].authoredUvs, "untextured imports do not gain source coordinates");
    close(result.triangles[0].authoredUvs->at(0).x(), -2, "repeat UV retained independently");
    close(result.triangles[0].authoredUvs->at(2).y(), 5, "source UV extent retained");
    for (size_t corner = 0; corner < 3; ++corner) {
        auto transformed = result.legacyChart->map(original.triangleUvs[2][corner]);
        close(result.triangles[2].packedUvs[corner].x(), transformed.x(), "legacy horizontal transform");
        close(result.triangles[2].packedUvs[corner].y(), transformed.y(), "legacy vertical transform");
    }
    for (const auto& triangle : result.triangles)
        for (const auto& uv : triangle.packedUvs)
            require(std::isfinite(uv.x()) && std::isfinite(uv.y()) && uv.x() >= 0 && uv.x() <= 1 && uv.y() >= 0 && uv.y() <= 1, "packed UVs normalized");
    require(object.triangleCornerUvs[0]->at(0).x() == -2 && original.triangleUvs[2][0].x() == .1, "inputs unchanged");
    auto options = dust3d::SourceUvAtlasOptions {};
    options.maximumPayloadBytes = result.payloadBytes;
    require(dust3d::SourceUvAtlasBuilder::build(object, original, options).payloadBytes == result.payloadBytes, "exact payload quota");
    --options.maximumPayloadBytes;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, original, options); }, dust3d::UvAtlasErrorCode::limitExceeded);
    object.triangleModelKeys[0] = "changed";
    require(result.triangles[0].modelKey == "model", "output independently owns identity");
}
void transformsAndLegacy()
{
    dust3d::SourceUvChart chart { .1, .2, .3, .4, false };
    auto point = chart.map({ .25, .75 });
    close(point.x(), .175, "normal chart x");
    close(point.y(), .5, "normal chart y");
    chart.flipped = true;
    point = chart.map({ .25, .75 });
    close(point.x(), .325, "transposed chart x");
    close(point.y(), .3, "transposed chart y");
    dust3d::Object object;
    populate(object);
    object.triangleModelKeys.clear();
    object.triangleCornerUvs.clear();
    auto source = legacy();
    auto result = dust3d::SourceUvAtlasBuilder::build(object, source);
    require(result.legacyChart && result.packedTextureSize == 128, "legacy-only packing unchanged");
    require(!result.triangles[0].known && result.triangles[2].known, "legacy unknown markers preserved");
    close(result.triangles[2].packedUvs[1].x(), .8, "legacy-only UV unchanged");
    auto empty = dust3d::SourceUvAtlasBuilder::build(object, {});
    require(!empty.legacyChart && empty.packedTextureSize == 0 && !empty.triangles[0].known, "unknown remains unknown");
    populate(object);
    object.triangleModelKeys[2] = "third";
    auto imported = dust3d::SourceUvAtlasBuilder::build(object, {});
    require(!imported.legacyChart && imported.triangles[2].importedChart, "all-imported chart supports reversed winding");
}
void malformedInputs()
{
    dust3d::Object object;
    populate(object);
    auto source = legacy();
    auto options = dust3d::SourceUvAtlasOptions {};
    std::atomic_bool cancelled { true };
    options.cancellation = &cancelled;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, options); }, dust3d::UvAtlasErrorCode::cancelled);
    options = {};
    options.maximumChartCount = 1;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, options); }, dust3d::UvAtlasErrorCode::limitExceeded);
    options = {};
    options.maximumPayloadBytes = 1;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, options); }, dust3d::UvAtlasErrorCode::limitExceeded);
    object.triangleModelKeys.pop_back();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangles[0][0] = 99;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1] = object.vertices[0];
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangleCornerUvs[0]->at(1)[0] = std::numeric_limits<double>::infinity();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangleModelKeys[0].clear();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    source.triangleUvs[2][0][1] = 1.1;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
}

void additionalBoundaries()
{
    dust3d::Object object;
    populate(object);
    auto source = legacy();
    for (size_t dimension : { size_t(0), size_t(8193) }) {
        dust3d::SourceUvAtlasOptions options;
        options.textureSize = dimension;
        rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, options); }, dust3d::UvAtlasErrorCode::invalidInput);
    }
    for (size_t count : { size_t(0), size_t(100001) }) {
        dust3d::SourceUvAtlasOptions options;
        options.maximumChartCount = count;
        rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, options); }, dust3d::UvAtlasErrorCode::invalidInput);
    }
    dust3d::SourceUvAtlasOptions quota;
    quota.maximumPayloadBytes = 256 * 1024 * 1024 + 1;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source, quota); }, dust3d::UvAtlasErrorCode::invalidInput);
    dust3d::SourceUvChart chart;
    rejects([&] { chart.map({ -1, 0 }); }, dust3d::UvAtlasErrorCode::invalidInput);
    chart.width = 2;
    rejects([&] { chart.map({ 0, 0 }); }, dust3d::UvAtlasErrorCode::invalidInput);
    object.vertices[0][0] = std::numeric_limits<double>::quiet_NaN();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangles[0].pop_back();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    source.triangleUvKnown[2] = 2;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    source = legacy();
    source.packedTextureSize = 0;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    source = legacy();
    source.triangleUvs.pop_back();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, source); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangleComponentKeys.pop_back();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.triangleCornerUvs.pop_back();
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1][0] = 1e308;
    object.vertices[2][1] = 1e308;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1] = { 1, 1, 0 };
    object.vertices[2] = { 1.7e308, 1.6e308, 0 };
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1][0] = 1e100;
    object.vertices[2][1] = 1e-100;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1][0] = 1e34;
    object.vertices[2][1] = 1e-34;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::packingFailed);
    populate(object);
    object.vertices[1][0] = 1e-162;
    object.vertices[2][1] = 5e-162;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    populate(object);
    object.vertices[1] = { 2e-35, 0, 0 };
    object.vertices[2] = { 5e34, 5e34, 0 };
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::packingFailed);
    populate(object);
    object.vertices[1] = { 1.0 / 6e34, 0, 0 };
    object.vertices[2] = { 6e34, 6e34, 0 };
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
    auto empty = dust3d::SourceUvAtlasBuilder::build(dust3d::Object {}, {});
    require(empty.triangles.empty() && empty.packedTextureSize == 0, "empty geometry remains empty");
    populate(object);
    object.triangles.assign(10, { 0, 1, 2 });
    object.triangleModelKeys.assign(10, "model");
    object.triangleComponentKeys.clear();
    object.triangleCornerUvs.clear();
    object.vertices[1][0] = 1e154;
    object.vertices[2][1] = 1e154;
    rejects([&] { dust3d::SourceUvAtlasBuilder::build(object, {}); }, dust3d::UvAtlasErrorCode::invalidInput);
}
void realImportedFixture(const char* directory)
{
    std::ifstream input(std::string(directory) + "/ImportedSurface.ds3", std::ios::binary);
    require(bool(input), "missing real imported fixture");
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
    std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
    std::string warning;
    dust3d::native::ImportedModels imports;
    auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning, nullptr, &imports);
    dust3d::MeshGenerator generator(snapshot.get());
    snapshot.release(); // The inherited constructor takes ownership.
    generator.setImportedModelData(imports.takeGeometry());
    generator.generate();
    require(generator.isSuccessful(), "actual imported generation failed");
    std::unique_ptr<dust3d::Object> object(generator.takeObject());
    require(object && object->triangles.size() == 4, "real imported fixture triangle count");
    const auto result = dust3d::SourceUvAtlasBuilder::build(*object);
    require(result.triangles.size() == 4 && !result.legacyChart, "real imported atlas ownership");
    for (size_t index = 0; index < result.triangles.size(); ++index) {
        const auto& triangle = result.triangles[index];
        require(triangle.known && triangle.importedChart && triangle.authoredUvs, "real source channels not mapped");
        require(triangle.modelKey == object->triangleModelKeys[index] && triangle.componentKey == object->triangleComponentKeys[index], "real generated identity changed");
        for (size_t corner = 0; corner < 3; ++corner) {
            close(triangle.authoredUvs->at(corner).x(), object->triangleCornerUvs[index]->at(corner).x(), "real authored corner u changed");
            close(triangle.authoredUvs->at(corner).y(), object->triangleCornerUvs[index]->at(corner).y(), "real authored corner v changed");
        }
        for (size_t other = 0; other < index; ++other)
            require(!overlaps(*triangle.importedChart, *result.triangles[other].importedChart), "real imported chart overlap");
    }
}
}
int main(int argc, char** argv)
{
    try {
        mixedOwnership();
        transformsAndLegacy();
        malformedInputs();
        additionalBoundaries();
        require(argc == 2, "fixture directory argument required");
        realImportedFixture(argv[1]);
        std::cout << "Source UV atlas contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerTransport.h>
#include <native/Engine/WorkerUvOperation.h>
#include <sstream>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
std::vector<uint8_t> read(const std::filesystem::path& path)
{
    require(std::filesystem::file_size(path) < 1024 * 1024, "Licensed visual fixture stays small");
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Licensed visual fixture opens");
    return { std::istreambuf_iterator<char>(input), {} };
}
UvMeshPayload roundTrip(const std::string& helper, const UvRequest& value, uint64_t ordinal)
{
    const auto requestBytes = encodeUvRequest(value, {});
    const auto decodedRequest = decodeUvRequest(requestBytes.data(), requestBytes.size(), {});
    require(decodedRequest.archive == value.archive && decodedRequest.textureSize == value.textureSize && decodedRequest.images.size() == value.images.size(), "Request preserves exact archive and resolved option values");
    if (!value.images.empty())
        require(decodedRequest.images[0].identity == value.images[0].identity && decodedRequest.images[0].width == 48 && decodedRequest.images[0].height == 96, "Resolved non-square image dimensions reach the helper without pixels");
    Frame request;
    request.header.operation = Operation::generateUvMesh;
    request.header.identity.document[0] = 7;
    request.header.identity.request = ordinal;
    request.header.identity.revision = 4;
    request.payload = requestBytes;
    request.header.payloadBytes = requestBytes.size();
    const auto expected = generateUvMeshReply(request, {});
    require(expected.header.kind == FrameKind::response, "Ordinary direct visual generation succeeds");
    const auto collected = runSupervisedWorkerJob(helper, request);
    require(collected.outcome.exitedNormally && collected.outcome.exitCode == 0, "Ordinary visual helper exits normally zero");
    const auto reply = decodeFrame(collected.reply.data(), collected.reply.size(), {});
    validateResponseIdentity(reply.header, request.header);
    require(reply.header.kind == FrameKind::response && reply.payload == expected.payload, "Complete visual geometry/UV binary values survive process transport");
    PendingReply pending(request.header);
    auto admitted = pending.complete(request.header.identity, collected.outcome, collected.reply.data(), collected.reply.size());
    auto pair = std::get<UvMeshPayload>(std::move(admitted));
    require(!pair.geometry.vertices.empty() && pair.geometry.vertices.size() == pair.uv.triangles.size() * 3 && !pair.uv.charts.empty(), "Admitted visual pair preserves retained corner alignment and atlas");
    return pair;
}
void licensedVisuals(const std::string& helper, const std::filesystem::path& fixtures)
{
    UvRequest request;
    request.archive = read(fixtures / "Sword.ds3");
    request.textureSize = 128;
    const auto sword = roundTrip(helper, request, 1);
    require(!sword.uv.gradients.empty(), "Sword retains actual native seam gradients");
    bool hasColorTransition = false;
    for (const auto& gradient : sword.uv.gradients) {
        require(gradient.chartIndex < sword.uv.charts.size() && gradient.resolution == 512, "Gradient references retain their native chart and descriptor resolution");
        hasColorTransition = hasColorTransition || gradient.largeColor != gradient.smallColor;
    }
    require(hasColorTransition, "Sword seam preserves distinct native endpoint colors");
    request.archive = read(fixtures / "RoundedCrossing.ds3");
    const auto crossing = roundTrip(helper, request, 2);
    const auto known = std::count_if(crossing.uv.triangles.begin(), crossing.uv.triangles.end(), [](const auto& triangle) { return triangle.known != 0; });
    require(known > 0 && static_cast<size_t>(known) < crossing.uv.triangles.size(), "Boolean result retains mapped and explicitly unmapped triangles");
    for (const auto& triangle : crossing.uv.triangles) {
        if (!triangle.known)
            for (const auto& corner : triangle.uv)
                require(corner[0] == 0 && corner[1] == 0, "Unmapped Boolean corners cannot invent UVs");
        require(triangle.association == UvAssociation::component ? triangle.componentIndex < crossing.uv.components.size() : triangle.componentIndex == noUvComponent, "Source association remains independent of chart availability");
    }
}
void imageVisual(const std::string& helper)
{
    // Independent synthetic MIT document, using the established single-node schema.
    const std::string component = "{zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz}";
    const std::string image = "{abcdef01-2345-6789-abcd-0123456789ab}";
    const std::string part = "{00000100-0000-4000-8000-000000000100}";
    const auto xml = "<canvas originX='0' originY='0' originZ='0'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='" + part
        + "' x='0' y='0' z='0' radius='.1'/></nodes><edges/><parts><part id='" + part
        + "'/></parts><components><component id='" + component + "' linkDataType='partId' linkData='" + part
        + "' color='#8000FF00' colorImageId='" + image + "'/></components></canvas>";
    const auto header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream prefix;
    prefix << "DUST3D 1.0 xml " << std::setw(10) << std::setfill('0') << (26 + header.size()) << '\n';
    require(prefix.str().size() == 26, "Independent archive prefix width is exact");
    const auto archive = prefix.str() + header + xml;
    UvRequest request;
    request.archive = { archive.begin(), archive.end() };
    request.textureSize = 128;
    request.images = { { image.substr(1, 36), 48, 96 } };
    const auto pair = roundTrip(helper, request, 3);
    require(pair.uv.components == std::vector<std::string>({ component }), "Original opaque legacy component spelling survives generation and transport");
    bool retainedImage = false;
    for (const auto& chart : pair.uv.charts) {
        if (chart.imageID == image) {
            retainedImage = true;
            require(chart.color == std::array<double, 4>({ 0, 1, 0, 128.0 / 255 }), "Image-backed chart retains source alpha-first RGBA without forced opacity");
            require(chart.association == UvAssociation::component && chart.componentIndex == 0, "Image-backed chart keeps its original component association");
        }
    }
    require(retainedImage, "Braced document image matches the same unbraced descriptor identity");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Fresh helper and licensed fixture directory required");
        licensedVisuals(argv[1], argv[2]);
        imageVisual(argv[1]);
        std::cout << "Worker UV visual acceptance passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

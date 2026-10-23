// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <native/Engine/WorkerMesh.h>
#include <stdexcept>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void reject(MeshPayloadErrorCode code, Action action)
{
    try {
        action();
    } catch (const MeshPayloadError& cause) {
        require(cause.code() == code && cause.what()[0], "Mesh payload errors must retain a typed cause");
        return;
    }
    throw std::runtime_error("Invalid worker mesh payload was admitted");
}
MeshPayload triangle()
{
    MeshPayload result;
    result.nodeCount = 2;
    result.partCount = 1;
    result.warning = "ok\n";
    result.vertices = {
        { { 0, 0, 0 }, { 0, 0, 1 }, { 1, 0, 0, 1 } },
        { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0, 1 } },
        { { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 1, 1 } }
    };
    return result;
}
void same(const MeshPayload& first, const MeshPayload& second)
{
    require(first.nodeCount == second.nodeCount && first.partCount == second.partCount && first.warning == second.warning && first.vertices.size() == second.vertices.size(), "Mesh metadata must survive transport");
    for (size_t index = 0; index < first.vertices.size(); ++index) {
        const auto& a = first.vertices[index];
        const auto& b = second.vertices[index];
        require(std::memcmp(a.position.data(), b.position.data(), 3 * sizeof(float)) == 0
                && std::memcmp(a.normal.data(), b.normal.data(), 3 * sizeof(float)) == 0
                && std::memcmp(a.color.data(), b.color.data(), 4 * sizeof(float)) == 0,
            "All geometry float bits must survive transport");
    }
}
void wireAndOwnership()
{
    auto original = triangle();
    auto wire = encodeMeshPayload(original, {});
    const std::array<uint8_t, 40> prefix {
        0, 1, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
        0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 1,
        0, 0, 0, 40, 0, 0, 0, 3
    };
    require(wire.size() == 163 && std::equal(prefix.begin(), prefix.end(), wire.begin()), "Independent mesh prefix fixture must match wire layout");
    require(wire[40] == 'o' && wire[41] == 'k' && wire[42] == '\n'
            && wire[43 + 20] == 0x3f && wire[43 + 21] == 0x80 && wire[43 + 22] == 0 && wire[43 + 23] == 0,
        "Warnings and IEEE normal components use explicit wire bytes");
    auto decoded = decodeMeshPayload(wire.data(), wire.size(), {});
    same(original, decoded);
    original.vertices.clear();
    original.warning.clear();
    wire.assign(wire.size(), 0);
    same(triangle(), decoded);
    auto negativeZero = triangle();
    negativeZero.vertices[0].position[0] = -0.0f;
    auto bytes = encodeMeshPayload(negativeZero, {});
    same(negativeZero, decodeMeshPayload(bytes.data(), bytes.size(), {}));
    auto unicode = triangle();
    unicode.warning = "\xc2\xa3 \xe2\x82\xac \xf0\x9f\x8c\x8e\t\r\n";
    bytes = encodeMeshPayload(unicode, {});
    same(unicode, decodeMeshPayload(bytes.data(), bytes.size(), {}));
    unicode.warning.clear();
    bytes = encodeMeshPayload(unicode, {});
    same(unicode, decodeMeshPayload(bytes.data(), bytes.size(), {}));
}
void boundsAndLayout()
{
    const auto value = triangle();
    const auto wire = encodeMeshPayload(value, {});
    MeshPayloadLimits limits;
    limits.maximumWireBytes = wire.size();
    limits.maximumDecodedBytes = value.vertices.size() * sizeof(MeshVertex) + value.warning.size();
    limits.maximumVertices = 3;
    limits.maximumNodeCount = 2;
    limits.maximumPartCount = 1;
    limits.maximumWarningBytes = 3;
    same(value, decodeMeshPayload(wire.data(), wire.size(), limits));
    auto less = limits;
    --less.maximumWireBytes;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { decodeMeshPayload(wire.data(), wire.size(), less); });
    less = limits;
    --less.maximumDecodedBytes;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { encodeMeshPayload(value, less); });
    reject(MeshPayloadErrorCode::limitExceeded, [&] { decodeMeshPayload(wire.data(), wire.size(), less); });
    less = limits;
    less.maximumVertices = 2;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { encodeMeshPayload(value, less); });
    less = limits;
    less.maximumNodeCount = 1;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { encodeMeshPayload(value, less); });
    less = limits;
    less.maximumWarningBytes = 2;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { encodeMeshPayload(value, less); });
    auto belowPrefix = limits;
    belowPrefix.maximumWireBytes = 1;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { decodeMeshPayload(wire.data(), wire.size(), belowPrefix); });
    auto belowWarning = limits;
    belowWarning.maximumDecodedBytes = 1;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { decodeMeshPayload(wire.data(), wire.size(), belowWarning); });
    auto tooManyParts = value;
    tooManyParts.partCount = 2;
    reject(MeshPayloadErrorCode::limitExceeded, [&] { encodeMeshPayload(tooManyParts, limits); });
    auto invalid = limits;
    invalid.maximumVertices = 0;
    reject(MeshPayloadErrorCode::invalidLimits, [&] { encodeMeshPayload(value, invalid); });
    invalid = limits;
    invalid.maximumWireBytes = maximumPayloadBytes + 1;
    reject(MeshPayloadErrorCode::invalidLimits, [&] { decodeMeshPayload(wire.data(), wire.size(), invalid); });
    using LimitField = uint64_t MeshPayloadLimits::*;
    struct LimitCase {
        LimitField field;
        uint64_t maximum;
    };
    for (const auto& field : { LimitCase { &MeshPayloadLimits::maximumWireBytes, maximumPayloadBytes },
             LimitCase { &MeshPayloadLimits::maximumDecodedBytes, maximumPayloadBytes },
             LimitCase { &MeshPayloadLimits::maximumVertices, maximumMeshVertices },
             LimitCase { &MeshPayloadLimits::maximumNodeCount, maximumSourceCount },
             LimitCase { &MeshPayloadLimits::maximumPartCount, maximumSourceCount },
             LimitCase { &MeshPayloadLimits::maximumWarningBytes, maximumDiagnosticBytes } })
        for (const auto outside : { uint64_t { 0 }, field.maximum + 1 }) {
            auto bad = limits;
            bad.*(field.field) = outside;
            reject(MeshPayloadErrorCode::invalidLimits, [&] { decodeMeshPayload(wire.data(), wire.size(), bad); });
        }
    for (size_t size = 0; size < wire.size(); ++size)
        reject(MeshPayloadErrorCode::truncated, [&] { decodeMeshPayload(wire.data(), size, {}); });
    reject(MeshPayloadErrorCode::truncated, [&] { decodeMeshPayload(nullptr, 0, {}); });
    reject(MeshPayloadErrorCode::invalidInput, [&] { decodeMeshPayload(nullptr, wire.size(), {}); });
    auto changed = wire;
    changed.push_back(0);
    reject(MeshPayloadErrorCode::trailingData, [&] { decodeMeshPayload(changed.data(), changed.size(), {}); });
    auto mutate = [&](size_t offset, uint8_t byte, MeshPayloadErrorCode code) {
        auto bytes = wire;
        bytes[offset] = byte;
        reject(code, [&] { decodeMeshPayload(bytes.data(), bytes.size(), {}); });
    };
    mutate(1, 2, MeshPayloadErrorCode::unsupportedVersion);
    mutate(3, 39, MeshPayloadErrorCode::invalidLayout);
    mutate(7, 1, MeshPayloadErrorCode::invalidLayout);
    mutate(35, 39, MeshPayloadErrorCode::invalidLayout);
    mutate(8, 255, MeshPayloadErrorCode::limitExceeded);
    mutate(16, 255, MeshPayloadErrorCode::limitExceeded);
    mutate(24, 255, MeshPayloadErrorCode::limitExceeded);
    mutate(36, 255, MeshPayloadErrorCode::limitExceeded);
    mutate(15, 0, MeshPayloadErrorCode::invalidGeometry);
    mutate(15, 2, MeshPayloadErrorCode::invalidGeometry);
}
void geometryAndText()
{
    const auto original = triangle();
    auto invalid = original;
    invalid.vertices.clear();
    reject(MeshPayloadErrorCode::invalidGeometry, [&] { encodeMeshPayload(invalid, {}); });
    invalid = original;
    invalid.vertices.pop_back();
    reject(MeshPayloadErrorCode::invalidGeometry, [&] { encodeMeshPayload(invalid, {}); });
    auto rejectVertex = [&](const MeshVertex& vertex) {
        auto changed = original;
        changed.vertices[0] = vertex;
        reject(MeshPayloadErrorCode::invalidGeometry, [&] { encodeMeshPayload(changed, {}); });
    };
    auto vertex = original.vertices[0];
    vertex.position[0] = std::numeric_limits<float>::infinity();
    rejectVertex(vertex);
    vertex = original.vertices[0];
    vertex.normal = { 0, 0, 0 };
    rejectVertex(vertex);
    vertex.normal = { 0, 0, 2 };
    rejectVertex(vertex);
    vertex.normal[0] = std::numeric_limits<float>::quiet_NaN();
    rejectVertex(vertex);
    vertex = original.vertices[0];
    vertex.color[0] = -0.1f;
    rejectVertex(vertex);
    vertex.color[0] = 1.1f;
    rejectVertex(vertex);
    vertex.color[0] = std::numeric_limits<float>::quiet_NaN();
    rejectVertex(vertex);
    invalid = original;
    invalid.vertices[2].position = invalid.vertices[1].position;
    reject(MeshPayloadErrorCode::invalidGeometry, [&] { encodeMeshPayload(invalid, {}); });
    for (const auto& text : { std::string("a\0b", 3), std::string("\x1b"), std::string("\x7f"), std::string("\xc0\x80"), std::string("\xe2\x82"),
             std::string("\xe2x\xac"), std::string("\xe0\x80\x80"), std::string("\xed\xa0\x80"), std::string("\xf4\x90\x80\x80"), std::string("\x80") }) {
        invalid = original;
        invalid.warning = text;
        reject(MeshPayloadErrorCode::invalidText, [&] { encodeMeshPayload(invalid, {}); });
    }
    auto noCatalog = original;
    noCatalog.nodeCount = noCatalog.partCount = 0;
    auto bytes = encodeMeshPayload(noCatalog, {});
    same(noCatalog, decodeMeshPayload(bytes.data(), bytes.size(), {}));
    auto rejectWireGeometry = [&](size_t offset, uint8_t first, uint8_t second) {
        auto changed = encodeMeshPayload(original, {});
        changed[offset] = first;
        changed[offset + 1] = second;
        reject(MeshPayloadErrorCode::invalidGeometry, [&] { decodeMeshPayload(changed.data(), changed.size(), {}); });
    };
    rejectWireGeometry(43 + 20, 0x40, 0); // Nonunit normal, 2.0.
    rejectWireGeometry(43 + 24, 0xbf, 0x80); // Color -1.0.
    auto collapsed = encodeMeshPayload(original, {});
    std::fill(collapsed.begin() + 43 + 80 + 4, collapsed.begin() + 43 + 80 + 8, 0);
    reject(MeshPayloadErrorCode::invalidGeometry, [&] { decodeMeshPayload(collapsed.data(), collapsed.size(), {}); });
    auto wire = encodeMeshPayload(original, {});
    wire[40] = 0;
    reject(MeshPayloadErrorCode::invalidText, [&] { decodeMeshPayload(wire.data(), wire.size(), {}); });
    wire = encodeMeshPayload(original, {});
    wire[43] = 0x7f;
    wire[44] = 0x80;
    reject(MeshPayloadErrorCode::invalidGeometry, [&] { decodeMeshPayload(wire.data(), wire.size(), {}); });
}
void realEngine(const char* directory)
{
    std::ifstream file(std::string(directory) + "/Capsule.ds3", std::ios::binary);
    require(bool(file), "Licensed Capsule fixture required");
    const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> owner(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(owner != nullptr, error.message);
    MeshPayload payload;
    payload.nodeCount = dust3d_mesh_node_count(owner.get());
    payload.partCount = dust3d_mesh_part_count(owner.get());
    payload.warning = dust3d_mesh_warning(owner.get());
    const auto* vertices = dust3d_mesh_vertices(owner.get());
    for (size_t index = 0; index < dust3d_mesh_vertex_count(owner.get()); ++index) {
        const auto& input = vertices[index];
        payload.vertices.push_back({ { input.position[0], input.position[1], input.position[2] },
            { input.normal[0], input.normal[1], input.normal[2] }, { input.color[0], input.color[1], input.color[2], input.color[3] } });
    }
    owner.reset();
    auto wire = encodeMeshPayload(payload, {});
    auto copied = decodeMeshPayload(wire.data(), wire.size(), {});
    same(payload, copied);
    Header request;
    request.identity.document[0] = 1;
    request.identity.request = 7;
    request.identity.revision = 3;
    request.payloadBytes = bytes.size();
    auto response = request;
    response.kind = FrameKind::response;
    response.payloadBytes = wire.size();
    auto frameBytes = encodeFrame({ response, wire }, {});
    auto framed = decodeFrame(frameBytes.data(), frameBytes.size(), {});
    validateResponseIdentity(framed.header, request);
    same(payload, decodeMeshPayload(framed.payload.data(), framed.payload.size(), {}));
    frameBytes.clear();
    framed.payload.clear();
    wire.clear();
    require(copied.vertices.size() == 240 && copied.nodeCount && copied.partCount, "Real worker payload retains Capsule geometry after owner release");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture path required");
        wireAndOwnership();
        boundsAndLayout();
        geometryAndText();
        realEngine(argv[1]);
        std::cout << "Worker semantic mesh payload contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

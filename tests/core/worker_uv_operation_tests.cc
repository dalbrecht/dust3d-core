// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerUvOperation.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Licensed Capsule fixture required");
        std::ifstream input(argv[1], std::ios::binary);
        require(bool(input), "Fixture opens");
        UvRequest request;
        request.archive = { std::istreambuf_iterator<char>(input), {} };
        request.textureSize = 64;
        auto generated = generateUvMesh(request, {});
        require(std::holds_alternative<UvMeshPayload>(generated), "Ordinary native UV generation succeeds");
        const auto& pair = std::get<UvMeshPayload>(generated);
        require(pair.geometry.vertices.size() == 240 && pair.uv.triangles.size() == 80 && !pair.uv.charts.empty() && !pair.uv.components.empty(), "Same-owner geometry and UVs retain actual corner order");
        auto wire = encodeUvMeshPayload(pair, {});
        auto copy = decodeUvMeshPayload(wire.data(), wire.size(), {});
        require(encodeUvMeshPayload(copy, {}) == wire, "Complete pair remains independently owned after native owner destruction");
        UvMeshPayloadLimits exact;
        const auto meshWire = encodeMeshPayload(pair.geometry, {});
        const auto uvWire = encodeUvPayload(pair.uv, 80, {});
        exact.maximumWireBytes = wire.size();
        exact.maximumDecodedBytes = inspectMeshPayload(meshWire.data(), meshWire.size(), {}).logicalBytes + inspectUvPayload(uvWire.data(), uvWire.size(), 80, {});
        require(std::holds_alternative<UvMeshPayload>(generateUvMesh(request, exact)), "Exact aggregate quota accepts ordinary fixture");
        for (int field = 0; field < 5; ++field) {
            auto limited = exact;
            if (field == 0)
                --limited.maximumWireBytes;
            if (field == 1)
                --limited.maximumDecodedBytes;
            if (field == 2)
                limited.uv.maximumTriangles = 1;
            if (field == 3)
                limited.uv.maximumDecodedBytes = 1;
            if (field == 4)
                limited.geometry.maximumVertices = 1;
            const auto result = generateUvMesh(request, limited);
            require(std::holds_alternative<FailurePayload>(result) && std::get<FailurePayload>(result).status == FailureStatus::limitExceeded, "Small output-copy quotas return typed limit without partial pair");
        }
        auto invalid = request;
        invalid.textureSize = 8193;
        try {
            generateUvMesh(invalid, {});
            throw std::runtime_error("Invalid options reached generation");
        } catch (const UvRequestError& error) {
            require(error.code() == UvRequestErrorCode::invalidOptions, "Pure request cause preserved before native execution");
        }
        UvMeshPayloadLimits invalidLimits;
        invalidLimits.maximumDecodedBytes = 0;
        try {
            generateUvMesh(request, invalidLimits);
            throw std::runtime_error("Invalid policy reached generation");
        } catch (const UvMeshPayloadError& error) {
            require(error.code() == UvMeshPayloadErrorCode::invalidLimits, "Pure policy cause preserved before native execution");
        }
        std::cout << "Same-owner native UV generation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

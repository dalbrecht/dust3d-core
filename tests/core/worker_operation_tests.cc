// SPDX-License-Identifier: MIT
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerOperation.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
Frame request(std::vector<uint8_t> bytes)
{
    Header header;
    header.identity.document[0] = 1;
    header.identity.request = 12;
    header.identity.revision = 2;
    header.payloadBytes = bytes.size();
    return { header, std::move(bytes) };
}
void mapping()
{
    struct Outcome {
        Dust3DStatus native;
        FailureStatus wire;
    };
    for (const auto& outcome : { Outcome { DUST3D_STATUS_INVALID_DOCUMENT, FailureStatus::invalidDocument },
             Outcome { DUST3D_STATUS_UNSUPPORTED_ASSET, FailureStatus::unsupportedAsset },
             Outcome { DUST3D_STATUS_GENERATION_FAILED, FailureStatus::generationFailed },
             Outcome { DUST3D_STATUS_LIMIT_EXCEEDED, FailureStatus::limitExceeded },
             Outcome { DUST3D_STATUS_NO_GEOMETRY, FailureStatus::noGeometry },
             Outcome { DUST3D_STATUS_CANCELLED, FailureStatus::cancelled } }) {
        Dust3DError error {};
        error.status = outcome.native;
        error.message[0] = 'a';
        error.message[1] = static_cast<char>(255);
        const auto copied = copyNativeFailure(error);
        require(copied.status == outcome.wire && copied.cause == std::vector<uint8_t>({ 'a', 255 }), "Every native non-success status maps explicitly without interpreting its cause");
        std::fill(std::begin(error.message), std::end(error.message), 'x');
        require(copyNativeFailure(error).cause.size() == sizeof(error.message), "A malformed nonterminated native diagnostic must remain bounded to its actual array");
    }
    Dust3DError unknown {};
    // Seven lies within this C enum's representable range but is not a known status.
    unknown.status = static_cast<Dust3DStatus>(7);
    unknown.message[0] = 'x';
    const auto copied = copyNativeFailure(unknown);
    require(copied.status == FailureStatus::generationFailed && copied.cause.back() == 'x'
            && std::string(copied.cause.begin(), copied.cause.end()).find("status 7") != std::string::npos,
        "Unknown native statuses preserve their original numeric status and cause");
    unknown.status = DUST3D_STATUS_OK;
    try {
        copyNativeFailure(unknown);
        throw std::runtime_error("Success cannot be converted into a native failure");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::invalidInput, "Invalid native failure input has a typed cause");
    }
}
void execution(const char* fixtures)
{
    std::ifstream file(std::string(fixtures) + "/Capsule.ds3", std::ios::binary);
    require(bool(file), "Licensed Capsule fixture required");
    auto input = request({ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() });
    auto output = generateMeshReply(input, {});
    validateResponseIdentity(output.header, input.header);
    require(output.header.kind == FrameKind::response && decodeMeshPayload(output.payload.data(), output.payload.size(), {}).vertices.size() == 240,
        "Native operation generates an owned real mesh response");
    for (const auto& limits : { MeshPayloadLimits { maximumPayloadBytes, 1, 3000000, maximumSourceCount, maximumSourceCount, 4096 },
             MeshPayloadLimits { 1, 128ULL * 1024 * 1024, 3000000, maximumSourceCount, maximumSourceCount, 4096 },
             MeshPayloadLimits { maximumPayloadBytes, 128ULL * 1024 * 1024, 1, maximumSourceCount, maximumSourceCount, 4096 } }) {
        const auto limited = generateMeshReply(input, limits);
        require(limited.header.kind == FrameKind::failure && decodeFailurePayload(limited.payload.data(), limited.payload.size(), {}).status == FailureStatus::limitExceeded,
            "Small consumer output budgets return typed limits after ordinary bounded generation");
    }
    auto badArchive = request({ 1, 2, 3 });
    const auto rejected = generateMeshReply(badArchive, {});
    require(rejected.header.kind == FrameKind::failure && decodeFailurePayload(rejected.payload.data(), rejected.payload.size(), {}).status == FailureStatus::invalidDocument,
        "Native input failures retain their operation status");
    auto wrongLength = input;
    ++wrongLength.header.payloadBytes;
    try {
        generateMeshReply(wrongLength, {});
        throw std::runtime_error("Mismatched request length was admitted");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::invalidInput, "Length mismatch has a typed cause before execution");
    }
    auto wrongKind = input;
    wrongKind.header.kind = FrameKind::response;
    try {
        generateMeshReply(wrongKind, {});
        throw std::runtime_error("Response was admitted as a request");
    } catch (const ProtocolError& cause) {
        require(cause.code() == ProtocolErrorCode::unexpectedResponse, "Wrong request kind has a typed cause");
    }
    MeshPayloadLimits invalid;
    invalid.maximumVertices = 0;
    try {
        generateMeshReply(input, invalid);
        throw std::runtime_error("Invalid output policy was admitted");
    } catch (const MeshPayloadError& cause) {
        require(cause.code() == MeshPayloadErrorCode::invalidLimits, "Output policy validates before native generation");
    }
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture path required");
        mapping();
        execution(argv[1]);
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

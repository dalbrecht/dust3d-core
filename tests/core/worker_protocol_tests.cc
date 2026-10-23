// SPDX-License-Identifier: MIT
#include <algorithm>
#include <array>
#include <iostream>
#include <native/Engine/WorkerProtocol.h>
#include <stdexcept>
#include <vector>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void reject(ProtocolErrorCode code, Action action)
{
    try {
        action();
    } catch (const ProtocolError& cause) {
        require(cause.code() == code && cause.what()[0], "Protocol errors retain their typed cause");
        return;
    }
    throw std::runtime_error("Invalid worker frame was admitted");
}
Header header(FrameKind kind = FrameKind::request)
{
    Header result;
    result.kind = kind;
    result.operation = Operation::generateMesh;
    result.identity.request = 0x0102030405060708;
    result.identity.revision = 0x1112131415161718;
    for (size_t index = 0; index < result.identity.document.size(); ++index)
        result.identity.document[index] = static_cast<uint8_t>(index + 1);
    result.payloadBytes = 3;
    return result;
}
void wireAndOwnership()
{
    const Limits limits { 3, 3, 3 };
    const auto original = header();
    const auto wire = encodeHeader(original, limits);
    // Independent wire fixture: network byte order, fixed version/size, no native struct dump.
    const std::array<uint8_t, 64> expected {
        'D', 'S', '3', 'W', 'O', 'R', 'K', 0,
        0, 1, 0, 64, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0,
        1, 2, 3, 4, 5, 6, 7, 8, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0, 0, 0, 0, 0, 3
    };
    require(wire == expected, "Worker wire header must match the independent endian fixture");
    const auto decoded = decodeHeader(wire.data(), wire.size(), limits);
    require(decoded.identity == original.identity && decoded.kind == original.kind && decoded.operation == original.operation && decoded.payloadBytes == 3,
        "Header fields round-trip exactly");
    Frame input { original, { 0, 128, 255 } };
    auto encoded = encodeFrame(input, limits);
    auto copied = decodeFrame(encoded.data(), encoded.size(), limits);
    input.payload.assign(3, 42);
    encoded.assign(encoded.size(), 19);
    require(copied.payload == std::vector<uint8_t>({ 0, 128, 255 }), "Decoded payload owns its bytes independently");
    auto empty = header();
    empty.payloadBytes = 0;
    const auto emptyWire = encodeFrame({ empty, {} }, limits);
    require(decodeFrame(emptyWire.data(), emptyWire.size(), limits).payload.empty(), "Empty framed payload remains a semantic-layer decision");
}
void boundedAdmission()
{
    const Limits limits { 3, 4, 2 };
    auto value = header();
    value.payloadBytes = 4;
    reject(ProtocolErrorCode::limitExceeded, [&] { encodeHeader(value, limits); });
    value.kind = FrameKind::response;
    encodeHeader(value, limits);
    value.kind = FrameKind::failure;
    reject(ProtocolErrorCode::limitExceeded, [&] { encodeHeader(value, limits); });
    value.payloadBytes = 2;
    encodeHeader(value, limits);
    auto wire = encodeHeader(header(), limits);
    wire[63] = 4;
    reject(ProtocolErrorCode::limitExceeded, [&] { decodeHeader(wire.data(), wire.size(), limits); });
    // Only the fixed header is present: hostile advertised length is rejected before body allocation.
    std::fill(wire.begin() + 56, wire.end(), 255);
    reject(ProtocolErrorCode::limitExceeded, [&] { decodeHeader(wire.data(), wire.size(), limits); });
    for (const auto& invalid : { Limits { 0, 1, 1 }, Limits { maximumPayloadBytes + 1, 1, 1 },
             Limits { 1, 0, 1 }, Limits { 1, maximumPayloadBytes + 1, 1 }, Limits { 1, 1, 0 },
             Limits { 1, 1, maximumDiagnosticBytes + 1 } })
        reject(ProtocolErrorCode::invalidLimits, [&] { encodeHeader(header(), invalid); });
}
void malformedFrames()
{
    const auto wire = encodeHeader(header(), {});
    reject(ProtocolErrorCode::truncated, [&] { decodeHeader(nullptr, 0, {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(ProtocolErrorCode::truncated, [&] { decodeHeader(wire.data(), size, {}); });
    reject(ProtocolErrorCode::invalidInput, [&] { decodeHeader(nullptr, wire.size(), {}); });
    std::vector<uint8_t> extended(wire.begin(), wire.end());
    extended.push_back(0);
    reject(ProtocolErrorCode::trailingData, [&] { decodeHeader(extended.data(), extended.size(), {}); });
    reject(ProtocolErrorCode::invalidInput, [&] { decodeFrame(nullptr, headerBytes, {}); });
    reject(ProtocolErrorCode::invalidLimits, [&] { decodeHeader(wire.data(), wire.size(), { 0, 1, 1 }); });
    auto unknown = header();
    unknown.kind = static_cast<FrameKind>(4);
    reject(ProtocolErrorCode::unsupportedKind, [&] { encodeHeader(unknown, {}); });
    unknown = header();
    unknown.operation = static_cast<Operation>(2);
    reject(ProtocolErrorCode::unsupportedOperation, [&] { encodeHeader(unknown, {}); });
    auto mutate = [&](size_t offset, uint8_t value, ProtocolErrorCode code) {
        auto changed = wire;
        changed[offset] = value;
        reject(code, [&] { decodeHeader(changed.data(), changed.size(), {}); });
    };
    mutate(0, 'X', ProtocolErrorCode::invalidHeader);
    mutate(7, 1, ProtocolErrorCode::invalidHeader);
    mutate(9, 2, ProtocolErrorCode::unsupportedVersion);
    mutate(11, 63, ProtocolErrorCode::invalidHeader);
    mutate(15, 1, ProtocolErrorCode::invalidHeader);
    mutate(23, 1, ProtocolErrorCode::invalidHeader);
    mutate(17, 4, ProtocolErrorCode::unsupportedKind);
    mutate(19, 2, ProtocolErrorCode::unsupportedOperation);
    auto missingIdentity = header();
    missingIdentity.identity.request = 0;
    reject(ProtocolErrorCode::invalidIdentity, [&] { encodeHeader(missingIdentity, {}); });
    missingIdentity = header();
    missingIdentity.identity.document.fill(0);
    reject(ProtocolErrorCode::invalidIdentity, [&] { encodeHeader(missingIdentity, {}); });
    auto frame = encodeFrame({ header(), { 1, 2, 3 } }, {});
    for (size_t size = wire.size(); size < frame.size(); ++size)
        reject(ProtocolErrorCode::truncated, [&] { decodeFrame(frame.data(), size, {}); });
    frame.push_back(0);
    reject(ProtocolErrorCode::trailingData, [&] { decodeFrame(frame.data(), frame.size(), {}); });
    reject(ProtocolErrorCode::invalidInput, [&] { encodeFrame({ header(), { 1, 2 } }, {}); });
}
void responseIdentity()
{
    const auto request = header();
    auto response = header(FrameKind::response);
    validateResponseIdentity(response, request);
    response.kind = FrameKind::failure;
    validateResponseIdentity(response, request);
    response.kind = FrameKind::request;
    reject(ProtocolErrorCode::unexpectedResponse, [&] { validateResponseIdentity(response, request); });
    response = header(FrameKind::response);
    ++response.identity.request;
    reject(ProtocolErrorCode::staleResponse, [&] { validateResponseIdentity(response, request); });
    response = header(FrameKind::response);
    ++response.identity.revision;
    reject(ProtocolErrorCode::staleResponse, [&] { validateResponseIdentity(response, request); });
    response = header(FrameKind::response);
    ++response.identity.document[0];
    reject(ProtocolErrorCode::staleResponse, [&] { validateResponseIdentity(response, request); });
    response = header(FrameKind::response);
    auto wrongRequest = request;
    wrongRequest.kind = FrameKind::response;
    reject(ProtocolErrorCode::unexpectedResponse, [&] { validateResponseIdentity(response, wrongRequest); });
    auto initial = header();
    initial.identity.revision = 0;
    encodeHeader(initial, {});
}
}
int main()
{
    try {
        wireAndOwnership();
        boundedAdmission();
        malformedFrames();
        responseIdentity();
        std::cout << "Worker frame protocol contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

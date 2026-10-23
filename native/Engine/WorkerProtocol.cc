// SPDX-License-Identifier: MIT
#include "WorkerProtocol.h"
#include <algorithm>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        constexpr std::array<uint8_t, 8> magic { 'D', 'S', '3', 'W', 'O', 'R', 'K', 0 };
        [[noreturn]] void reject(ProtocolErrorCode code, const char* message)
        {
            throw ProtocolError(code, message);
        }
        void validateLimits(const Limits& limits)
        {
            if (!limits.maximumInputBytes || limits.maximumInputBytes > maximumPayloadBytes
                || !limits.maximumOutputBytes || limits.maximumOutputBytes > maximumPayloadBytes
                || !limits.maximumFailureBytes || limits.maximumFailureBytes > maximumDiagnosticBytes)
                reject(ProtocolErrorCode::invalidLimits, "Worker payload limits are outside their supported range.");
        }
        void validateFields(const Header& header)
        {
            if (header.kind != FrameKind::request && header.kind != FrameKind::response && header.kind != FrameKind::failure)
                reject(ProtocolErrorCode::unsupportedKind, "Unknown worker frame kind.");
            if (header.operation != Operation::generateMesh)
                reject(ProtocolErrorCode::unsupportedOperation, "Unknown worker operation.");
            if (!header.identity.request || std::all_of(header.identity.document.begin(), header.identity.document.end(), [](uint8_t value) { return value == 0; }))
                reject(ProtocolErrorCode::invalidIdentity, "Worker frames require a document and nonzero request identity.");
        }
        void validate(const Header& header, const Limits& limits)
        {
            validateLimits(limits);
            validateFields(header);
            const auto maximum = header.kind == FrameKind::request ? limits.maximumInputBytes : header.kind == FrameKind::response ? limits.maximumOutputBytes
                                                                                                                                   : limits.maximumFailureBytes;
            if (header.payloadBytes > maximum || header.payloadBytes > std::numeric_limits<size_t>::max() - headerBytes)
                reject(ProtocolErrorCode::limitExceeded, "Worker advertised payload exceeds its configured limit.");
        }
        void write(uint8_t* output, uint64_t value, size_t count)
        {
            for (size_t index = 0; index < count; ++index) {
                output[count - index - 1] = static_cast<uint8_t>(value & 255);
                value >>= 8;
            }
        }
        uint64_t read(const uint8_t* input, size_t count)
        {
            uint64_t result = 0;
            for (size_t index = 0; index < count; ++index)
                result = (result << 8) | input[index];
            return result;
        }
    }
    ProtocolError::ProtocolError(ProtocolErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    bool RequestIdentity::operator==(const RequestIdentity& other) const noexcept
    {
        return document == other.document && request == other.request && revision == other.revision;
    }
    std::array<uint8_t, headerBytes> encodeHeader(const Header& header, const Limits& limits)
    {
        validate(header, limits);
        std::array<uint8_t, headerBytes> result {};
        std::copy(magic.begin(), magic.end(), result.begin());
        write(result.data() + 8, 1, 2);
        write(result.data() + 10, headerBytes, 2);
        write(result.data() + 16, static_cast<uint16_t>(header.kind), 2);
        write(result.data() + 18, static_cast<uint16_t>(header.operation), 2);
        write(result.data() + 24, header.identity.request, 8);
        write(result.data() + 32, header.identity.revision, 8);
        std::copy(header.identity.document.begin(), header.identity.document.end(), result.begin() + 40);
        write(result.data() + 56, header.payloadBytes, 8);
        return result;
    }
    Header decodeHeader(const uint8_t* bytes, size_t size, const Limits& limits)
    {
        validateLimits(limits);
        if (size < headerBytes)
            reject(ProtocolErrorCode::truncated, "Worker frame header is incomplete.");
        if (!bytes)
            reject(ProtocolErrorCode::invalidInput, "Worker header bytes are missing.");
        if (size != headerBytes)
            reject(ProtocolErrorCode::trailingData, "Worker header span contains extra bytes.");
        if (!std::equal(magic.begin(), magic.end(), bytes))
            reject(ProtocolErrorCode::invalidHeader, "Worker frame magic is invalid.");
        if (read(bytes + 8, 2) != 1)
            reject(ProtocolErrorCode::unsupportedVersion, "Worker protocol version is unsupported.");
        if (read(bytes + 10, 2) != headerBytes || read(bytes + 12, 4) || read(bytes + 20, 4))
            reject(ProtocolErrorCode::invalidHeader, "Worker header size, flags or reserved fields are invalid.");
        Header result;
        result.kind = static_cast<FrameKind>(read(bytes + 16, 2));
        result.operation = static_cast<Operation>(read(bytes + 18, 2));
        result.identity.request = read(bytes + 24, 8);
        result.identity.revision = read(bytes + 32, 8);
        std::copy(bytes + 40, bytes + 56, result.identity.document.begin());
        result.payloadBytes = read(bytes + 56, 8);
        validate(result, limits);
        return result;
    }
    std::vector<uint8_t> encodeFrame(const Frame& frame, const Limits& limits)
    {
        const auto header = encodeHeader(frame.header, limits);
        if (frame.payload.size() != frame.header.payloadBytes)
            reject(ProtocolErrorCode::invalidInput, "Worker payload length does not match its header.");
        std::vector<uint8_t> result;
        result.reserve(headerBytes + frame.payload.size());
        result.insert(result.end(), header.begin(), header.end());
        result.insert(result.end(), frame.payload.begin(), frame.payload.end());
        return result;
    }
    Frame decodeFrame(const uint8_t* bytes, size_t size, const Limits& limits)
    {
        const auto header = decodeHeader(bytes, std::min(size, headerBytes), limits);
        const auto count = static_cast<size_t>(header.payloadBytes);
        if (size - headerBytes < count)
            reject(ProtocolErrorCode::truncated, "Worker frame payload is incomplete.");
        if (size - headerBytes != count)
            reject(ProtocolErrorCode::trailingData, "Worker frame contains trailing bytes.");
        return { header, std::vector<uint8_t>(bytes + headerBytes, bytes + size) };
    }
    void validateResponseIdentity(const Header& response, const Header& request)
    {
        validateFields(request);
        validateFields(response);
        if (request.kind != FrameKind::request || response.kind == FrameKind::request)
            reject(ProtocolErrorCode::unexpectedResponse, "Worker response kind does not match a pending request.");
        if (response.operation != request.operation || !(response.identity == request.identity))
            reject(ProtocolErrorCode::staleResponse, "Worker response does not match the pending document revision and request.");
    }
}
}

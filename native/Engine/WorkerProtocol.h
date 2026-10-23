// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_PROTOCOL_H_
#define DUST3D_NATIVE_WORKER_PROTOCOL_H_
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace dust3d {
namespace worker {
    constexpr size_t headerBytes = 64;
    constexpr uint64_t maximumPayloadBytes = 256ULL * 1024 * 1024;
    constexpr uint64_t maximumDiagnosticBytes = 64ULL * 1024;

    enum class FrameKind : uint16_t { request = 1,
        response = 2,
        failure = 3 };
    enum class Operation : uint16_t { generateMesh = 1 };
    enum class ProtocolErrorCode { invalidInput,
        invalidLimits,
        truncated,
        invalidHeader,
        unsupportedVersion,
        unsupportedKind,
        unsupportedOperation,
        invalidIdentity,
        limitExceeded,
        trailingData,
        unexpectedResponse,
        staleResponse };

    /// Typed framing failure. Messages contain no document contents.
    class ProtocolError : public std::runtime_error {
    public:
        ProtocolError(ProtocolErrorCode code, const char* message);
        ProtocolErrorCode code() const noexcept { return m_code; }

    private:
        ProtocolErrorCode m_code;
    };

    /// Logical wire-payload limits, not allocator, decoded-output or RSS limits.
    struct Limits {
        uint64_t maximumInputBytes = 128ULL * 1024 * 1024;
        uint64_t maximumOutputBytes = maximumPayloadBytes;
        uint64_t maximumFailureBytes = 4096;
    };

    /// Parent-issued identity; document and request must be nonzero, revision may be zero.
    struct RequestIdentity {
        std::array<uint8_t, 16> document {};
        uint64_t request = 0;
        uint64_t revision = 0;
        bool operator==(const RequestIdentity& other) const noexcept;
    };

    /// In-memory fields only; never serialize this native struct directly.
    struct Header {
        FrameKind kind = FrameKind::request;
        Operation operation = Operation::generateMesh;
        RequestIdentity identity;
        uint64_t payloadBytes = 0;
    };
    struct Frame {
        Header header;
        std::vector<uint8_t> payload;
    };

    /// Encode a version-one, big-endian 64-byte header after limit/identity validation.
    /// Throws ProtocolError on invalid fields or limits. No body allocation occurs.
    std::array<uint8_t, headerBytes> encodeHeader(const Header& header, const Limits& limits);
    /// Decode exactly 64 readable bytes and validate advertised size before body allocation.
    /// NULL/short/extra input, unknown fields and invalid limits throw ProtocolError.
    Header decodeHeader(const uint8_t* bytes, size_t size, const Limits& limits);
    /// Encode owned payload after verifying its length matches the validated header.
    /// Throws ProtocolError or an ordinary allocation exception; no C boundary is crossed.
    std::vector<uint8_t> encodeFrame(const Frame& frame, const Limits& limits);
    /// Decode one complete frame into independently owned bytes. Partial/trailing input rejects.
    /// This validates framing only; callers must validate result semantics before publication.
    Frame decodeFrame(const uint8_t* bytes, size_t size, const Limits& limits);
    /// Require response/failure kind and exact parent-issued operation/document/request/revision.
    /// Throws ProtocolError for malformed/stale headers. Does not admit payload semantics.
    void validateResponseIdentity(const Header& response, const Header& request);
}
}
#endif

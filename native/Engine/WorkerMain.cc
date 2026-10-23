// SPDX-License-Identifier: MIT
#include "WorkerOperation.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <iostream>
#include <streambuf>
#include <unistd.h>

namespace {
using namespace dust3d::worker;
constexpr int usageExit = 64;
constexpr int protocolExit = 65;
constexpr int internalExit = 70;
constexpr int ioExit = 74;

class Descriptor {
public:
    explicit Descriptor(int value)
        : m_value(value)
    {
    }
    ~Descriptor()
    {
        if (m_value >= 0)
            ::close(m_value);
    }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const noexcept { return m_value; }

private:
    int m_value;
};
// Keep sanitizer/C runtime stderr observable while discarding C++ engine chatter.
class DiscardBuffer : public std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize count) override { return count; }
    int_type overflow(int_type value) override { return traits_type::not_eof(value); }
};
class QuietErrors {
public:
    QuietErrors()
        : m_previous(std::cerr.rdbuf(&m_sink))
    {
    }
    ~QuietErrors() { std::cerr.rdbuf(m_previous); }
    QuietErrors(const QuietErrors&) = delete;
    QuietErrors& operator=(const QuietErrors&) = delete;

private:
    DiscardBuffer m_sink;
    std::streambuf* m_previous;
};
struct IoResult {
    bool complete;
    int error;
};
IoResult readExactly(int descriptor, uint8_t* bytes, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        const auto count = ::read(descriptor, bytes + offset, std::min(size - offset, size_t { 65536 }));
        if (count == 0)
            return { false, 0 };
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return { false, errno };
        }
        offset += static_cast<size_t>(count);
    }
    return { true, 0 };
}
IoResult writeAll(int descriptor, const uint8_t* bytes, size_t size) noexcept
{
    size_t offset = 0;
    while (offset < size) {
        const auto count = ::write(descriptor, bytes + offset, std::min(size - offset, size_t { 65536 }));
        if (count <= 0) {
            if (count < 0 && errno == EINTR)
                continue;
            return { false, count < 0 ? errno : EIO };
        }
        offset += static_cast<size_t>(count);
    }
    return { true, 0 };
}
// Only transport/codec descriptions reach this bounded channel, never native cause bytes.
// A secondary diagnostic failure must not alter the primary exit/result decision.
void diagnostic(int descriptor, const char* stage, int code, const char* cause, uint64_t request) noexcept
{
    char message[1024];
    const int length = std::snprintf(message, sizeof(message), "stage=%s code=%d request=%llu operation=generateMesh cause=%s\n",
        stage, code, static_cast<unsigned long long>(request), cause);
    if (length > 0)
        (void)writeAll(descriptor, reinterpret_cast<const uint8_t*>(message), std::min(static_cast<size_t>(length), sizeof(message) - 1));
}

}
int main(int argc, char**)
{
    // Broken response pipes report ordinary I/O errors; no signal-delivery probe is used.
    if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return ioExit;
    Descriptor output(::fcntl(STDOUT_FILENO, F_DUPFD_CLOEXEC, 3));
    const int outputError = output.get() < 0 ? errno : 0;
    Descriptor errors(::fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 3));
    const int diagnosticError = errors.get() < 0 ? errno : 0;
    Descriptor sink(::open("/dev/null", O_WRONLY | O_CLOEXEC));
    const int sinkError = sink.get() < 0 ? errno : 0;
    if (outputError || diagnosticError || sinkError) {
        const int cause = outputError ? outputError : diagnosticError ? diagnosticError
                                                                      : sinkError;
        diagnostic(STDERR_FILENO, "setup", cause, std::strerror(cause), 0);
        return ioExit;
    }
    if (::dup2(sink.get(), STDOUT_FILENO) < 0) {
        const int cause = errno;
        diagnostic(errors.get(), "setup", cause, std::strerror(cause), 0);
        return ioExit;
    }
    if (argc != 1) {
        diagnostic(errors.get(), "usage", usageExit, "Worker accepts no command-line arguments.", 0);
        return usageExit;
    }
    uint64_t requestIdentity = 0;
    try {
        QuietErrors quiet;
        std::array<uint8_t, headerBytes> bytes {};
        auto input = readExactly(STDIN_FILENO, bytes.data(), bytes.size());
        if (!input.complete) {
            if (input.error) {
                diagnostic(errors.get(), "input", input.error, std::strerror(input.error), 0);
                return ioExit;
            }
            throw ProtocolError(ProtocolErrorCode::truncated, "Worker request header is incomplete.");
        }
        const auto header = decodeHeader(bytes.data(), bytes.size(), {});
        if (header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Worker accepts request frames only.");
        requestIdentity = header.identity.request;
        Frame request { header, std::vector<uint8_t>(static_cast<size_t>(header.payloadBytes)) };
        input = readExactly(STDIN_FILENO, request.payload.data(), request.payload.size());
        if (!input.complete) {
            if (input.error) {
                diagnostic(errors.get(), "input", input.error, std::strerror(input.error), requestIdentity);
                return ioExit;
            }
            throw ProtocolError(ProtocolErrorCode::truncated, "Worker request body is incomplete.");
        }
        uint8_t extra;
        input = readExactly(STDIN_FILENO, &extra, 1);
        if (input.complete)
            throw ProtocolError(ProtocolErrorCode::trailingData, "Worker accepts exactly one request followed by EOF.");
        if (input.error) {
            diagnostic(errors.get(), "input", input.error, std::strerror(input.error), requestIdentity);
            return ioExit;
        }
        auto response = generateMeshReply(request, {});
        std::vector<uint8_t>().swap(request.payload);
        const auto reply = encodeFrame(response, {});
        const auto written = writeAll(output.get(), reply.data(), reply.size());
        if (!written.complete) {
            diagnostic(errors.get(), "output", written.error, std::strerror(written.error), requestIdentity);
            return ioExit;
        }
        return 0;
    } catch (const ProtocolError& cause) {
        diagnostic(errors.get(), "protocol", static_cast<int>(cause.code()), cause.what(), requestIdentity);
        return protocolExit;
    } catch (const std::exception& cause) {
        // Operation diagnostics remain binary data; only transport/codec exceptions reach here.
        diagnostic(errors.get(), "internal", internalExit, cause.what(), requestIdentity);
        return internalExit;
    } catch (...) {
        diagnostic(errors.get(), "internal", internalExit, "Exception without an available diagnostic.", requestIdentity);
        return internalExit;
    }
}

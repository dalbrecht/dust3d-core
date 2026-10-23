// SPDX-License-Identifier: MIT
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "WorkerTransport.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
namespace {
using namespace dust3d::worker;
class Descriptor {
public:
    ~Descriptor() { close(); }
    Descriptor() = default;
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const noexcept { return m_value; }
    void own(int value) noexcept { m_value = value; }
    void close() noexcept
    {
        if (m_value >= 0) {
            ::close(m_value);
            m_value = -1;
        }
    }

private:
    int m_value = -1;
};
struct Channel {
    Descriptor parent;
    Descriptor child;
    Channel()
    {
        int descriptors[2];
        if (::socketpair(AF_UNIX, SOCK_STREAM, 0, descriptors) < 0)
            throw TransportError(TransportErrorCode::setup, errno, "socketpair");
        // Normalize above stdio so spawn dup/close actions cannot collide with closed parent stdio.
        Descriptor originals[2];
        originals[0].own(descriptors[0]);
        originals[1].own(descriptors[1]);
        const int first = ::fcntl(originals[0].get(), F_DUPFD_CLOEXEC, 3);
        if (first < 0)
            throw TransportError(TransportErrorCode::setup, errno, "duplicate parent channel");
        parent.own(first);
        const int second = ::fcntl(originals[1].get(), F_DUPFD_CLOEXEC, 3);
        if (second < 0)
            throw TransportError(TransportErrorCode::setup, errno, "duplicate child channel");
        child.own(second);
        const int flags = ::fcntl(parent.get(), F_GETFL);
        if (flags < 0 || ::fcntl(parent.get(), F_SETFL, flags | O_NONBLOCK) < 0)
            throw TransportError(TransportErrorCode::setup, errno, "nonblocking parent channel");
#if defined(SO_NOSIGPIPE)
        const int enabled = 1;
        if (::setsockopt(parent.get(), SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) < 0)
            throw TransportError(TransportErrorCode::setup, errno, "suppress socket SIGPIPE");
#endif
    }
};
class Child {
public:
    Child() = default;
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    ~Child()
    {
        if (m_pid > 0) {
            int status;
            while (::waitpid(m_pid, &status, 0) < 0 && errno == EINTR) { }
        }
    }
    void own(pid_t pid) noexcept { m_pid = pid; }
    bool collect(ProcessOutcome& outcome)
    {
        if (m_pid <= 0)
            return true;
        int status;
        const auto result = ::waitpid(m_pid, &status, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR))
            return false;
        if (result < 0)
            throw TransportError(TransportErrorCode::wait, errno, "waitpid");
        m_pid = -1;
        outcome = { WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status)
                                                                                                     : status };
        return true;
    }

private:
    pid_t m_pid = -1;
};
void checked(int code, const char* operation)
{
    if (code != 0)
        throw TransportError(TransportErrorCode::setup, code, operation);
}
class SpawnActions {
public:
    SpawnActions() { checked(posix_spawn_file_actions_init(&value), "spawn file actions"); }
    ~SpawnActions() { posix_spawn_file_actions_destroy(&value); }
    SpawnActions(const SpawnActions&) = delete;
    SpawnActions& operator=(const SpawnActions&) = delete;
    posix_spawn_file_actions_t value;
};
class SpawnAttributes {
public:
    SpawnAttributes() { checked(posix_spawnattr_init(&value), "spawn attributes"); }
    ~SpawnAttributes() { posix_spawnattr_destroy(&value); }
    SpawnAttributes(const SpawnAttributes&) = delete;
    SpawnAttributes& operator=(const SpawnAttributes&) = delete;
    posix_spawnattr_t value;
};
void spawn(Child& child, const std::string& executable, Channel& input, Channel& output, Channel& diagnostic)
{
    SpawnActions actions;
    SpawnAttributes attributes;
    checked(posix_spawn_file_actions_adddup2(&actions.value, input.child.get(), STDIN_FILENO), "spawn stdin");
    checked(posix_spawn_file_actions_adddup2(&actions.value, output.child.get(), STDOUT_FILENO), "spawn stdout");
    checked(posix_spawn_file_actions_adddup2(&actions.value, diagnostic.child.get(), STDERR_FILENO), "spawn stderr");
#if defined(__APPLE__)
    checked(posix_spawnattr_setflags(&attributes.value, POSIX_SPAWN_CLOEXEC_DEFAULT), "spawn close descriptors");
#elif defined(__linux__)
    checked(posix_spawn_file_actions_addclosefrom_np(&actions.value, 3), "spawn close descriptors");
#else
#error Worker transport supports macOS and Linux only
#endif
    char* arguments[] = { const_cast<char*>(executable.c_str()), nullptr };
    pid_t pid;
    const int code = posix_spawn(&pid, executable.c_str(), &actions.value, &attributes.value, arguments, environ);
    if (code != 0)
        throw TransportError(TransportErrorCode::spawn, code, "posix_spawn");
    child.own(pid);
    input.child.close();
    output.child.close();
    diagnostic.child.close();
}
ssize_t receive(Descriptor& channel, uint8_t* bytes, size_t size, TransportErrorCode code)
{
    const auto count = ::read(channel.get(), bytes, size);
    if (count == 0)
        channel.close();
    else if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
        throw TransportError(code, errno, "read worker channel");
    return count;
}
}
namespace dust3d {
namespace worker {
    TransportError::TransportError(TransportErrorCode code, int operatingSystemCode, const char* operation)
        : std::runtime_error(std::string(operation) + ": code=" + std::to_string(operatingSystemCode) + " cause=" + std::strerror(operatingSystemCode))
        , m_code(code)
        , m_operatingSystemCode(operatingSystemCode)
    {
    }
    CollectedJob runWorkerJob(const std::string& executable, const Frame& request, const TransportLimits& limits)
    {
        if (executable.empty() || executable[0] != '/' || executable.find('\0') != std::string::npos
            || limits.maximumIoChunkBytes == 0 || limits.maximumIoChunkBytes > 65536
            || limits.maximumCapturedDiagnosticBytes == 0 || limits.maximumCapturedDiagnosticBytes > maximumDiagnosticBytes)
            throw TransportError(TransportErrorCode::invalidConfiguration, EINVAL, "worker configuration");
        PendingReply preflight(request.header);
        const auto wire = encodeFrame(request, limits.wire);
        CollectedJob result;
        result.reply.reserve(headerBytes);
        result.diagnostic.reserve(limits.maximumCapturedDiagnosticBytes);
        // Declare the owner first: exceptions close every channel before blocking reap.
        Child child;
        Channel input;
        Channel output;
        Channel diagnostic;
        spawn(child, executable, input, output, diagnostic);
        size_t sent = 0;
        uint64_t expectedReplyBytes = headerBytes;
        bool hasHeader = false;
        bool hasExited = false;
        std::array<uint8_t, 65536> bytes;
        while (input.parent.get() >= 0 || output.parent.get() >= 0 || diagnostic.parent.get() >= 0 || !hasExited) {
            std::array<pollfd, 3> channels { pollfd { input.parent.get(), POLLOUT, 0 },
                pollfd { output.parent.get(), POLLIN, 0 }, pollfd { diagnostic.parent.get(), POLLIN, 0 } };
            if (::poll(channels.data(), channels.size(), 50) < 0) {
                if (errno == EINTR)
                    continue;
                throw TransportError(TransportErrorCode::output, errno, "poll worker channels");
            }
            if (channels[0].revents) {
#if defined(MSG_NOSIGNAL)
                constexpr int sendFlags = MSG_NOSIGNAL;
#else
                constexpr int sendFlags = 0;
#endif
                const auto count = ::send(input.parent.get(), wire.data() + sent,
                    std::min(wire.size() - sent, limits.maximumIoChunkBytes), sendFlags);
                if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
                    throw TransportError(TransportErrorCode::input, errno, "send worker input");
                if (count > 0)
                    sent += static_cast<size_t>(count);
                if (sent == wire.size())
                    input.parent.close();
            }
            if (channels[1].revents) {
                // Admit the fixed header before collecting body bytes.
                const size_t remaining = static_cast<size_t>(expectedReplyBytes) - result.reply.size();
                const size_t requested = hasHeader ? std::min(limits.maximumIoChunkBytes, remaining + 1) : std::min(limits.maximumIoChunkBytes, remaining);
                const auto count = receive(output.parent, bytes.data(), requested, TransportErrorCode::output);
                if (count > 0) {
                    if (static_cast<uint64_t>(count) > remaining)
                        throw ProtocolError(ProtocolErrorCode::trailingData, "Worker sent bytes beyond the advertised response");
                    result.reply.insert(result.reply.end(), bytes.begin(), bytes.begin() + count);
                    if (!hasHeader && result.reply.size() == headerBytes) {
                        const auto header = decodeHeader(result.reply.data(), result.reply.size(), limits.wire);
                        validateResponseIdentity(header, request.header);
                        expectedReplyBytes = headerBytes + header.payloadBytes;
                        hasHeader = true;
                    }
                }
            }
            if (channels[2].revents) {
                const auto count = receive(diagnostic.parent, bytes.data(), limits.maximumIoChunkBytes, TransportErrorCode::diagnostic);
                if (count > 0) {
                    const size_t captured = std::min(static_cast<size_t>(count), limits.maximumCapturedDiagnosticBytes - result.diagnostic.size());
                    result.diagnostic.insert(result.diagnostic.end(), bytes.begin(), bytes.begin() + captured);
                    result.diagnosticTruncated = result.diagnosticTruncated || captured < static_cast<size_t>(count);
                }
            }
            hasExited = child.collect(result.outcome);
        }
        return result;
    }
}
}

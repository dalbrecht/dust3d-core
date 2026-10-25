// SPDX-License-Identifier: MIT
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "WorkerTransport.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

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
class Child : private WorkerCleanupSystem {
public:
    explicit Child(bool shouldStopOnFailure, WorkerCleanupReport* cleanupReport)
        : m_shouldStopOnFailure(shouldStopOnFailure)
        , m_cleanupReport(cleanupReport)
    {
    }
    Child(const Child&) = delete;
    Child& operator=(const Child&) = delete;
    ~Child() override
    {
        if (m_pid > 0) {
            const auto report = cleanupWorkerChild(true, m_shouldStopOnFailure, *this);
            m_pid = -1;
            if (m_cleanupReport)
                *m_cleanupReport = report;
        }
    }
    void own(pid_t pid) noexcept { m_pid = pid; }
    void requestStop(bool force)
    {
        if (m_pid > 0 && ::kill(m_pid, force ? SIGKILL : SIGTERM) < 0 && errno != ESRCH)
            throw TransportError(TransportErrorCode::stop, errno, "stop owned worker");
    }
    bool collect(ProcessOutcome& outcome)
    {
        if (m_pid <= 0)
            return true;
        int status;
        const auto result = ::waitpid(m_pid, &status, WNOHANG);
        if (result == 0 || (result < 0 && errno == EINTR))
            return false;
        if (result < 0) {
            const int cause = errno;
            // Never stop a PID after the OS reports that it is no longer our child.
            if (cause == ECHILD)
                m_pid = -1;
            throw TransportError(TransportErrorCode::wait, cause, "waitpid");
        }
        m_pid = -1;
        outcome = { WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status)
                                                                                                     : status };
        return true;
    }

private:
    int requestForceStop() noexcept override
    {
        if (::kill(m_pid, SIGKILL) == 0)
            return 0;
        const int cause = errno;
        return cause == ESRCH ? 0 : cause;
    }
    CleanupWaitResult reap() noexcept override
    {
        int status = 0;
        if (::waitpid(m_pid, &status, 0) < 0) {
            const int cause = errno;
            return { cause == EINTR ? CleanupWaitState::interrupted : CleanupWaitState::failed, {}, cause };
        }
        return { CleanupWaitState::collected,
            { WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? WTERMSIG(status)
                                                                                               : status },
            0 };
    }
    pid_t m_pid = -1;
    bool m_shouldStopOnFailure;
    WorkerCleanupReport* m_cleanupReport;
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
void spawn(Child& child, const std::string& executable, Channel& input, Channel& output, Channel& diagnostic, const std::vector<std::string>& resourceArguments)
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
    std::vector<char*> arguments;
    arguments.reserve(resourceArguments.size() + 2);
    arguments.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& argument : resourceArguments)
        arguments.push_back(const_cast<char*>(argument.c_str()));
    arguments.push_back(nullptr);
    pid_t pid;
    const int code = posix_spawn(&pid, executable.c_str(), &actions.value, &attributes.value, arguments.data(), environ);
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
    WorkerStopped::WorkerStopped(LifetimeReason reason, std::optional<CollectedJob> collected)
        : std::runtime_error("Worker job retired: reason=" + std::to_string(static_cast<unsigned>(reason)))
        , m_reason(reason)
        , m_collected(std::move(collected))
    {
    }
    static CollectedJob runJob(const std::string& executable, const Frame& request, const TransportLimits& limits,
        const WorkerSupervision* supervision, const ResourceLaunch* resourceLaunch)
    {
        if (supervision && supervision->cleanupReport)
            *supervision->cleanupReport = {};
        if (supervision && supervision->attemptReport)
            *supervision->attemptReport = {};
        if (executable.empty() || executable[0] != '/' || executable.find('\0') != std::string::npos
            || limits.maximumIoChunkBytes == 0 || limits.maximumIoChunkBytes > 65536
            || limits.maximumCapturedDiagnosticBytes == 0 || limits.maximumCapturedDiagnosticBytes > maximumDiagnosticBytes)
            throw TransportError(TransportErrorCode::invalidConfiguration, EINVAL, "worker configuration");
        std::vector<std::string> resourceArguments;
        if (resourceLaunch) {
            if (limits.maximumCapturedDiagnosticBytes < maximumResourceObservationBytes)
                throw TransportError(TransportErrorCode::invalidConfiguration, EINVAL, "worker resource diagnostic capture");
            if (resourceLaunch->profile.addressBytes)
                throw ResourceError(ResourceErrorCode::unsupportedResource, ResourceKind::addressBytes, "Worker virtual address activation is unsupported");
            resourceArguments = encodeResourceLaunch(*resourceLaunch);
        }
        std::optional<WorkerLifetime> lifetime;
        if (supervision)
            lifetime.emplace(supervision->lifetime);
        PendingReply preflight(request.header, limits.wire);
        if (supervision && supervision->cancellation && supervision->cancellation->isCancellationRequested())
            throw WorkerStopped(LifetimeReason::cancelled, std::nullopt);
        const auto wire = encodeFrame(request, limits.wire);
        CollectedJob result;
        bool hasExited = false;
        WorkerAttemptCapture capture(result, hasExited, supervision ? supervision->attemptReport : nullptr);
        result.reply.reserve(headerBytes);
        result.diagnostic.reserve(limits.maximumCapturedDiagnosticBytes);
        // Declare the owner first: exceptions close every channel before blocking reap.
        Child child(supervision != nullptr, supervision ? supervision->cleanupReport : nullptr);
        Channel input;
        Channel output;
        Channel diagnostic;
        if (supervision && supervision->cancellation && supervision->cancellation->isCancellationRequested())
            throw WorkerStopped(LifetimeReason::cancelled, std::nullopt);
        spawn(child, executable, input, output, diagnostic, resourceArguments);
        const auto started = std::chrono::steady_clock::now();
        size_t sent = 0;
        uint64_t expectedReplyBytes = headerBytes;
        bool hasHeader = false;
        std::array<uint8_t, 65536> bytes;
        while (supervision || input.parent.get() >= 0 || output.parent.get() >= 0 || diagnostic.parent.get() >= 0 || !hasExited) {
            if (supervision) {
                hasExited = child.collect(result.outcome);
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
                if (elapsed < 0)
                    throw TransportError(TransportErrorCode::clock, EINVAL, "worker monotonic clock");
                const auto decision = lifetime->observe({ static_cast<uint64_t>(elapsed),
                    supervision->cancellation && supervision->cancellation->isCancellationRequested(), hasExited,
                    input.parent.get() < 0 && output.parent.get() < 0 && diagnostic.parent.get() < 0 });
                if (decision.action == LifetimeAction::requestStop || decision.action == LifetimeAction::requestForceStop) {
                    input.parent.close();
                    child.requestStop(decision.action == LifetimeAction::requestForceStop);
                }
                if (decision.action == LifetimeAction::closeChannels) {
                    input.parent.close();
                    output.parent.close();
                    diagnostic.parent.close();
                }
                if (decision.finished) {
                    if (!decision.mayAdmit) {
                        capture.dismiss();
                        throw WorkerStopped(decision.reason, std::move(result));
                    }
                    break;
                }
            }
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
    CollectedJob runWorkerJob(const std::string& executable, const Frame& request, const TransportLimits& limits)
    {
        return runJob(executable, request, limits, nullptr, nullptr);
    }
    CollectedJob runSupervisedWorkerJob(const std::string& executable, const Frame& request,
        const TransportLimits& limits, const WorkerSupervision& supervision)
    {
        return runJob(executable, request, limits, &supervision, nullptr);
    }
    ResourceJobError::ResourceJobError(CollectedJob collected, std::optional<ResourceObservationError> observationFailure)
        : std::runtime_error(observationFailure ? std::string(observationFailure->what())
                                                : "Worker resource completion: normal=" + std::to_string(collected.outcome.exitedNormally) + " code=" + std::to_string(collected.outcome.exitCode))
        , m_collected(std::move(collected))
        , m_observationFailure(std::move(observationFailure))
    {
    }
    ResourceCollectedJob admitResourceCompletion(CollectedJob collected, const ResourceLaunch& requested)
    {
        if (!collected.outcome.exitedNormally || collected.outcome.exitCode != 0)
            throw ResourceJobError(std::move(collected));
        try {
            const auto observation = decodeResourceObservation(collected.diagnostic.data(), collected.diagnostic.size(), requested);
            return { std::move(collected), observation };
        } catch (const ResourceObservationError& cause) {
            throw ResourceJobError(std::move(collected), cause);
        }
    }
    ResourceCollectedJob runResourceLimitedWorkerJob(const std::string& executable, const Frame& request,
        const ResourceLaunch& launch, const TransportLimits& limits, const WorkerSupervision& supervision)
    {
        return admitResourceCompletion(runJob(executable, request, limits, &supervision, &launch), launch);
    }

}
}

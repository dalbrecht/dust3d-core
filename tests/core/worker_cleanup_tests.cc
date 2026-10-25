// SPDX-License-Identifier: MIT
#include <array>
#include <cerrno>
#include <iostream>
#include <native/Engine/WorkerCleanup.h>
#include <stdexcept>
#include <type_traits>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
class RecordingCleanup final : public WorkerCleanupSystem {
public:
    int stopError = 0;
    unsigned stopCalls = 0;
    unsigned reapCalls = 0;
    std::array<CleanupWaitResult, 3> replies {};
    int requestForceStop() noexcept override
    {
        ++stopCalls;
        return stopError;
    }
    CleanupWaitResult reap() noexcept override
    {
        const auto index = reapCalls++;
        if (index < replies.size())
            return replies[index];
        return { CleanupWaitState::failed, {}, EINVAL };
    }
};
void ordinaryCleanup()
{
    RecordingCleanup system;
    auto absent = cleanupWorkerChild(false, true, system);
    require(!absent.attemptedStop && !absent.attemptedReap && !absent.outcome && !absent.stopError && !absent.waitFailure, "No owned child means no process operation");
    require(system.stopCalls == 0 && system.reapCalls == 0, "No process calls for an absent owner");
    system.replies[0] = { CleanupWaitState::collected, { true, 7 }, 0 };
    const auto legacy = cleanupWorkerChild(true, false, system);
    require(!legacy.attemptedStop && legacy.attemptedReap && legacy.outcome && legacy.outcome->exitedNormally && legacy.outcome->exitCode == 7, "Legacy cleanup only collects the exact outcome");
    require(system.stopCalls == 0 && system.reapCalls == 1 && !legacy.stopError && !legacy.waitFailure, "Successful reap has no invented failure");
    RecordingCleanup supervised;
    supervised.replies = { CleanupWaitResult { CleanupWaitState::interrupted, {}, EINTR },
        CleanupWaitResult { CleanupWaitState::interrupted, {}, EINTR },
        CleanupWaitResult { CleanupWaitState::collected, { false, 9 }, 0 } };
    const auto result = cleanupWorkerChild(true, true, supervised);
    require(result.attemptedStop && result.attemptedReap && !result.stopError && !result.waitFailure, "Supervised cleanup retains successful stop/reap state");
    require(supervised.stopCalls == 1 && supervised.reapCalls == 3 && result.outcome && !result.outcome->exitedNormally && result.outcome->exitCode == 9, "Interrupted observations retry reap only and preserve outcome");
}
void independentFailures()
{
    RecordingCleanup system;
    system.stopError = EPERM;
    system.replies[0] = { CleanupWaitState::failed, {}, ECHILD };
    const auto result = cleanupWorkerChild(true, true, system);
    require(result.stopError == EPERM && result.waitFailure && result.waitFailure->code == CleanupFailureCode::operatingSystem && result.waitFailure->operatingSystemCode == ECHILD, "Both original error codes survive independently");
    require(!result.outcome && system.stopCalls == 1 && system.reapCalls == 1, "Stop failure never suppresses reap or invents an outcome");
    RecordingCleanup reaped;
    reaped.stopError = EACCES;
    reaped.replies[0] = { CleanupWaitState::collected, { true, 0 }, 0 };
    const auto collected = cleanupWorkerChild(true, true, reaped);
    require(collected.stopError == EACCES && !collected.waitFailure && collected.outcome, "A later successful reap does not erase the stop error");
    RecordingCleanup invalid;
    invalid.replies[0] = { static_cast<CleanupWaitState>(99), {}, 123 };
    const auto rejected = cleanupWorkerChild(true, false, invalid);
    require(rejected.waitFailure && rejected.waitFailure->code == CleanupFailureCode::invalidWaitResult && !rejected.outcome, "An invalid trusted-boundary observation is never a collected child");
}
void originalExceptionSurvives()
{
    static_assert(noexcept(cleanupWorkerChild(true, true, std::declval<WorkerCleanupSystem&>())));
    static_assert(std::is_nothrow_copy_assignable<WorkerCleanupReport>::value);
    RecordingCleanup system;
    system.stopError = EACCES;
    system.replies[0] = { CleanupWaitState::failed, {}, EIO };
    WorkerCleanupReport report;
    try {
        struct Owner {
            RecordingCleanup& system;
            WorkerCleanupReport& report;
            ~Owner() { report = cleanupWorkerChild(true, true, system); }
        } owner { system, report };
        throw std::runtime_error("original transport cause");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()) == "original transport cause", "Cleanup must not replace the active exception");
    }
    require(report.stopError == EACCES && report.waitFailure && report.waitFailure->operatingSystemCode == EIO, "Cleanup context remains available after original exception propagation");
}
}
int main()
{
    try {
        ordinaryCleanup();
        independentFailures();
        originalExceptionSurvives();
        std::cout << "Worker cleanup recording-only contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

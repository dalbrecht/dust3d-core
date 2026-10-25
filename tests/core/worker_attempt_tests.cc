// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerAttempt.h>
#include <native/Engine/WorkerTransport.h>
#include <stdexcept>
#include <type_traits>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
CollectedJob buffered()
{
    return { { false, 7 }, { 0, 255, 27, 13 }, { 255, 0, 27 }, true };
}
void retainsExceptionalBuffers(bool observedExit)
{
    WorkerAttemptReport report;
    auto result = buffered();
    try {
        WorkerAttemptCapture capture(result, observedExit, &report);
        throw std::runtime_error("original channel error");
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()) == "original channel error", "The primary exception remains unchanged");
    }
    result.reply = { 1 };
    result.diagnostic = { 2 };
    require(report.captured && report.outcome.has_value() == observedExit, "A default outcome must never imply observed child termination");
    require(report.reply == std::vector<uint8_t>({ 0, 255, 27, 13 }) && report.diagnostic == std::vector<uint8_t>({ 255, 0, 27 }) && report.diagnosticTruncated, "Independent original opaque buffers and truncation survive unwinding");
    if (observedExit)
        require(!report.outcome->exitedNormally && report.outcome->exitCode == 7, "Only actually observed outcome bits survive");
}
void normalAndDismissedScopes()
{
    WorkerAttemptReport report;
    report.captured = true;
    report.reply = { 88 };
    auto result = buffered();
    bool exited = false;
    {
        WorkerAttemptCapture capture(result, exited, &report);
    }
    require(!report.captured && report.reply.empty() && result.reply == buffered().reply, "Normal scope resets stale output without reporting failure or consuming collector buffers");
    try {
        WorkerAttemptCapture capture(result, exited, &report);
        capture.dismiss();
        throw std::runtime_error("owned retirement already retains context");
    } catch (const std::runtime_error&) {
    }
    require(!report.captured && result.diagnostic == buffered().diagnostic, "Dismissed capture leaves an independently owned retirement context unchanged");
    try {
        WorkerAttemptCapture capture(result, exited, nullptr);
        throw std::runtime_error("no observer");
    } catch (const std::runtime_error&) {
    }
    require(result.reply == buffered().reply && result.diagnosticTruncated, "Omitting capture storage preserves legacy buffer ownership");
}
}
int main()
{
    static_assert(std::is_nothrow_move_assignable<WorkerAttemptReport>::value);
    static_assert(std::is_nothrow_destructible<WorkerAttemptCapture>::value);
    static_assert(!std::is_constructible<WorkerAttemptCapture, CollectedJob&, bool&&, WorkerAttemptReport*>::value);
    try {
        retainsExceptionalBuffers(false);
        retainsExceptionalBuffers(true);
        normalAndDismissedScopes();
        std::cout << "Worker attempt value-only contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

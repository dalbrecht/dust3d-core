// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerLifetime.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(LifetimeErrorCode code, Action action)
{
    try {
        action();
    } catch (const LifetimeError& error) {
        require(error.code() == code && error.what()[0], "Original typed lifecycle error required");
        return;
    }
    throw std::runtime_error("Invalid lifecycle state admitted");
}
void normalAndDrain()
{
    WorkerLifetime normal({ 100, 10, 20 });
    require(normal.observe({ 0, false, false, false }).action == LifetimeAction::none, "Ordinary running work does not stop");
    require(!normal.observe({ 99, false, true, false }).mayAdmit, "Exit alone cannot publish before channel collection");
    auto done = normal.observe({ 118, false, true, true });
    require(done.finished && done.mayAdmit && done.reason == LifetimeReason::none, "Collected timely exit permits later semantic admission");
    require(!normal.observe({ 119, false, true, true }).mayAdmit, "Terminal observation cannot repeat publication permission");
    WorkerLifetime drain({ 100, 10, 20 });
    drain.observe({ 5, false, true, false });
    require(!drain.observe({ 24, false, true, false }).finished, "Drain remains open before its deadline");
    done = drain.observe({ 25, false, true, false });
    require(done.finished && !done.mayAdmit && done.action == LifetimeAction::closeChannels && done.reason == LifetimeReason::drainTimeout, "Exact drain deadline discards output and closes channels");
    require(drain.observe({ 26, false, true, false }).action == LifetimeAction::none, "Drain closure action is single-use");
}
void stopping()
{
    WorkerLifetime deadline({ 100, 10, 20 });
    auto value = deadline.observe({ 100, false, false, false });
    require(value.action == LifetimeAction::requestStop && value.reason == LifetimeReason::deadline, "Exact work deadline retires publication and requests stop");
    require(deadline.observe({ 109, false, false, false }).action == LifetimeAction::none, "Stop request is not repeated");
    require(deadline.observe({ 110, false, false, false }).action == LifetimeAction::requestForceStop, "Exact grace deadline requests one escalation");
    require(deadline.observe({ 111, false, false, false }).action == LifetimeAction::none, "Escalation is not repeated");
    value = deadline.observe({ 112, false, true, true });
    require(value.finished && !value.mayAdmit && value.reason == LifetimeReason::deadline, "Stopped output remains inadmissible after exit");
    WorkerLifetime cancelled({ 100, 10, 20 });
    value = cancelled.observe({ 100, true, false, false });
    require(value.reason == LifetimeReason::cancelled, "Observed cancellation has precedence at the work boundary");
    value = cancelled.observe({ 101, false, true, false });
    require(value.action == LifetimeAction::none, "Observed exit prevents unnecessary stop escalation");
    value = cancelled.observe({ 121, false, true, false });
    require(value.reason == LifetimeReason::cancelled && value.action == LifetimeAction::closeChannels, "Drain expiry retains original cancellation reason");
    WorkerLifetime late({ 100, 10, 20 });
    require(!late.observe({ 100, false, true, true }).mayAdmit, "Exit first observed at the work deadline is not timely");
    WorkerLifetime afterExit({ 100, 10, 20 });
    afterExit.observe({ 1, false, true, false });
    require(!afterExit.observe({ 2, true, true, true }).mayAdmit, "Cancellation during drain prevents publication");
}
void validation()
{
    for (auto policy : { LifetimePolicy { 0, 1, 1 }, LifetimePolicy { 600001, 1, 1 }, LifetimePolicy { 1, 0, 1 }, LifetimePolicy { 1, 10001, 1 }, LifetimePolicy { 1, 1, 0 }, LifetimePolicy { 1, 1, 10001 } })
        rejects(LifetimeErrorCode::invalidPolicy, [&] { WorkerLifetime bad(policy); });
    WorkerLifetime state({ 100, 10, 20 });
    state.observe({ 5, false, false, true });
    rejects(LifetimeErrorCode::timeReversed, [&] { state.observe({ 4, false, false, true }); });
    rejects(LifetimeErrorCode::stateReversed, [&] { state.observe({ 6, false, false, false }); });
    require(state.observe({ 5, false, true, true }).mayAdmit, "Rejected observations do not mutate state");
    rejects(LifetimeErrorCode::stateReversed, [&] { state.observe({ 6, false, false, true }); });
    WorkerLifetime maximum({ 600000, 10000, 10000 });
    const auto end = std::numeric_limits<uint64_t>::max();
    maximum.observe({ end - 10, true, false, false });
    require(maximum.observe({ end, false, false, false }).action == LifetimeAction::none, "Elapsed subtraction does not overflow near integer maximum");
}
}
int main()
{
    try {
        normalAndDrain();
        stopping();
        validation();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

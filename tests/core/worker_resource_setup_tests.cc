// SPDX-License-Identifier: MIT
#include <cerrno>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerResourceSetup.h>
#include <native/Engine/WorkerUnixResources.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
class RecordingCalls final : public UnixResourceCalls {
public:
    std::vector<std::pair<char, int>> events;
    std::vector<int> reads;
    std::vector<int> writes;
    std::vector<rlimit> values;
    int readFailure = -1;
    int writeFailure = -1;
    rlimit inherited { RLIM_INFINITY, RLIM_INFINITY };
    int read(int resource, rlimit& result) override
    {
        events.push_back({ 'r', resource });
        reads.push_back(resource);
        if (resource == readFailure)
            return EACCES;
        result = inherited;
        return 0;
    }
    int write(int resource, const rlimit& value) override
    {
        events.push_back({ 'w', resource });
        writes.push_back(resource);
        values.push_back(value);
        if (resource == writeFailure)
            return EPERM;
        return 0;
    }
};
ResourceProfile profile() { return { { 10, 20 }, { 1024, 2048 }, { 16, 32 }, std::nullopt }; }
template <typename Action>
void setupFailure(ResourceSetupOperation operation, ResourceKind resource, int systemError, Action action)
{
    try {
        action();
    } catch (const ResourceSetupError& error) {
        require(error.operation() == operation && error.resource() == resource && error.systemError() == systemError && error.what()[0], "Original resource setup cause required");
        return;
    }
    throw std::runtime_error("Setup failure was swallowed");
}
template <typename Action>
void policyFailure(ResourceErrorCode code, ResourceKind resource, Action action)
{
    try {
        action();
    } catch (const ResourceError& error) {
        require(error.code() == code && error.resource() == resource, "Original policy rejection required");
        return;
    }
    throw std::runtime_error("Invalid policy admitted");
}
void ordinary()
{
    RecordingCalls calls;
    UnixResourceSystem system(calls);
    const auto plan = prepareWorkerResources(profile(), ResourceRuntime::ordinary, system);
    const std::vector<int> order { RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_CORE, RLIMIT_NOFILE };
    require(calls.reads == order && calls.writes == order && plan.size() == 4, "Inspect all mandatory resources then apply stable order");
    for (size_t index = 0; index < order.size(); ++index) {
        require(calls.events[index] == std::make_pair('r', order[index]) && calls.events[index + order.size()] == std::make_pair('w', order[index]), "Every inspection precedes the first mutation");
    }
    require(calls.values[0].rlim_cur == 10 && calls.values[0].rlim_max == 20, "CPU seconds preserved");
    require(calls.values[1].rlim_cur == 1024 && calls.values[1].rlim_max == 2048, "File bytes preserved");
    require(calls.values[2].rlim_cur == 0 && calls.values[2].rlim_max == 0, "Core dump cap always zero");
    require(calls.values[3].rlim_cur == 16 && calls.values[3].rlim_max == 32, "Descriptor-number ceiling preserved");
    calls = RecordingCalls {};
    calls.inherited = { 4, 8 };
    auto input = profile();
    input.addressBytes = ResourceBudget { 100, 200 };
    const auto bounded = prepareWorkerResources(input, ResourceRuntime::ordinary, system);
    require(calls.reads.back() == RLIMIT_AS && calls.writes.back() == RLIMIT_AS && bounded.size() == 5, "Explicit virtual cap mapped independently");
    for (size_t index = 0; index < calls.values.size(); ++index) {
        const auto& value = calls.values[index];
        require(value.rlim_cur <= 4 && value.rlim_max <= 8, "Adapter never raises observed inherited limits");
        require(value.rlim_cur == bounded[index].effective.soft && value.rlim_max == bounded[index].effective.hard, "Applied finite values match returned plan");
    }
}
void failureOrdering()
{
    const int resources[] = { RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_CORE, RLIMIT_NOFILE, RLIMIT_AS };
    for (size_t index = 0; index < 5; ++index) {
        auto input = profile();
        input.addressBytes = ResourceBudget { 100, 200 };
        RecordingCalls calls;
        UnixResourceSystem system(calls);
        calls.readFailure = resources[index];
        setupFailure(ResourceSetupOperation::inspect, static_cast<ResourceKind>(index), EACCES, [&] { prepareWorkerResources(input, ResourceRuntime::ordinary, system); });
        require(calls.reads.size() == index + 1 && calls.writes.empty(), "Inspection failure makes no mutations or later calls");
        calls = RecordingCalls {};
        calls.writeFailure = resources[index];
        setupFailure(ResourceSetupOperation::apply, static_cast<ResourceKind>(index), EPERM, [&] { prepareWorkerResources(input, ResourceRuntime::ordinary, system); });
        require(calls.reads.size() == 5 && calls.writes.size() == index + 1, "Apply failure stops immediately without retry or rollback");
    }
    RecordingCalls calls;
    UnixResourceSystem system(calls);
    auto input = profile();
    input.descriptors = { 0, 1 };
    policyFailure(ResourceErrorCode::invalidPolicy, ResourceKind::descriptors, [&] { prepareWorkerResources(input, ResourceRuntime::ordinary, system); });
    require(calls.writes.empty(), "Late validation failure precedes every mutation");
    input = profile();
    policyFailure(ResourceErrorCode::invalidPolicy, ResourceKind::descriptors, [&] { prepareWorkerResources(input, ResourceRuntime::ordinary, system, 17); });
    require(calls.writes.empty(), "Startup descriptor floor rejects before all mutations");
    input.addressBytes = ResourceBudget { 100, 200 };
    policyFailure(ResourceErrorCode::incompatibleRuntime, ResourceKind::addressBytes, [&] { prepareWorkerResources(input, ResourceRuntime::addressSanitized, system); });
    require(calls.writes.empty(), "Instrumented virtual policy cannot partially apply");
}
void conversionBoundaries()
{
    RecordingCalls calls;
    UnixResourceSystem system(calls);
    auto capability = system.inspect(ResourceKind::cpuSeconds);
    require(capability.isSupported && !capability.inherited.soft && !capability.inherited.hard && capability.maximumFinite < static_cast<uint64_t>(RLIM_INFINITY), "Infinity never converted to a finite inherited cap");
    calls.inherited = { static_cast<rlim_t>(capability.maximumFinite), RLIM_INFINITY };
    capability = system.inspect(ResourceKind::cpuSeconds);
    require(capability.inherited.soft == capability.maximumFinite && !capability.inherited.hard, "Largest convertible finite observation retained");
    system.apply({ ResourceKind::cpuSeconds, { capability.maximumFinite, capability.maximumFinite } });
    require(calls.values.back().rlim_cur == capability.maximumFinite, "Largest finite applied without sentinel collision");
    calls.inherited = { RLIM_INFINITY, 5 };
    policyFailure(ResourceErrorCode::invalidInherited, ResourceKind::cpuSeconds, [&] { prepareWorkerResources(profile(), ResourceRuntime::ordinary, system); });
    const auto finiteMaximum = std::numeric_limits<rlim_t>::max();
    if (finiteMaximum > RLIM_INFINITY) {
        calls.inherited = { finiteMaximum, RLIM_INFINITY };
        policyFailure(ResourceErrorCode::invalidInherited, ResourceKind::cpuSeconds, [&] { system.inspect(ResourceKind::cpuSeconds); });
        calls.inherited = { 1, finiteMaximum };
        policyFailure(ResourceErrorCode::invalidInherited, ResourceKind::cpuSeconds, [&] { system.inspect(ResourceKind::cpuSeconds); });
    }
    const auto written = calls.writes.size();
    policyFailure(ResourceErrorCode::unrepresentable, ResourceKind::cpuSeconds, [&] { system.apply({ ResourceKind::cpuSeconds, { 1, static_cast<uint64_t>(RLIM_INFINITY) } }); });
    policyFailure(ResourceErrorCode::invalidPolicy, ResourceKind::cpuSeconds, [&] { system.apply({ ResourceKind::cpuSeconds, { 2, 1 } }); });
    const auto unknown = static_cast<ResourceKind>(99);
    policyFailure(ResourceErrorCode::unsupportedResource, unknown, [&] { system.inspect(unknown); });
    policyFailure(ResourceErrorCode::unsupportedResource, unknown, [&] { system.apply({ unknown, { 1, 1 } }); });
    require(calls.writes.size() == written, "Invalid standalone adapter arguments never reach OS calls");
}
}
int main()
{
    try {
        ordinary();
        failureOrdering();
        conversionBoundaries();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

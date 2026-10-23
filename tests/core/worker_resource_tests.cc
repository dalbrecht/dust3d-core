// SPDX-License-Identifier: MIT
#include <iostream>
#include <limits>
#include <native/Engine/WorkerResources.h>
#include <stdexcept>

using namespace dust3d::worker;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(ResourceErrorCode code, ResourceKind resource, Action action)
{
    try {
        action();
    } catch (const ResourceError& error) {
        require(error.code() == code && error.resource() == resource && error.what()[0], "Original typed resource error required");
        return;
    }
    throw std::runtime_error("Invalid resource plan admitted");
}
ResourceProfile profile()
{
    return { { 10, 20 }, { 1024, 2048 }, { 16, 32 }, std::nullopt };
}
ResourceEnvironment environment()
{
    ResourceEnvironment value {};
    value.runtime = ResourceRuntime::ordinary;
    for (auto& resource : value.resources)
        resource = { true, 65535, { std::nullopt, std::nullopt } };
    return value;
}
void ordinaryAndInherited()
{
    const auto input = profile();
    const auto platform = environment();
    auto plan = planWorkerResources(input, platform);
    require(plan.size() == 4, "Mandatory CPU/file/core/descriptor caps planned without a memory default");
    const ResourceKind order[] = { ResourceKind::cpuSeconds, ResourceKind::fileBytes, ResourceKind::coreBytes, ResourceKind::descriptors };
    const uint64_t soft[] = { 10, 1024, 0, 16 };
    const uint64_t hard[] = { 20, 2048, 0, 32 };
    for (size_t index = 0; index < plan.size(); ++index) {
        require(plan[index].resource == order[index], "Stable helper setup order required");
        require(plan[index].effective.soft == soft[index] && plan[index].effective.hard == hard[index], "Finite requested caps retained against unlimited inheritance");
    }
    auto inherited = platform;
    inherited.resources[0].inherited = { 5, 15 };
    inherited.resources[1].inherited = { 1500, 1800 };
    inherited.resources[3].inherited = { 8, 12 };
    plan = planWorkerResources(input, inherited);
    require(plan[0].effective.soft == 5 && plan[0].effective.hard == 15, "Stricter inherited CPU limits retained");
    require(plan[1].effective.soft == 1024 && plan[1].effective.hard == 1800, "Soft and hard selected independently");
    require(plan[3].effective.soft == 8 && plan[3].effective.hard == 12, "No inherited descriptor limit raised");
    inherited.resources[0].inherited = { 0, 0 };
    inherited.resources[1].inherited = { 9, std::nullopt };
    plan = planWorkerResources(input, inherited);
    require(plan[0].effective.soft == 0 && plan[0].effective.hard == 0, "Inherited zero remains effective; helper preflight must check viability");
    require(plan[1].effective.soft == 9 && plan[1].effective.hard == 2048, "Finite soft with unlimited hard supported");
    require(input.cpuSeconds.soft == 10 && !platform.resources[0].inherited.soft, "Planning leaves requested and inherited state unchanged");
}
void validation()
{
    for (size_t index : { size_t { 0 }, size_t { 1 }, size_t { 3 } }) {
        const auto kind = static_cast<ResourceKind>(index);
        for (auto budget : { ResourceBudget { 0, 1 }, ResourceBudget { 1, 0 }, ResourceBudget { 2, 1 } }) {
            auto input = profile();
            if (index == 0)
                input.cpuSeconds = budget;
            else if (index == 1)
                input.fileBytes = budget;
            else
                input.descriptors = budget;
            rejects(ResourceErrorCode::invalidPolicy, kind, [&] { planWorkerResources(input, environment()); });
        }
    }
    for (size_t index = 0; index < 5; ++index) {
        const auto kind = static_cast<ResourceKind>(index);
        auto input = profile();
        input.addressBytes = ResourceBudget { 100, 200 };
        auto platform = environment();
        platform.resources[index].isSupported = false;
        rejects(ResourceErrorCode::unsupportedResource, kind, [&] { planWorkerResources(input, platform); });
        platform = environment();
        platform.resources[index].maximumFinite = 0;
        if (kind != ResourceKind::coreBytes)
            rejects(ResourceErrorCode::unrepresentable, kind, [&] { planWorkerResources(input, platform); });
        else
            require(planWorkerResources(input, platform)[index].effective.hard == 0, "Zero core cap is representable even with zero finite maximum");
        for (auto inherited : { InheritedResourceBudget { std::nullopt, 10 }, InheritedResourceBudget { 11, 10 }, InheritedResourceBudget { 65536, std::nullopt }, InheritedResourceBudget { 1, 65536 } }) {
            platform = environment();
            platform.resources[index].inherited = inherited;
            rejects(ResourceErrorCode::invalidInherited, kind, [&] { planWorkerResources(input, platform); });
        }
    }
    auto input = profile();
    auto platform = environment();
    platform.resources[0].maximumFinite = 15;
    rejects(ResourceErrorCode::unrepresentable, ResourceKind::cpuSeconds, [&] { planWorkerResources(input, platform); });
    platform = environment();
    platform.resources[0].maximumFinite = 9;
    rejects(ResourceErrorCode::unrepresentable, ResourceKind::cpuSeconds, [&] { planWorkerResources(input, platform); });
    // A rejected late resource must not leave partially modified platform state.
    platform = environment();
    platform.resources[3].isSupported = false;
    rejects(ResourceErrorCode::unsupportedResource, ResourceKind::descriptors, [&] { planWorkerResources(input, platform); });
    require(!platform.resources[0].inherited.soft, "Validation has no OS or input mutations");
    platform.resources[3].isSupported = true;
    require(planWorkerResources(input, platform).size() == 4, "Rejected planning does not poison subsequent valid planning");
}
void memoryAndRuntime()
{
    auto input = profile();
    auto platform = environment();
    ResourceEnvironment unspecified {};
    unspecified.resources = platform.resources;
    rejects(ResourceErrorCode::incompatibleRuntime, ResourceKind::addressBytes, [&] { planWorkerResources(input, unspecified); });
    platform.runtime = ResourceRuntime::addressSanitized;
    require(planWorkerResources(input, platform).size() == 4, "Instrumented profile may omit a virtual cap without claiming memory containment");
    input.addressBytes = ResourceBudget { 100, 200 };
    rejects(ResourceErrorCode::incompatibleRuntime, ResourceKind::addressBytes, [&] { planWorkerResources(input, platform); });
    platform.runtime = ResourceRuntime::ordinary;
    platform.resources[4].inherited = { 90, 180 };
    auto plan = planWorkerResources(input, platform);
    require(plan.size() == 5 && plan.back().resource == ResourceKind::addressBytes && plan.back().effective.soft == 90 && plan.back().effective.hard == 180, "Explicit supported virtual cap retained separately from resident memory");
    input.addressBytes = ResourceBudget { 0, 1 };
    rejects(ResourceErrorCode::invalidPolicy, ResourceKind::addressBytes, [&] { planWorkerResources(input, platform); });
    platform = environment();
    input.addressBytes = ResourceBudget { 1, std::numeric_limits<uint64_t>::max() };
    platform.resources[4].maximumFinite = std::numeric_limits<uint64_t>::max();
    require(planWorkerResources(input, platform).back().effective.hard == std::numeric_limits<uint64_t>::max(), "Largest explicit finite value never silently becomes infinity");
    platform.runtime = static_cast<ResourceRuntime>(99);
    rejects(ResourceErrorCode::incompatibleRuntime, ResourceKind::addressBytes, [&] { planWorkerResources(input, platform); });
    platform.runtime = ResourceRuntime::ordinary;
    input.addressBytes.reset();
    platform.resources[4].isSupported = false;
    require(planWorkerResources(input, platform).size() == 4, "Unrequested unsupported virtual cap does not affect other resources");
}
}
int main()
{
    try {
        ordinaryAndInherited();
        validation();
        memoryAndRuntime();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include "WorkerResourceSetup.h"
#include <system_error>

namespace dust3d {
namespace worker {
    ResourceSetupError::ResourceSetupError(ResourceSetupOperation operation, ResourceKind resource, int systemError)
        : std::runtime_error(std::string(operation == ResourceSetupOperation::inspect ? "Worker resource inspection: " : "Worker resource application: ") + std::system_category().message(systemError))
        , m_operation(operation)
        , m_resource(resource)
        , m_systemError(systemError)
    {
    }
    std::vector<PlannedResourceLimit> prepareWorkerResources(const ResourceProfile& profile, ResourceRuntime runtime, ResourceSystem& system, uint64_t minimumDescriptorCeiling)
    {
        ResourceEnvironment environment {};
        environment.runtime = runtime;
        for (auto resource : { ResourceKind::cpuSeconds, ResourceKind::fileBytes, ResourceKind::coreBytes, ResourceKind::descriptors })
            environment.resources[static_cast<size_t>(resource)] = system.inspect(resource);
        if (profile.addressBytes)
            environment.resources[static_cast<size_t>(ResourceKind::addressBytes)] = system.inspect(ResourceKind::addressBytes);
        auto plan = planWorkerResources(profile, environment);
        if (plan[3].effective.soft < minimumDescriptorCeiling)
            throw ResourceError(ResourceErrorCode::invalidPolicy, ResourceKind::descriptors, "Worker descriptor cap is below its startup requirement");
        for (const auto& entry : plan)
            system.apply(entry);
        return plan;
    }
}
}

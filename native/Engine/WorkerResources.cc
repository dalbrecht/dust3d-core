// SPDX-License-Identifier: MIT
#include "WorkerResources.h"
#include <algorithm>

namespace dust3d {
namespace worker {
    namespace {
        PlannedResourceLimit plan(ResourceKind resource, ResourceBudget requested, const ResourceEnvironment& environment)
        {
            if (requested.soft > requested.hard || (resource != ResourceKind::coreBytes && !requested.soft))
                throw ResourceError(ResourceErrorCode::invalidPolicy, resource, "Worker resource budget must be positive and ordered");
            const auto& capability = environment.resources[static_cast<size_t>(resource)];
            if (!capability.isSupported)
                throw ResourceError(ResourceErrorCode::unsupportedResource, resource, "Worker resource is unsupported by this platform profile");
            if (requested.hard > capability.maximumFinite)
                throw ResourceError(ResourceErrorCode::unrepresentable, resource, "Worker resource budget is not a finite platform value");
            const auto& inherited = capability.inherited;
            if ((inherited.soft && *inherited.soft > capability.maximumFinite)
                || (inherited.hard && *inherited.hard > capability.maximumFinite)
                || (inherited.hard && (!inherited.soft || *inherited.soft > *inherited.hard)))
                throw ResourceError(ResourceErrorCode::invalidInherited, resource, "Inherited worker resource limits are invalid");
            return { resource, { inherited.soft ? std::min(requested.soft, *inherited.soft) : requested.soft, inherited.hard ? std::min(requested.hard, *inherited.hard) : requested.hard } };
        }
    }
    ResourceError::ResourceError(ResourceErrorCode code, ResourceKind resource, const char* message)
        : std::runtime_error(message)
        , m_code(code)
        , m_resource(resource)
    {
    }
    std::vector<PlannedResourceLimit> planWorkerResources(const ResourceProfile& profile, const ResourceEnvironment& environment)
    {
        if ((environment.runtime != ResourceRuntime::ordinary && environment.runtime != ResourceRuntime::addressSanitized)
            || (profile.addressBytes && environment.runtime == ResourceRuntime::addressSanitized))
            throw ResourceError(ResourceErrorCode::incompatibleRuntime, ResourceKind::addressBytes, "Worker virtual address policy is incompatible with its runtime profile");
        std::vector<PlannedResourceLimit> result {
            plan(ResourceKind::cpuSeconds, profile.cpuSeconds, environment),
            plan(ResourceKind::fileBytes, profile.fileBytes, environment),
            plan(ResourceKind::coreBytes, { 0, 0 }, environment),
            plan(ResourceKind::descriptors, profile.descriptors, environment)
        };
        if (profile.addressBytes)
            result.push_back(plan(ResourceKind::addressBytes, *profile.addressBytes, environment));
        return result;
    }
}
}

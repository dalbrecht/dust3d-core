// SPDX-License-Identifier: MIT
#include "WorkerUnixResources.h"
#include <cerrno>
#include <limits>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(std::numeric_limits<rlim_t>::is_integer && !std::numeric_limits<rlim_t>::is_signed
                && std::numeric_limits<rlim_t>::digits <= 64,
            "Worker resource conversion requires an unsigned resource type up to 64 bits");
        static_assert(RLIM_INFINITY > 0 && RLIM_SAVED_CUR == RLIM_INFINITY && RLIM_SAVED_MAX == RLIM_INFINITY,
            "Worker resource conversion requires reviewed infinity/saved sentinel semantics");
        constexpr uint64_t maximumFinite = static_cast<uint64_t>(RLIM_INFINITY) - 1;
        int systemResource(ResourceKind resource)
        {
            switch (resource) {
            case ResourceKind::cpuSeconds:
                return RLIMIT_CPU;
            case ResourceKind::fileBytes:
                return RLIMIT_FSIZE;
            case ResourceKind::coreBytes:
                return RLIMIT_CORE;
            case ResourceKind::descriptors:
                return RLIMIT_NOFILE;
            case ResourceKind::addressBytes:
                return RLIMIT_AS;
            }
            throw ResourceError(ResourceErrorCode::unsupportedResource, resource, "Unknown Unix worker resource");
        }
        std::optional<uint64_t> finite(rlim_t value, ResourceKind resource)
        {
            if (value == RLIM_INFINITY)
                return std::nullopt;
            if (value > maximumFinite)
                throw ResourceError(ResourceErrorCode::invalidInherited, resource, "Inherited resource value exceeds supported finite range");
            return static_cast<uint64_t>(value);
        }
    }
    int SystemUnixResourceCalls::read(int resource, rlimit& value)
    {
        if (::getrlimit(static_cast<decltype(RLIMIT_CPU)>(resource), &value) == 0)
            return 0;
        return errno;
    }
    int SystemUnixResourceCalls::write(int resource, const rlimit& value)
    {
        if (::setrlimit(static_cast<decltype(RLIMIT_CPU)>(resource), &value) == 0)
            return 0;
        return errno;
    }
    UnixResourceSystem::UnixResourceSystem(UnixResourceCalls& calls)
        : m_calls(calls)
    {
    }
    ResourceCapability UnixResourceSystem::inspect(ResourceKind resource)
    {
        rlimit value {};
        const auto error = m_calls.read(systemResource(resource), value);
        if (error)
            throw ResourceSetupError(ResourceSetupOperation::inspect, resource, error);
        return { true, maximumFinite, { finite(value.rlim_cur, resource), finite(value.rlim_max, resource) } };
    }
    void UnixResourceSystem::apply(const PlannedResourceLimit& limit)
    {
        const int resource = systemResource(limit.resource);
        if (limit.effective.soft > limit.effective.hard)
            throw ResourceError(ResourceErrorCode::invalidPolicy, limit.resource, "Unix worker resource caps are reversed");
        if (limit.effective.hard > maximumFinite)
            throw ResourceError(ResourceErrorCode::unrepresentable, limit.resource, "Unix worker cap collides with infinity or exceeds finite range");
        const rlimit value { static_cast<rlim_t>(limit.effective.soft), static_cast<rlim_t>(limit.effective.hard) };
        const auto error = m_calls.write(resource, value);
        if (error)
            throw ResourceSetupError(ResourceSetupOperation::apply, limit.resource, error);
    }
}
}

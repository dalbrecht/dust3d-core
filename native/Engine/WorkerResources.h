// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_RESOURCES_H_
#define DUST3D_NATIVE_WORKER_RESOURCES_H_
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dust3d {
namespace worker {
    /// Index order for ResourceEnvironment::resources, also the setup order.
    enum class ResourceKind : uint8_t { cpuSeconds,
        fileBytes,
        coreBytes,
        descriptors,
        addressBytes };
    /// Requested/effective caps are always finite; zero is permitted only for core
    /// dumps or an already stricter inherited limit. No value encodes infinity.
    struct ResourceBudget {
        uint64_t soft;
        uint64_t hard;
    };
    /// nullopt explicitly represents OS infinity, not zero or a numeric sentinel.
    struct InheritedResourceBudget {
        std::optional<uint64_t> soft;
        std::optional<uint64_t> hard;
    };
    struct ResourceCapability {
        bool isSupported;
        /// Largest finite value convertible to the platform resource type without
        /// colliding with its infinity sentinel. Supplied by a trusted adapter.
        uint64_t maximumFinite;
        InheritedResourceBudget inherited;
    };
    enum class ResourceRuntime { unknown,
        ordinary,
        addressSanitized };
    struct ResourceEnvironment {
        std::array<ResourceCapability, 5> resources;
        ResourceRuntime runtime = ResourceRuntime::unknown;
    };
    /// Explicit units; core dumps are always disabled by the resulting plan.
    /// No default virtual address cap or resident-memory guarantee is supplied.
    struct ResourceProfile {
        ResourceBudget cpuSeconds;
        ResourceBudget fileBytes;
        /// Descriptor-number ceiling (highest admissible descriptor plus one),
        /// not a count of currently open files; startup viability is separate.
        ResourceBudget descriptors;
        std::optional<ResourceBudget> addressBytes;
    };
    struct PlannedResourceLimit {
        ResourceKind resource;
        ResourceBudget effective;
    };
    enum class ResourceErrorCode { invalidPolicy,
        unsupportedResource,
        unrepresentable,
        invalidInherited,
        incompatibleRuntime };
    class ResourceError : public std::runtime_error {
    public:
        ResourceError(ResourceErrorCode code, ResourceKind resource, const char* message);
        ResourceErrorCode code() const noexcept { return m_code; }
        ResourceKind resource() const noexcept { return m_resource; }

    private:
        ResourceErrorCode m_code;
        ResourceKind m_resource;
    };
    /// Validate explicit requested budgets, platform representability, inherited
    /// limits and runtime compatibility; return four or five finite effective caps.
    /// Throws ResourceError with the original resource/code; no OS calls, process
    /// changes or input mutations occur, including on a late validation failure.
    /// Inherited stricter limits are never raised. ASan with a requested virtual
    /// cap is rejected. Actual startup viability/application/containment and trusted
    /// launch-profile identity are separate helper-adapter responsibilities (#123).
    /// Example: planWorkerResources({{10,20},{1024,2048},{16,32},std::nullopt}, env).
    std::vector<PlannedResourceLimit> planWorkerResources(const ResourceProfile& profile, const ResourceEnvironment& environment);
}
}
#endif

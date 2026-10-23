// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_RESOURCE_SETUP_H_
#define DUST3D_NATIVE_WORKER_RESOURCE_SETUP_H_
#include "WorkerResources.h"

namespace dust3d {
namespace worker {
    enum class ResourceSetupOperation { inspect,
        apply };
    /// Original OS cause, with resource and operation; helper callers must report
    /// it and exit before document work. Earlier mutations are not rolled back.
    class ResourceSetupError : public std::runtime_error {
    public:
        ResourceSetupError(ResourceSetupOperation operation, ResourceKind resource, int systemError);
        ResourceSetupOperation operation() const noexcept { return m_operation; }
        ResourceKind resource() const noexcept { return m_resource; }
        int systemError() const noexcept { return m_systemError; }

    private:
        ResourceSetupOperation m_operation;
        ResourceKind m_resource;
        int m_systemError;
    };
    /// Private helper boundary. Production implementations mutate only a fresh
    /// helper, never the app/test runner. Recording implementations do no OS work.
    class ResourceSystem {
    public:
        virtual ~ResourceSystem() = default;
        virtual ResourceCapability inspect(ResourceKind resource) = 0;
        /// Apply one validated plan entry; caller exclusively owns process setup.
        virtual void apply(const PlannedResourceLimit& limit) = 0;
    };
    /// Inspect requested resources, validate the complete profile, then apply in
    /// stable order. Returns effective caps only after every apply succeeds.
    /// Propagates the original ResourceError/ResourceSetupError. No retries or
    /// rollback on partial setup. Caller must prevent document work on exception.
    /// This function is not a sandbox or a helper launch/profile protocol.
    std::vector<PlannedResourceLimit> prepareWorkerResources(const ResourceProfile& profile, ResourceRuntime runtime, ResourceSystem& system);
}
}
#endif

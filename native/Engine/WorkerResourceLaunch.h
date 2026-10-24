// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_RESOURCE_LAUNCH_H_
#define DUST3D_NATIVE_WORKER_RESOURCE_LAUNCH_H_
#include "WorkerResources.h"
#include <string>

namespace dust3d {
namespace worker {
    struct ResourceLaunch {
        ResourceRuntime runtime;
        ResourceProfile profile;
    };
    enum class ResourceLaunchErrorCode { invalidArguments,
        unsupportedVersion,
        runtimeMismatch,
        invalidNumber };
    class ResourceLaunchError : public std::runtime_error {
    public:
        ResourceLaunchError(ResourceLaunchErrorCode code, const char* message);
        ResourceLaunchErrorCode code() const noexcept { return m_code; }

    private:
        ResourceLaunchErrorCode m_code;
    };
    /// Canonical v1 argv excluding executable: flag, runtime, CPU soft/hard,
    /// file-byte soft/hard, descriptor soft/hard, optional virtual-byte soft/hard.
    /// Core dumps are always disabled. No production budgets/defaults selected.
    /// Throws original ResourceError for invalid profile/runtime before encoding.
    std::vector<std::string> encodeResourceLaunch(const ResourceLaunch& launch);
    /// Bound 8/10 tokens to 32 bytes each; require canonical unsigned decimals,
    /// supported version and exact compiled-helper runtime before policy validation.
    /// Throws ResourceLaunchError or original ResourceError. Makes no OS calls.
    /// Launcher must trust helper identity separately; argv is not a sandbox.
    ResourceLaunch decodeResourceLaunch(const std::vector<std::string>& arguments, ResourceRuntime actualRuntime);
}
}
#endif

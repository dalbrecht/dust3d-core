// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_RESOURCE_OBSERVATION_H_
#define DUST3D_NATIVE_WORKER_RESOURCE_OBSERVATION_H_
#include "WorkerResourceLaunch.h"

namespace dust3d {
namespace worker {
    constexpr size_t maximumResourceObservationBytes = 1024;
    struct ResourceObservation {
        ResourceRuntime runtime = ResourceRuntime::unknown;
        /// CPU/file/core/descriptor effective soft/hard values in ResourceKind order.
        std::array<ResourceBudget, 4> effective {};
        /// Original diagnostic bytes after this prefix remain opaque and owned by
        /// the transport. They are not decoded, discarded or logged here.
        size_t diagnosticPrefixBytes = 0;
    };
    enum class ResourceObservationErrorCode { invalidReport,
        runtimeMismatch,
        relaxedLimit };
    class ResourceObservationError : public std::runtime_error {
    public:
        ResourceObservationError(ResourceObservationErrorCode code, std::optional<ResourceKind> resource, const std::string& message);
        ResourceObservationErrorCode code() const noexcept { return m_code; }
        std::optional<ResourceKind> resource() const noexcept { return m_resource; }

    private:
        ResourceObservationErrorCode m_code;
        std::optional<ResourceKind> m_resource;
    };
    /// Parse one complete canonical v1 prefix within the first 1024 bytes. Require
    /// requested runtime, ordered finite budgets and no relaxed requested soft/hard
    /// cap, including disabled core dumps. Preserve an opaque binary tail by offset.
    /// Makes no OS calls and does not admit a process/frame/revision or authenticate
    /// the helper. Throws ResourceObservationError, original ResourceError for an
    /// unsupported/invalid requested profile, or allocation errors.
    ResourceObservation decodeResourceObservation(const uint8_t* bytes, size_t size, const ResourceLaunch& requested);
}
}
#endif

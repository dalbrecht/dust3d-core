// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UNIX_RESOURCES_H_
#define DUST3D_NATIVE_WORKER_UNIX_RESOURCES_H_
#include "WorkerResourceSetup.h"
#include <sys/resource.h>

namespace dust3d {
namespace worker {
    /// Unix-only private syscall seam. Returns zero or a captured original errno;
    /// callers never depend on ambient errno after the call.
    class UnixResourceCalls {
    public:
        virtual ~UnixResourceCalls() = default;
        virtual int read(int resource, rlimit& value) = 0;
        virtual int write(int resource, const rlimit& value) = 0;
    };
    /// Actual process calls: instantiate only in the fresh helper. Tests must use
    /// a recording seam and must never change limits on the test runner.
    class SystemUnixResourceCalls final : public UnixResourceCalls {
    public:
        int read(int resource, rlimit& value) override;
        int write(int resource, const rlimit& value) override;
    };
    /// Checked macOS/Linux resource conversion and original-error preservation.
    /// Borrowed calls must outlive this object. inspect is read-only; apply must
    /// receive a validated plan with exclusively owned, unchanged process setup.
    /// Helper activation, startup viability and launch identity are separate gates.
    class UnixResourceSystem final : public ResourceSystem {
    public:
        explicit UnixResourceSystem(UnixResourceCalls& calls);
        ResourceCapability inspect(ResourceKind resource) override;
        void apply(const PlannedResourceLimit& limit) override;

    private:
        UnixResourceCalls& m_calls;
    };
}
}
#endif

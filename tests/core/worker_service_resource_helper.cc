// SPDX-License-Identifier: MIT
#include <Dust3DWorkerResources.h>
#include <cstdio>

// CTest launches this dedicated short-lived helper. Never link its main into a host/test runner.
// Only successful ordinary application/readback is exercised: no exhaustion, faults or signal probes.
int main()
{
    const Dust3DWorkerResourceProfile profile {
        DUST3D_WORKER_RESOURCE_ABI_VERSION, { 300, 600 }, { 67108864, 134217728 }, { 1024, 2048 }, 256
    };
    const auto result = dust3d_worker_resources_apply(profile);
    if (result.status != DUST3D_WORKER_RESOURCE_READY || result.applied_count != 4 || result.verified_count != 4
        || result.effective[2].soft != 0 || result.effective[2].hard != 0
        || result.effective[3].soft < profile.minimum_descriptor_ceiling) {
        std::fprintf(stderr, "Resource helper setup rejected: status=%u operation=%u errno=%d applied=%u verified=%u\n",
            static_cast<unsigned>(result.status), static_cast<unsigned>(result.operation), result.system_error,
            result.applied_count, result.verified_count);
        return 1;
    }
    std::puts("Fresh dedicated helper applied and read back four resource caps; no native work or exhaustion attempted");
    return 0;
}

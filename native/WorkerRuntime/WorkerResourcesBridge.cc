// SPDX-License-Identifier: MIT
#include "WorkerResourcesBridge.h"
#include <cstdio>
#include <exception>

using namespace dust3d::worker;
namespace dust3d {
namespace worker {
    Dust3DWorkerResourceResult initialWorkerResourceResult(ResourceRuntime runtime) noexcept
    {
        Dust3DWorkerResourceResult result {};
        result.abi_version = DUST3D_WORKER_RESOURCE_ABI_VERSION;
        result.status = DUST3D_WORKER_RESOURCE_INTERNAL_FAILED;
        result.runtime = runtime == ResourceRuntime::ordinary ? 1 : runtime == ResourceRuntime::addressSanitized ? 2
                                                                                                                 : 0;
        return result;
    }
    void recordWorkerResourceCause(Dust3DWorkerResourceResult& result, const char* message) noexcept
    {
        const int size = std::snprintf(result.message, sizeof(result.message), "%s", message ? message : "");
        result.message_truncated = size < 0 || static_cast<size_t>(size) >= sizeof(result.message);
    }
}
}
namespace {
void resource(Dust3DWorkerResourceResult& result, ResourceKind kind) noexcept
{
    result.has_detail_resource = 1;
    result.detail_resource = static_cast<uint32_t>(kind);
}
class TrackedResources final : public ResourceSystem {
public:
    TrackedResources(ResourceSystem& system, Dust3DWorkerResourceResult& result)
        : m_system(system)
        , m_result(result)
    {
    }
    ResourceCapability inspect(ResourceKind kind) override
    {
        m_result.operation = m_verifying ? DUST3D_WORKER_RESOURCE_OPERATION_VERIFY : DUST3D_WORKER_RESOURCE_OPERATION_INSPECT;
        resource(m_result, kind);
        return m_system.inspect(kind);
    }
    void apply(const PlannedResourceLimit& limit) override
    {
        if (m_result.applied_count >= DUST3D_WORKER_RESOURCE_COUNT)
            throw ResourceError(ResourceErrorCode::invalidPolicy, limit.resource, "Unexpected additional service resource limit");
        m_result.operation = DUST3D_WORKER_RESOURCE_OPERATION_APPLY;
        resource(m_result, limit.resource);
        m_result.application_started = 1;
        m_system.apply(limit);
        m_result.effective[m_result.applied_count] = { limit.effective.soft, limit.effective.hard };
        ++m_result.applied_count;
    }
    void beginVerification() noexcept { m_verifying = true; }

private:
    ResourceSystem& m_system;
    Dust3DWorkerResourceResult& m_result;
    bool m_verifying = false;
};
}
namespace dust3d {
namespace worker {
    Dust3DWorkerResourceResult applyWorkerResourceProfile(const Dust3DWorkerResourceProfile& profile, ResourceRuntime runtime, ResourceSystem& system) noexcept
    {
        auto result = initialWorkerResourceResult(runtime);
        if (profile.abi_version != DUST3D_WORKER_RESOURCE_ABI_VERSION) {
            result.status = DUST3D_WORKER_RESOURCE_POLICY_REJECTED;
            result.detail_code = -1;
            recordWorkerResourceCause(result, "Unsupported service resource profile ABI");
            return result;
        }
        if (!profile.minimum_descriptor_ceiling) {
            result.status = DUST3D_WORKER_RESOURCE_POLICY_REJECTED;
            result.detail_code = static_cast<int32_t>(ResourceErrorCode::invalidPolicy);
            resource(result, ResourceKind::descriptors);
            recordWorkerResourceCause(result, "Service resource profile requires an explicit startup descriptor floor");
            return result;
        }
        try {
            const ResourceProfile requested { { profile.cpu_seconds.soft, profile.cpu_seconds.hard },
                { profile.file_bytes.soft, profile.file_bytes.hard }, { profile.descriptors.soft, profile.descriptors.hard }, std::nullopt };
            TrackedResources tracked(system, result);
            const auto plan = prepareWorkerResources(requested, runtime, tracked, profile.minimum_descriptor_ceiling);
            tracked.beginVerification();
            for (const auto& entry : plan) {
                const auto actual = tracked.inspect(entry.resource).inherited;
                if (actual.soft != entry.effective.soft || actual.hard != entry.effective.hard) {
                    result.status = DUST3D_WORKER_RESOURCE_READBACK_REJECTED;
                    result.detail_code = static_cast<int32_t>(ResourceErrorCode::invalidInherited);
                    recordWorkerResourceCause(result, "Effective service resource caps differ after application");
                    return result;
                }
                ++result.verified_count;
            }
            result.status = DUST3D_WORKER_RESOURCE_READY;
            result.operation = DUST3D_WORKER_RESOURCE_OPERATION_NONE;
            result.has_detail_resource = 0;
            result.detail_resource = 0;
        } catch (const ResourceSetupError& error) {
            result.status = DUST3D_WORKER_RESOURCE_SYSTEM_FAILED;
            // A readback syscall remains VERIFY, independent of the original inspect syscall classification.
            if (result.operation != DUST3D_WORKER_RESOURCE_OPERATION_VERIFY)
                result.operation = error.operation() == ResourceSetupOperation::inspect ? DUST3D_WORKER_RESOURCE_OPERATION_INSPECT : DUST3D_WORKER_RESOURCE_OPERATION_APPLY;
            resource(result, error.resource());
            result.system_error = error.systemError();
            recordWorkerResourceCause(result, error.what());
        } catch (const ResourceError& error) {
            result.status = DUST3D_WORKER_RESOURCE_POLICY_REJECTED;
            result.operation = DUST3D_WORKER_RESOURCE_OPERATION_NONE;
            resource(result, error.resource());
            result.detail_code = static_cast<int32_t>(error.code());
            recordWorkerResourceCause(result, error.what());
        } catch (const std::exception& error) {
            result.status = DUST3D_WORKER_RESOURCE_INTERNAL_FAILED;
            recordWorkerResourceCause(result, error.what());
        } catch (...) {
            result.status = DUST3D_WORKER_RESOURCE_INTERNAL_FAILED;
            recordWorkerResourceCause(result, "Unknown service resource preparation failure");
        }
        return result;
    }
}
}
extern "C" uint32_t dust3d_worker_resource_abi_version(void) noexcept { return DUST3D_WORKER_RESOURCE_ABI_VERSION; }

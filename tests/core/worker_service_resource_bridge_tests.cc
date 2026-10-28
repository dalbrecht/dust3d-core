// SPDX-License-Identifier: MIT
#include <Dust3DWorkerResources.h>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <native/WorkerRuntime/WorkerResourcesBridge.h>
#include <stdexcept>
#include <string>
#include <vector>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
// Recording values only. No getrlimit/setrlimit or native document operation occurs.
class RecordedResources final : public ResourceSystem {
public:
    std::array<InheritedResourceBudget, 4> inherited {};
    std::vector<ResourceKind> reads;
    std::vector<ResourceKind> writes;
    std::optional<ResourceKind> failRead;
    std::optional<ResourceKind> failWrite;
    bool corruptReadback = false;
    bool failReadback = false;
    std::optional<std::string> unexpectedMessage;
    bool unknownException = false;
    ResourceCapability inspect(ResourceKind kind) override
    {
        reads.push_back(kind);
        if (unexpectedMessage)
            throw std::runtime_error(*unexpectedMessage);
        if (unknownException)
            throw 7;
        if (failRead == kind || (failReadback && !writes.empty() && kind == ResourceKind::coreBytes))
            throw ResourceSetupError(ResourceSetupOperation::inspect, kind, EACCES);
        auto value = inherited.at(static_cast<size_t>(kind));
        if (corruptReadback && !writes.empty() && kind == ResourceKind::fileBytes)
            value.soft = 1;
        return { true, 1000000, value };
    }
    void apply(const PlannedResourceLimit& limit) override
    {
        if (failWrite == limit.resource)
            throw ResourceSetupError(ResourceSetupOperation::apply, limit.resource, EPERM);
        writes.push_back(limit.resource);
        inherited.at(static_cast<size_t>(limit.resource)) = { limit.effective.soft, limit.effective.hard };
    }
};
Dust3DWorkerResourceProfile profile()
{
    return { DUST3D_WORKER_RESOURCE_ABI_VERSION, { 10, 20 }, { 1024, 2048 }, { 128, 256 }, 64 };
}
void ordinary()
{
    RecordedResources system;
    const auto result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, system);
    require(result.status == DUST3D_WORKER_RESOURCE_READY, "Ordinary recording profile must prepare");
    require(result.applied_count == 4 && result.verified_count == 4 && result.application_started, "All applied/readback observations required");
    require(result.effective[0].soft == 10 && result.effective[1].hard == 2048 && result.effective[2].hard == 0 && result.effective[3].soft == 128, "Exact units and disabled core dumps preserved");
    require(result.operation == DUST3D_WORKER_RESOURCE_OPERATION_NONE && !result.has_detail_resource && !result.system_error && !result.message[0], "Success must not fabricate diagnostics");
    require(system.reads.size() == 8 && system.writes.size() == 4, "Inspect before apply and read back after all applications");
}
void originalFailures()
{
    RecordedResources read;
    read.failRead = ResourceKind::fileBytes;
    auto result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, read);
    require(result.status == DUST3D_WORKER_RESOURCE_SYSTEM_FAILED && result.operation == DUST3D_WORKER_RESOURCE_OPERATION_INSPECT, "Original inspection failure classification");
    require(result.has_detail_resource && result.detail_resource == static_cast<uint32_t>(ResourceKind::fileBytes) && result.system_error == EACCES, "Original inspection resource/errno");
    require(!result.application_started && result.applied_count == 0 && read.writes.empty(), "Failed inspection must precede every mutation");
    RecordedResources write;
    write.failWrite = ResourceKind::fileBytes;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, write);
    require(result.status == DUST3D_WORKER_RESOURCE_SYSTEM_FAILED && result.operation == DUST3D_WORKER_RESOURCE_OPERATION_APPLY && result.system_error == EPERM, "Original application failure");
    require(result.application_started && result.applied_count == 1 && result.verified_count == 0 && write.writes.size() == 1, "Partial application must remain visible without rollback claim");
    RecordedResources verify;
    verify.corruptReadback = true;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, verify);
    require(result.status == DUST3D_WORKER_RESOURCE_READBACK_REJECTED && result.operation == DUST3D_WORKER_RESOURCE_OPERATION_VERIFY, "Readback must match applied values");
    require(result.applied_count == 4 && result.verified_count == 1 && result.has_detail_resource && result.detail_resource == static_cast<uint32_t>(ResourceKind::fileBytes), "Preserve independent readback progress/resource");
}
void inheritedAndReadback()
{
    RecordedResources stricter;
    stricter.inherited[0] = { 5, 8 };
    stricter.inherited[3] = { 96, 100 };
    auto result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, stricter);
    require(result.status == DUST3D_WORKER_RESOURCE_READY && result.effective[0].soft == 5 && result.effective[0].hard == 8 && result.effective[3].hard == 100, "Inherited stricter caps must never be raised");
    RecordedResources belowFloor;
    belowFloor.inherited[3] = { 32, 32 };
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, belowFloor);
    require(result.status == DUST3D_WORKER_RESOURCE_POLICY_REJECTED && belowFloor.writes.empty() && !result.application_started, "Inherited descriptor ceiling must satisfy startup before mutation");
    RecordedResources readback;
    readback.failReadback = true;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, readback);
    require(result.status == DUST3D_WORKER_RESOURCE_SYSTEM_FAILED && result.operation == DUST3D_WORKER_RESOURCE_OPERATION_VERIFY && result.system_error == EACCES, "Readback syscall retains original errno and verification phase");
    require(result.applied_count == 4 && result.verified_count == 2 && result.detail_resource == static_cast<uint32_t>(ResourceKind::coreBytes), "Readback failure preserves exact successful progress");
}
void exceptionMetadataAndRuntime()
{
    RecordedResources standard;
    standard.unexpectedMessage = "Original recorded setup cause";
    auto result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, standard);
    require(result.status == DUST3D_WORKER_RESOURCE_INTERNAL_FAILED && result.operation == DUST3D_WORKER_RESOURCE_OPERATION_INSPECT && !result.application_started, "Unexpected recording error must remain inside result boundary");
    require(std::string(result.message) == *standard.unexpectedMessage && !result.message_truncated, "Original standard exception diagnostic retained");
    standard.unexpectedMessage = std::string(300, 'x');
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, standard);
    require(result.message_truncated && std::strlen(result.message) == 255 && std::string(result.message) == std::string(255, 'x'), "Bounded diagnostic copying must mark actual truncation");
    RecordedResources unknown;
    unknown.unknownException = true;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::ordinary, unknown);
    require(result.status == DUST3D_WORKER_RESOURCE_INTERNAL_FAILED && std::string(result.message) == "Unknown service resource preparation failure", "Non-standard recording exception must not escape");
    RecordedResources sanitized;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::addressSanitized, sanitized);
    require(result.status == DUST3D_WORKER_RESOURCE_READY && result.runtime == 2, "Synthetic instrumented runtime retains classification without address cap");
    RecordedResources unidentified;
    result = applyWorkerResourceProfile(profile(), ResourceRuntime::unknown, unidentified);
    require(result.status == DUST3D_WORKER_RESOURCE_POLICY_REJECTED && result.runtime == 0 && unidentified.writes.empty(), "Unknown runtime is never promoted to ready");
    require(dust3d_worker_resource_abi_version() == DUST3D_WORKER_RESOURCE_ABI_VERSION, "Public no-I/O ABI version must agree");
}
void preflight()
{
    RecordedResources system;
    auto requested = profile();
    requested.abi_version = 0;
    auto result = applyWorkerResourceProfile(requested, ResourceRuntime::ordinary, system);
    require(result.status == DUST3D_WORKER_RESOURCE_POLICY_REJECTED && system.reads.empty() && system.writes.empty(), "Unknown ABI must not inspect or apply");
    requested = profile();
    requested.minimum_descriptor_ceiling = 0;
    result = applyWorkerResourceProfile(requested, ResourceRuntime::ordinary, system);
    require(result.status == DUST3D_WORKER_RESOURCE_POLICY_REJECTED && system.reads.empty(), "Missing startup floor is not a permissive default");
    requested = profile();
    requested.cpu_seconds.soft = 0;
    result = applyWorkerResourceProfile(requested, ResourceRuntime::ordinary, system);
    require(result.status == DUST3D_WORKER_RESOURCE_POLICY_REJECTED && result.has_detail_resource && result.detail_resource == static_cast<uint32_t>(ResourceKind::cpuSeconds), "Original pure planner rejection");
    require(system.writes.empty() && !result.application_started, "Invalid plan never reaches recording apply");
}
}
int main()
{
    try {
        ordinary();
        originalFailures();
        preflight();
        inheritedAndReadback();
        exceptionMetadataAndRuntime();
        std::cout << "Recorded resource bridge contracts passed; no process limits changed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

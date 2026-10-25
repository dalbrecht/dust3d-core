// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerBuild.h>
#include <native/Engine/WorkerQueue.h>
#include <native/Engine/WorkerTransport.h>
#include <stdexcept>
#include <sys/resource.h>
#include <utility>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
Frame request(const std::vector<uint8_t>& archive, uint64_t sequence)
{
    Header header;
    header.identity.document[0] = 1;
    header.identity.request = sequence;
    header.identity.revision = 5;
    header.payloadBytes = archive.size();
    return { header, archive };
}
std::vector<uint8_t> readFixture(const std::filesystem::path& path)
{
    require(std::filesystem::file_size(path) < 1024 * 1024, "Trusted fixture must remain small");
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot open trusted fixture");
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
template <typename Error, typename Action>
void rejects(Action action)
{
    try {
        action();
    } catch (const Error&) {
        return;
    }
    throw std::runtime_error("Invalid transport preflight was admitted");
}
void preflight(const std::string& worker)
{
    const auto input = request({}, 99);
    for (const auto& path : { std::string(), std::string("relative/worker"), worker + std::string(1, '\0') + "suffix" }) {
        try {
            runWorkerJob(path, input);
        } catch (const TransportError& cause) {
            require(cause.code() == TransportErrorCode::invalidConfiguration && cause.operatingSystemCode() == EINVAL, "Path preflight preserves typed cause");
            continue;
        }
        throw std::runtime_error("Invalid executable path admitted");
    }
    for (size_t index = 0; index < 4; ++index) {
        TransportLimits limits;
        if (index == 0)
            limits.maximumIoChunkBytes = 0;
        if (index == 1)
            limits.maximumIoChunkBytes = 65537;
        if (index == 2)
            limits.maximumCapturedDiagnosticBytes = 0;
        if (index == 3)
            limits.maximumCapturedDiagnosticBytes = maximumDiagnosticBytes + 1;
        rejects<TransportError>([&] { runWorkerJob(worker, input, limits); });
    }
    CollectedJob owned;
    owned.outcome = { true, 0 };
    owned.reply = { 1, 2 };
    owned.diagnostic = { 0, 255, 27 };
    owned.diagnosticTruncated = true;
    WorkerStopped retired(LifetimeReason::deadline, std::move(owned));
    owned.diagnostic = { 7 };
    require(retired.reason() == LifetimeReason::deadline && retired.collected()
            && retired.collected()->outcome.exitedNormally && retired.collected()->outcome.exitCode == 0
            && retired.collected()->reply == std::vector<uint8_t>({ 1, 2 })
            && retired.collected()->diagnostic == std::vector<uint8_t>({ 0, 255, 27 }) && retired.collected()->diagnosticTruncated,
        "Retirement error independently owns the original outcome and opaque diagnostics");
    WorkerCancellation cancelled;
    require(!cancelled.isCancellationRequested(), "Cancellation starts clear");
    cancelled.requestCancellation();
    cancelled.requestCancellation();
    require(cancelled.isCancellationRequested(), "Cancellation is idempotent and sticky");
    WorkerCleanupReport cleanup;
    cleanup.stopError = 123;
    WorkerAttemptReport attempt;
    attempt.captured = true;
    attempt.reply = { 123 };
    WorkerSupervision supervision;
    supervision.attemptReport = &attempt;
    supervision.cleanupReport = &cleanup;
    supervision.cancellation = &cancelled;
    try {
        runSupervisedWorkerJob(worker + "/missing-helper", input, {}, supervision);
        throw std::runtime_error("Cancelled work unexpectedly started");
    } catch (const WorkerStopped& cause) {
        require(cause.reason() == LifetimeReason::cancelled && !cause.collected(),
            "Pre-start cancellation returns a typed cause without any spawned child");
    }
    require(!cleanup.attemptedStop && !cleanup.attemptedReap && !cleanup.stopError && !cleanup.waitFailure && !cleanup.outcome, "Pre-start cancellation resets stale cleanup output without process operations");
    require(!attempt.captured && !attempt.outcome && attempt.reply.empty() && attempt.diagnostic.empty(), "Pre-start cancellation resets partial diagnostic output before any collector starts");
    supervision.lifetime.workMilliseconds = 0;
    rejects<LifetimeError>([&] { runSupervisedWorkerJob(worker, input, {}, supervision); });
    auto invalid = input;
    invalid.header.kind = FrameKind::response;
    rejects<AdmissionError>([&] { runWorkerJob(worker, invalid); });
    invalid = input;
    invalid.header.payloadBytes = 1;
    rejects<ProtocolError>([&] { runWorkerJob(worker, invalid); });
    try {
        runWorkerJob(worker + "/missing-helper", input);
    } catch (const TransportError& cause) {
        require(cause.code() == TransportErrorCode::spawn && cause.operatingSystemCode() == ENOTDIR,
            "Direct spawn failure preserves original path error");
        return;
    }
    throw std::runtime_error("Missing executable unexpectedly started");
}
void verify(const std::string& worker, const std::vector<uint8_t>& archive, uint64_t sequence, size_t chunk, bool resourceLimited = false)
{
    auto input = request(archive, sequence);
    TransportLimits limits;
    limits.maximumIoChunkBytes = chunk;
    std::optional<ResourceObservation> observation;
    CollectedJob collected;
    if (resourceLimited) {
        ResourceLaunch launch { compiledResourceRuntime(), { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } };
        auto result = runResourceLimitedWorkerJob(worker, input, launch, limits);
        observation = result.observation;
        collected = std::move(result.collected);
        require(observation->runtime == launch.runtime && observation->diagnosticPrefixBytes == collected.diagnostic.size()
                && !collected.diagnosticTruncated && observation->effective[2].soft == 0 && observation->effective[2].hard == 0,
            "Actual resource helper reports matching complete bounded setup before result admission");
    } else {
        WorkerCleanupReport cleanup;
        cleanup.stopError = 123;
        WorkerAttemptReport attempt;
        attempt.captured = true;
        attempt.diagnostic = { 123 };
        WorkerSupervision supervision;
        supervision.attemptReport = &attempt;
        supervision.cleanupReport = &cleanup;
        collected = sequence == 1 ? runSupervisedWorkerJob(worker, input, limits, supervision) : runWorkerJob(worker, input, limits);
        if (sequence == 1) {
            require(!cleanup.attemptedStop && !cleanup.attemptedReap && !cleanup.stopError && !cleanup.waitFailure && !cleanup.outcome, "Ordinary supervised completion requires no exceptional cleanup");
            require(!attempt.captured && !attempt.outcome && attempt.reply.empty() && attempt.diagnostic.empty(), "Normal completion never becomes a partial-error report");
        }
    }
    require(collected.outcome.exitedNormally && collected.outcome.exitCode == 0, "Real helper must exit normally");
    if (!observation && (!collected.diagnostic.empty() || collected.diagnosticTruncated)) {
        // Surface unexpected runtime/sanitizer causes as bounded escaped data, never terminal controls.
        constexpr char digits[] = "0123456789abcdef";
        std::cerr << "Unexpected child diagnostic bytes (hex, truncated=" << collected.diagnosticTruncated << "): ";
        for (const auto byte : collected.diagnostic)
            std::cerr << digits[byte >> 4] << digits[byte & 15];
        std::cerr << '\n';
        throw std::runtime_error("Ordinary helper emitted unexpected diagnostic data");
    }
    PendingReply pending(input.header);
    auto admitted = pending.complete(input.header.identity, collected.outcome, collected.reply.data(), collected.reply.size());
    collected.reply.clear();
    require(pending.isRetired(), "Actual transport result retires once");
    Dust3DError error {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> reference(dust3d_mesh_load(archive.data(), archive.size(), &error), dust3d_mesh_destroy);
    require((sequence <= 2) == bool(reference), "Licensed fixtures require real geometry; invalid archives require failure");
    if (!reference) {
        const auto& failure = std::get<FailurePayload>(admitted);
        const auto end = std::find(std::begin(error.message), std::end(error.message), '\0');
        require(error.status == DUST3D_STATUS_INVALID_DOCUMENT && failure.status == FailureStatus::invalidDocument
                && failure.cause == std::vector<uint8_t>(error.message, end),
            "Actual invalid archive preserves original cause");
        return;
    }
    const auto& mesh = std::get<MeshPayload>(admitted);
    require(mesh.vertices.size() == dust3d_mesh_vertex_count(reference.get())
            && mesh.nodeCount == dust3d_mesh_node_count(reference.get())
            && mesh.partCount == dust3d_mesh_part_count(reference.get())
            && mesh.warning == dust3d_mesh_warning(reference.get()),
        "Pipe transport preserves source counts and warnings");
    const auto* vertices = dust3d_mesh_vertices(reference.get());
    for (size_t index = 0; index < mesh.vertices.size(); ++index)
        require(std::memcmp(mesh.vertices[index].position.data(), vertices[index].position, 3 * sizeof(float)) == 0
                && std::memcmp(mesh.vertices[index].normal.data(), vertices[index].normal, 3 * sizeof(float)) == 0
                && std::memcmp(mesh.vertices[index].color.data(), vertices[index].color, 4 * sizeof(float)) == 0,
            "Pipe transport preserves every native vertex bit");
}
std::array<std::pair<rlim_t, rlim_t>, 5> parentResourceLimits()
{
    std::array<std::pair<rlim_t, rlim_t>, 5> values {};
    const int resources[] = { RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_CORE, RLIMIT_NOFILE, RLIMIT_AS };
    for (size_t index = 0; index < values.size(); ++index) {
        rlimit value {};
        if (::getrlimit(static_cast<decltype(RLIMIT_CPU)>(resources[index]), &value) != 0) {
            const int cause = errno;
            throw std::runtime_error(std::string("Cannot inspect parent resource limit: ") + std::strerror(cause));
        }
        values[index] = { value.rlim_cur, value.rlim_max };
    }
    return values;
}
void resourcePreflight(const std::string& worker)
{
    const auto input = request({}, 100);
    ResourceLaunch launch { compiledResourceRuntime(), { { 600, 600 }, { 16777216, 16777216 }, { 128, 128 }, std::nullopt } };
    TransportLimits limits;
    limits.maximumCapturedDiagnosticBytes = maximumResourceObservationBytes - 1;
    try {
        runResourceLimitedWorkerJob(worker + "/missing-helper", input, launch, limits);
        throw std::runtime_error("Undersized resource diagnostic capture admitted");
    } catch (const TransportError& error) {
        require(error.code() == TransportErrorCode::invalidConfiguration && error.operatingSystemCode() == EINVAL, "Resource capture rejects before spawn");
    }
    launch.profile.addressBytes = ResourceBudget { 100, 200 };
    try {
        runResourceLimitedWorkerJob(worker + "/missing-helper", input, launch);
        throw std::runtime_error("Unsupported virtual profile started");
    } catch (const ResourceError& error) {
        require(error.code() == ResourceErrorCode::unsupportedResource && error.resource() == ResourceKind::addressBytes, "Unsupported resource profile rejects before spawn");
    }
    launch.profile.addressBytes.reset();
    WorkerCancellation cancelled;
    cancelled.requestCancellation();
    WorkerCleanupReport cleanup;
    cleanup.stopError = 123;
    WorkerAttemptReport attempt;
    attempt.captured = true;
    attempt.reply = { 123 };
    WorkerSupervision supervision;
    supervision.attemptReport = &attempt;
    supervision.cleanupReport = &cleanup;
    supervision.cancellation = &cancelled;
    try {
        runResourceLimitedWorkerJob(worker + "/missing-helper", input, launch, {}, supervision);
        throw std::runtime_error("Pre-cancelled resource work started");
    } catch (const WorkerStopped& error) {
        require(error.reason() == LifetimeReason::cancelled && !error.collected(), "Resource launch preserves pre-start cancellation without spawning");
    }
    CollectedJob failed;
    failed.outcome = { true, 64 };
    failed.reply = { 1, 2 };
    failed.diagnostic = { 0, 255, 27 };
    failed.diagnosticTruncated = true;
    try {
        admitResourceCompletion(std::move(failed), launch);
        throw std::runtime_error("Synthetic nonzero resource outcome admitted");
    } catch (const ResourceJobError& error) {
        failed.diagnostic = { 7 };
        require(error.code() == ResourceJobErrorCode::unsuccessfulExit && !error.observationFailure()
                && error.collected().outcome.exitCode == 64 && error.collected().reply == std::vector<uint8_t>({ 1, 2 })
                && error.collected().diagnostic == std::vector<uint8_t>({ 0, 255, 27 }) && error.collected().diagnosticTruncated,
            "Original synthetic failure owns all outcome/reply/binary diagnostic bytes, without parsing as success");
    }
    // Synthetic terminal-state data only; no OS signal or failing-helper probe.
    CollectedJob nonNormal;
    nonNormal.outcome = { false, 0 };
    try {
        admitResourceCompletion(std::move(nonNormal), launch);
        throw std::runtime_error("Synthetic non-normal resource outcome admitted");
    } catch (const ResourceJobError& error) {
        require(error.code() == ResourceJobErrorCode::unsuccessfulExit && !error.collected().outcome.exitedNormally,
            "Non-normal completion rejects before any metadata parsing");
    }
    const std::string prefix = std::string("resource-profile-v1 ") + (launch.runtime == ResourceRuntime::ordinary ? "ordinary" : "asan") + " 600 600 16777216 16777216 0 0 128 128\n";
    CollectedJob opaque;
    opaque.outcome = { true, 0 };
    opaque.reply = { 42 };
    opaque.diagnostic.assign(prefix.begin(), prefix.end());
    opaque.diagnostic.insert(opaque.diagnostic.end(), { 0, 255, 27 });
    opaque.diagnosticTruncated = true;
    auto admitted = admitResourceCompletion(std::move(opaque), launch);
    require(admitted.observation.diagnosticPrefixBytes == prefix.size() && admitted.collected.diagnosticTruncated
            && admitted.collected.diagnostic.size() == prefix.size() + 3 && admitted.collected.diagnostic.back() == 27,
        "Complete resource metadata preserves opaque binary tail and truncation state");
    PendingReply pending(input.header);
    rejects<ProtocolError>([&] { pending.complete(input.header.identity, admitted.collected.outcome, admitted.collected.reply.data(), admitted.collected.reply.size()); });
    require(pending.isRetired(), "Resource admission cannot replace normal frame/revision admission");
    CollectedJob malformed;
    malformed.outcome = { true, 0 };
    malformed.diagnostic = { 0, 255, 27 };
    try {
        admitResourceCompletion(std::move(malformed), launch);
        throw std::runtime_error("Malformed synthetic resource report admitted");
    } catch (const ResourceJobError& error) {
        require(error.code() == ResourceJobErrorCode::invalidObservation && error.observationFailure()
                && error.observationFailure()->code() == ResourceObservationErrorCode::invalidReport
                && std::strcmp(error.what(), error.observationFailure()->what()) == 0
                && error.collected().diagnostic == std::vector<uint8_t>({ 0, 255, 27 }),
            "Resource report rejection surfaces the original typed metadata cause and preserves opaque bytes");
    }
}
void queuedTransport(const std::string& worker, const std::vector<uint8_t>& archive)
{
    RequestQueue queue;
    queue.submit(request(archive, 1));
    auto first = queue.begin();
    auto collected = runWorkerJob(worker, *first);
    require(collected.diagnostic.empty() && !collected.diagnosticTruncated, "Ordinary queued helper emits no runtime diagnostic");
    queue.submit(request({ 1, 2, 3 }, 2));
    queue.submit(request(archive, 3));
    rejects<AdmissionError>([&] {
        queue.complete(first->header.identity, collected.outcome, collected.reply.data(), collected.reply.size());
    });
    auto latest = queue.begin();
    require(latest && latest->header.identity.request == 3, "Only latest queued archive launches");
    collected = runWorkerJob(worker, *latest);
    require(collected.diagnostic.empty() && !collected.diagnosticTruncated, "Latest queued helper emits no runtime diagnostic");
    auto admitted = queue.complete(latest->header.identity, collected.outcome, collected.reply.data(), collected.reply.size());
    require(!std::get<MeshPayload>(admitted).vertices.empty(), "Latest actual helper result passes queue admission");
    require(!queue.begin(), "Actual sequential transport drains queue");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Expected worker and trusted fixture directory");
        const auto parentLimits = parentResourceLimits();
        preflight(argv[1]);
        resourcePreflight(argv[1]);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Capsule.ds3"), 1, 7);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Sword.ds3"), 2, 4096);
        verify(argv[1], { 1, 2, 3 }, 3, 1);
        verify(argv[1], {}, 4, 64);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Capsule.ds3"), 1, 7, true);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Sword.ds3"), 2, 4096, true);
        verify(argv[1], { 1, 2, 3 }, 3, 1, true);
        queuedTransport(argv[1], readFixture(std::filesystem::path(argv[2]) / "Capsule.ds3"));
        require(parentResourceLimits() == parentLimits, "Resource worker launches must not alter the parent process limits");
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

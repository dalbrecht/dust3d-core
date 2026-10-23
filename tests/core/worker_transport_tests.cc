// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerQueue.h>
#include <native/Engine/WorkerTransport.h>
#include <stdexcept>
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
    WorkerSupervision supervision;
    supervision.cancellation = &cancelled;
    try {
        runSupervisedWorkerJob(worker + "/missing-helper", input, {}, supervision);
        throw std::runtime_error("Cancelled work unexpectedly started");
    } catch (const WorkerStopped& cause) {
        require(cause.reason() == LifetimeReason::cancelled && !cause.collected(),
            "Pre-start cancellation returns a typed cause without any spawned child");
    }
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
void verify(const std::string& worker, const std::vector<uint8_t>& archive, uint64_t sequence, size_t chunk)
{
    auto input = request(archive, sequence);
    TransportLimits limits;
    limits.maximumIoChunkBytes = chunk;
    auto collected = sequence == 1
        ? runSupervisedWorkerJob(worker, input, limits, WorkerSupervision {})
        : runWorkerJob(worker, input, limits);
    require(collected.outcome.exitedNormally && collected.outcome.exitCode == 0, "Real helper must exit normally");
    if (!collected.diagnostic.empty() || collected.diagnosticTruncated) {
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
        preflight(argv[1]);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Capsule.ds3"), 1, 7);
        verify(argv[1], readFixture(std::filesystem::path(argv[2]) / "Sword.ds3"), 2, 4096);
        verify(argv[1], { 1, 2, 3 }, 3, 1);
        verify(argv[1], {}, 4, 64);
        queuedTransport(argv[1], readFixture(std::filesystem::path(argv[2]) / "Capsule.ds3"));
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

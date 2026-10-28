// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerClient
import Dust3DWorkerPreview
import Dust3DWorkerRuntime
import Dust3DWorkerXPC
import Foundation

private struct SessionXPCSnapshot: Sendable {
    let indices: [UInt32]
    let positions: [Float]
    let rebuilt: UInt64
    let reused: UInt64
    let retained: UInt64
}
private func copySessionXPCOutput(_ owner: OpaquePointer) throws -> SessionXPCSnapshot {
    guard dust3d_worker_session_output_summary(owner).outcome == DUST3D_WORKER_SESSION_SOURCED,
        let primary = dust3d_worker_session_output_primary(owner)
    else { throw StaticFixtureError.invalid("Expected ordinary sourced Capsule output") }
    let summary = dust3d_worker_decoded_summary(primary)
    let count = dust3d_worker_decoded_index_count(primary)
    guard count == 240, summary.vertex_count > 0, summary.vertex_count <= 240,
        dust3d_worker_decoded_source_summary(primary).corner_count == count
    else { throw StaticFixtureError.invalid("Capsule indexed/source correspondence") }
    var vertices = [Dust3DWorkerVertex](repeating: Dust3DWorkerVertex(), count: summary.vertex_count)
    var indices = [UInt32](repeating: 0, count: count)
    guard
        vertices.withUnsafeMutableBufferPointer({ dust3d_worker_decoded_vertices(primary, 0, $0.count, $0.baseAddress) }
        ) == 1,
        indices.withUnsafeMutableBufferPointer({ dust3d_worker_decoded_indices(primary, 0, $0.count, $0.baseAddress) })
            == 1
    else { throw StaticFixtureError.invalid("Owned indexed copies") }
    let positions = vertices.flatMap { vertex in
        withUnsafeBytes(of: vertex.position) { Array($0.bindMemory(to: Float.self)) }
    }
    guard positions.allSatisfy(\.isFinite), indices.allSatisfy({ $0 < summary.vertex_count }) else {
        throw StaticFixtureError.invalid("Finite indexed output")
    }
    var statistics = Dust3DWorkerSessionStatistics()
    guard dust3d_worker_session_output_statistics(owner, &statistics, MemoryLayout.size(ofValue: statistics)) == 1,
        statistics.maximum_cache_payload_bytes == 64 * 1024 * 1024,
        dust3d_worker_session_output_source_issue(owner) == nil,
        dust3d_worker_session_output_statistics_issue(owner) == nil
    else { throw StaticFixtureError.invalid("Actual session observations") }
    return SessionXPCSnapshot(
        indices: indices, positions: positions, rebuilt: statistics.rebuilt_components,
        reused: statistics.reused_components,
        retained: statistics.retained_payload_bytes)
}

/// Healthy licensed Capsule only. Failure/overlap/cancellation cases stay in fake phase tests.
@main
struct WorkerPreviewExecutorHost {
    static func main() async {
        do { try await run() } catch {
            // A failed QA assertion must report normally, never become a Swift top-level trap.
            FileHandle.standardError.write(Data("Preview executor fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
    private static func run() async throws {
        guard CommandLine.arguments.count >= 3 else {
            throw StaticFixtureError.invalid("Expected Capsule fixture and owned root")
        }
        let archive = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        let pins = try readDevelopmentPins()
        let configuration = try WorkerXPCConfiguration(
            serviceIdentifier: staticServiceIdentifier, signingRequirement: developmentRequirement(pins.service))
        let executor = try WorkerXPCPreviewExecutor<SessionXPCSnapshot>(
            configuration: configuration, registrationTimeoutMilliseconds: 5000, handshakeTimeoutMilliseconds: 5000,
            copy: copySessionXPCOutput)
        let pipeline = WorkerPreviewPipeline(executor: executor)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 64 * 1024 * 1024, archiveBytes: UInt64(archive.count),
            maximumIndexedBytes: 0, maximumSourceBytes: 0,
            maximumInputTransferBytes: UInt64(archive.count) + 24, inputChunkBytes: 64,
            maximumOutputTransferBytes: 64 * 1024 * 1024, outputChunkBytes: 256)
        let source = try WorkerPreviewSource(archive: archive, options: options)
        let document = UUID()
        func identity(_ request: UInt64) throws -> WorkerPreviewIdentity {
            try WorkerPreviewIdentity(document: document, request: request, revision: 7, generation: 9)
        }
        let first = try await pipeline.generate(identity: identity(1), source: source)
        let second = try await pipeline.generate(identity: identity(2), source: source)
        // Reuse is per component; other components may still rebuild in the same generation.
        guard first.rebuilt > 0, second.reused > 0,
            first.indices == second.indices, first.positions == second.positions,
            first.retained > 0, second.retained > 0
        else { throw StaticFixtureError.invalid("Actual owned preview reuse observations") }
        _ = try await executor.reset(request: 3)
        let third = try await pipeline.generate(identity: identity(4), source: source)
        guard third.rebuilt > 0, third.reused == 0,
            third.indices == first.indices, third.positions == first.positions
        else { throw StaticFixtureError.invalid("Actual owned preview rebuild after reset") }
        try await pipeline.retire()
        let retirement = try await executor.retire()
        guard retirement.invalidationRequested, let peer = retirement.peer,
            peer.observedProcess > 0, peer.observedProcess != getpid(), peer.sandboxEntitlementVerified
        else { throw StaticFixtureError.invalid("Independent peer retirement request observation") }
        let observed = try await executor.retireAndObserve(timeoutMilliseconds: 5000)
        guard observed.peer.instance == peer.instance,
            observed.peer.observedProcess == peer.observedProcess,
            observed.peer.connectionGeneration == peer.connectionGeneration
        else { throw StaticFixtureError.invalid("Correlated registered service exit event") }
        print("Registered service exit observed after fresh matching incarnation barrier")
        print("Actual public preview executor passed: servicePID=\(peer.observedProcess); hostPID=\(getpid())")
        print("Authenticated handshake before channel factory; actual writer, EOF decoder and control joined")
        print("Owned Capsule geometry copied; cache reused then rebuilt after reset; pipeline publication admitted")
        // The controller independently observes normal PID disappearance. This is only a request.
        print("Normal invalidation requested after owned phase joins")
    }
}

// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

func runProceduralXPCFixture(archive: Data, root: URL, requirement: String, deadline: DispatchTime) throws {
    var identity = Dust3DWorkerRequestIdentity()
    identity.document.0 = 1
    identity.request = 46
    identity.revision = 7
    let generation: UInt64 = 11
    let limits = Dust3DWorkerWireLimits(
        maximum_input_bytes: 1_048_576, maximum_output_bytes: 1_048_576, maximum_failure_bytes: 4096)
    var options = Dust3DWorkerRequestLegacyOptions()
    options.struct_size = MemoryLayout.size(ofValue: options)
    options.atlas_texture_size = 8
    options.color_size = 8
    options.normal_size = 8
    options.material_size = 8
    var encoded = Dust3DWorkerRequestResult()
    let request = archive.withUnsafeBytes {
        dust3d_worker_legacy_request_create(
            identity, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, &options, limits, &encoded)
    }
    guard let request, encoded.status == DUST3D_WORKER_REQUEST_OK else {
        throw StaticFixtureError.invalid("Procedural request encoding")
    }
    defer { dust3d_worker_request_destroy(request) }
    var size = 0
    guard let bytes = dust3d_worker_request_bytes(request, &size) else {
        throw StaticFixtureError.invalid("Owned procedural frame")
    }
    let queue = try WorkerServiceQueue(limits: limits)
    try queue.submit(Data(bytes: bytes, count: size))
    guard case .started(let pending) = try queue.begin(generation: generation) else {
        throw StaticFixtureError.invalid("Procedural queue begin")
    }
    let inputURL = root.appendingPathComponent("procedural-request")
    let outputURL = root.appendingPathComponent("procedural-reply")
    try pending.frame.write(to: inputURL)
    try Data().write(to: outputURL)
    let input = try FileHandle(forReadingFrom: inputURL)
    let output = try FileHandle(forWritingTo: outputURL)
    defer {
        try? input.close()
        try? output.close()
    }
    var copiedPixels = Data()
    var process: Int32 = 0
    try withWorkerXPCPeer(requirement: requirement, deadline: deadline) { peer in
        let outcome = WorkerOutcome<WorkerControlReceipt>()
        Task.detached {
            do {
                try await peer.submit(
                    input: input, output: output, isStream: false, request: 46, generation: generation, outcome: outcome
                )
            } catch { outcome.finish(.failure(error)) }
        }
        let control = try outcome.wait(until: deadline)
        guard control.status == 0, control.operation == 7, control.request == 46,
            control.admissionStatus == 0, control.finalizationError == 0, control.sandboxEntitlementVerified,
            control.systemError == 0, control.stage == 0, control.source == 0, control.code == 0,
            control.truncated == 0,
            control.process > 0, control.process != getpid(), control.process == control.observedProcess
        else { throw StaticFixtureError.invalid("Procedural receipt/actual peer") }
        process = control.observedProcess
        let reader = try FileHandle(forReadingFrom: outputURL)
        defer { try? reader.close() }
        try queue.withReplyCollector { collector in
            while let data = try reader.read(upToCount: 97), !data.isEmpty {
                let result = data.withUnsafeBytes {
                    dust3d_worker_reply_append(collector, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
                }
                guard result.status == DUST3D_WORKER_REPLY_READY else {
                    throw StaticFixtureError.invalid("Procedural reply copy")
                }
            }
            guard dust3d_worker_reply_finish(collector).status == DUST3D_WORKER_REPLY_FRAMED else {
                throw StaticFixtureError.invalid("Procedural actual EOF")
            }
        }
        let observed = Dust3DWorkerServiceObservation(
            connection_generation: generation, process_identifier: Int64(control.observedProcess))
        let receipt = Dust3DWorkerServiceReceipt(
            runtime_status: control.status, admission_status: control.admissionStatus,
            operation: control.operation, request_identity: control.request, process_identifier: Int64(control.process),
            system_error: control.systemError, io_stage: control.stage, detail_source: control.source,
            detail_code: control.code, message_truncated: control.truncated,
            finalization_error: control.finalizationError)
        copiedPixels = try queue.withDecodedReply(observed: observed, receipt: receipt, maximumDecodedBytes: 1_048_576)
        { decoded in
            let summary = dust3d_worker_decoded_summary(decoded)
            guard summary.kind == DUST3D_WORKER_DECODED_LEGACY_BAKED, summary.vertex_count == 240,
                summary.triangle_count == 80, summary.chart_count > 0,
                dust3d_worker_decoded_baked_summary(decoded).tangent_count == 80
            else { throw StaticFixtureError.invalid("Expected complete procedural baked rendering") }
            var view = Dust3DWorkerTextureView()
            guard dust3d_worker_decoded_baked_level_count(decoded, 0) == 1,
                dust3d_worker_decoded_baked_texture(decoded, 0, 0, &view) == 1,
                view.width == 8, view.height == 8, view.row_stride == 32, view.byte_count == 256,
                let pixels = view.pixels
            else { throw StaticFixtureError.invalid("Procedural 8px straight color map") }
            return Data(bytes: pixels, count: view.byte_count)
        }
        print("XPC service runtime sandbox entitlement verified")
        print("Service one-slot native admission accepted")
        print("Persistent service C admission accepted after actual EOF and OS peer observation")
        print("Queue-authoritative service completion admitted")
    }
    guard copiedPixels.count == 256, copiedPixels.contains(where: { $0 != 0 }) else {
        throw StaticFixtureError.invalid("Copied procedural map after retirement")
    }
    print("Isolated XPC procedural PNG bake passed: servicePID=\(process); hostPID=\(getpid())")
}

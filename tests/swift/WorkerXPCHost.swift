// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

private struct EncodedRequest: Sendable {
    let frame: Data
}
private func encodeRequest(archive: Data, operation: Dust3DWorkerOperation, request: UInt64) throws -> EncodedRequest {
    var identity = Dust3DWorkerRequestIdentity()
    withUnsafeMutableBytes(of: &identity.document) { $0[0] = 1 }
    identity.request = request
    identity.revision = 7
    let limits = Dust3DWorkerWireLimits(
        maximum_input_bytes: 1_048_576, maximum_output_bytes: 1_048_576, maximum_failure_bytes: 4096)
    var options = Dust3DWorkerRequestUvOptions(
        texture_size: 64, metadata_budget: 1_048_576, images: nil, image_count: 0)
    var receipt = Dust3DWorkerRequestResult()
    let created = archive.withUnsafeBytes { bytes in
        if operation == DUST3D_WORKER_OPERATION_UV_MESH {
            return dust3d_worker_request_create(
                operation, identity, bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count,
                &options, limits, &receipt)
        }
        return dust3d_worker_request_create(
            operation, identity, bytes.bindMemory(to: UInt8.self).baseAddress, bytes.count,
            nil, limits, &receipt)
    }
    guard let owner = created, receipt.status == DUST3D_WORKER_REQUEST_OK else {
        throw StaticFixtureError.invalid(
            "Shared request encoder rejected: source=\(receipt.detail_source.rawValue) code=\(receipt.detail_code)")
    }
    defer { dust3d_worker_request_destroy(owner) }
    var headerSize = 0
    var frameSize = 0
    guard dust3d_worker_request_header(owner, &headerSize) != nil, headerSize == 64,
        let frame = dust3d_worker_request_bytes(owner, &frameSize), frameSize > headerSize,
        frameSize <= 1_048_576 + headerSize
    else { throw StaticFixtureError.invalid("Owned request views rejected") }
    return EncodedRequest(frame: Data(bytes: frame, count: frameSize))
}

private struct CopiedReply: Sendable {
    let vertexCount: Int
    let triangleCount: Int
    let vertexScalars: [Float]
    let uvScalars: [Double]
}
private func framingFailure(_ reason: String, _ result: Dust3DWorkerReplyResult) -> StaticFixtureError {
    let cause = withUnsafeBytes(of: result.message) {
        String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self)
    }
    return .invalid(
        "\(reason): status=\(result.status.rawValue) source=\(result.detail_source.rawValue) code=\(result.detail_code) truncated=\(result.message_truncated) cause=\(cause)"
    )
}
private func prepareServiceRequest(frame: Data, generation: UInt64) throws -> (
    queue: WorkerServiceQueue, request: WorkerServiceRequest
) {
    let queue = try WorkerServiceQueue(
        limits: .init(
            maximum_input_bytes: 1_048_576, maximum_output_bytes: 1_048_576, maximum_failure_bytes: 4096))
    try queue.submit(frame)
    guard case .started(let request) = try queue.begin(generation: generation) else {
        throw StaticFixtureError.invalid("Fresh coordinator did not begin its captured request")
    }
    return (queue, request)
}
private func readReply(
    _ stream: FileHandle, queue: WorkerServiceQueue,
    isUv: Bool, completion: WorkerOutcome<WorkerControlReceipt>, deadline: DispatchTime
) throws -> CopiedReply {
    try queue.withReplyCollector { owner in
        while true {
            var count = 0
            let sized = dust3d_worker_reply_next_read(owner, 97, &count)
            guard sized.status == DUST3D_WORKER_REPLY_READY, count > 0, count <= 97
            else {
                throw framingFailure("Read sizing rejected count=\(count)", sized)
            }
            guard let data = try stream.read(upToCount: count), !data.isEmpty else { break }
            let receipt = data.withUnsafeBytes {
                dust3d_worker_reply_append(owner, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
            }
            guard receipt.status == DUST3D_WORKER_REPLY_READY else {
                throw framingFailure("Correlated descriptor frame rejected", receipt)
            }
        }
        let finished = dust3d_worker_reply_finish(owner)
        guard finished.status == DUST3D_WORKER_REPLY_FRAMED else {
            throw framingFailure("Actual channel EOF rejected", finished)
        }
    }
    let control = try completion.wait(until: deadline)
    guard control.sandboxEntitlementVerified, control.observedProcess != getpid() else {
        throw StaticFixtureError.invalid("Service sandbox receipt/independent OS peer rejected")
    }
    let observed = Dust3DWorkerServiceObservation(
        connection_generation: control.connectionGeneration, process_identifier: Int64(control.observedProcess))
    let receipt = Dust3DWorkerServiceReceipt(
        runtime_status: control.status, admission_status: control.admissionStatus,
        operation: control.operation, request_identity: control.request,
        process_identifier: Int64(control.process), system_error: control.systemError,
        io_stage: control.stage, detail_source: control.source, detail_code: control.code,
        message_truncated: control.truncated, finalization_error: control.finalizationError)
    return try queue.withDecodedReply(observed: observed, receipt: receipt, maximumDecodedBytes: 1_048_576) { decoded in
        print("Persistent service C admission accepted after actual EOF and OS peer observation")
        print("Queue-authoritative service completion admitted")
        return try copyDecodedReply(decoded, isUv: isUv)
    }
}

private func copyDecodedReply(_ decoded: OpaquePointer, isUv: Bool) throws -> CopiedReply {
    let summary = dust3d_worker_decoded_summary(decoded)
    guard summary.kind == (isUv ? DUST3D_WORKER_DECODED_UV_MESH : DUST3D_WORKER_DECODED_MESH),
        summary.vertex_count == 240
    else {
        throw StaticFixtureError.invalid("Ordinary Capsule vertex count differs")
    }
    var vertices = [Dust3DWorkerVertex](repeating: Dust3DWorkerVertex(), count: summary.vertex_count)
    guard
        vertices.withUnsafeMutableBufferPointer({
            dust3d_worker_decoded_vertices(decoded, 0, $0.count, $0.baseAddress)
        }) == 1
    else {
        throw StaticFixtureError.invalid("Independent vertex copy failed")
    }
    var copiedUv: [Double] = []
    if isUv {
        guard summary.triangle_count == 80, summary.triangle_count * 3 == summary.vertex_count else {
            throw StaticFixtureError.invalid("Paired UV count lost")
        }
        for index in 0..<summary.triangle_count {
            var triangle = Dust3DWorkerUvTriangle()
            guard dust3d_worker_decoded_triangle(decoded, index, &triangle) == 1,
                triangle.known <= 1, triangle.association <= 2,
                withUnsafeBytes(of: triangle.uv, { $0.bindMemory(to: Double.self).allSatisfy(\.isFinite) }),
                triangle.association == 1
                    ? triangle.component_index < summary.component_count : triangle.component_index == UInt64.max
            else { throw StaticFixtureError.invalid("Copied UV corner/association values lost") }
            copiedUv += withUnsafeBytes(of: triangle.uv) { Array($0.bindMemory(to: Double.self)) }
        }
    }
    let copiedVertices = vertices.flatMap { vertex in
        withUnsafeBytes(of: vertex.position) { Array($0.bindMemory(to: Float.self)) }
            + withUnsafeBytes(of: vertex.normal) { Array($0.bindMemory(to: Float.self)) }
            + withUnsafeBytes(of: vertex.color) { Array($0.bindMemory(to: Float.self)) }
    }
    return CopiedReply(
        vertexCount: summary.vertex_count, triangleCount: summary.triangle_count,
        vertexScalars: copiedVertices, uvScalars: copiedUv)
}

private func validateCopiedReply(_ value: CopiedReply, isUv: Bool) throws {
    guard value.vertexCount == 240, value.vertexScalars.count == 2400,
        value.vertexScalars.allSatisfy(\.isFinite),
        !isUv || (value.triangleCount == 80 && value.uvScalars.count == 480 && value.uvScalars.allSatisfy(\.isFinite))
    else { throw StaticFixtureError.invalid("Independent copied reply values differ") }
}
private func printResult(_ receipt: WorkerControlReceipt, isUv: Bool, isStream: Bool) {
    if isUv {
        print(
            "Isolated XPC ordinary UV mesh passed: 240 vertices; 80 UV triangles; servicePID=\(receipt.process); hostPID=\(getpid())"
        )
    } else {
        print("Isolated XPC ordinary mesh passed: 240 vertices; servicePID=\(receipt.process); hostPID=\(getpid())")
    }
    if isStream { print("Concurrent descriptor writer, actual EOF decoder and control receipt joined") }
}

@main
private struct WorkerXPCHost {
    static func main() {
        do { try run() } catch {
            FileHandle.standardError.write(Data("Isolated static XPC fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
    static func run() throws {
        guard CommandLine.arguments.count == 5 else {
            throw StaticFixtureError.invalid("Expected licensed fixture, owned scratch, base/UV mode and channel kind")
        }
        let mode = CommandLine.arguments[3]
        guard mode == "mesh" || mode == "uv" else { throw StaticFixtureError.invalid("Unknown fixture mode") }
        let channelKind = CommandLine.arguments[4]
        guard channelKind == "file" || channelKind == "socket" else {
            throw StaticFixtureError.invalid("Unknown channel kind")
        }
        let isStream = channelKind == "socket"
        let isUv = mode == "uv"
        let operation = isUv ? DUST3D_WORKER_OPERATION_UV_MESH : DUST3D_WORKER_OPERATION_MESH
        let request: UInt64 = isUv ? 43 : 42
        let pins = try readDevelopmentPins()
        let source = URL(fileURLWithPath: CommandLine.arguments[1])
        let attributes = try source.resourceValues(forKeys: [.fileSizeKey])
        guard let sourceSize = attributes.fileSize, sourceSize > 0, sourceSize < 1_048_576 else {
            throw StaticFixtureError.invalid("Ordinary licensed archive stays small")
        }
        let archive = try Data(contentsOf: source)
        let encoded = try encodeRequest(archive: archive, operation: operation, request: request)
        let generation: UInt64 = 1  // This fixture owns exactly one connection; no reconnect/reuse.
        let fileAttempt = isStream ? nil : try prepareServiceRequest(frame: encoded.frame, generation: generation)
        let root = URL(fileURLWithPath: CommandLine.arguments[2])
        let inputURL = root.appendingPathComponent("request")
        let outputURL = root.appendingPathComponent("reply")
        let inputChannel: WorkerSocketChannel?
        let outputChannel: WorkerSocketChannel?
        let input: FileHandle
        let output: FileHandle
        if isStream {
            let incoming = try makeWorkerSocketChannel()
            let outgoing = try makeWorkerSocketChannel()
            inputChannel = incoming
            outputChannel = outgoing
            input = incoming.reader
            output = outgoing.writer
        } else {
            guard let fileAttempt else { throw StaticFixtureError.invalid("Missing prepared file request") }
            try fileAttempt.request.frame.write(to: inputURL)
            try Data().write(to: outputURL)
            inputChannel = nil
            outputChannel = nil
            input = try FileHandle(forReadingFrom: inputURL)
            output = try FileHandle(forWritingTo: outputURL)
        }
        defer {
            try? input.close()
            try? output.close()
        }
        let deadline = DispatchTime.now() + 20
        let outcome = WorkerOutcome<WorkerControlReceipt>()
        let prepared = WorkerOutcome<WorkerServiceRequest>()
        let reading = WorkerOutcome<CopiedReply>()
        let writing = WorkerOutcome<Void>()
        if let incoming = inputChannel, let outgoing = outputChannel {
            DispatchQueue(label: "org.dust3d.core.fixture.reply-reader").async {
                do {
                    let copied = try withOwnedWorkerChannel(outgoing.reader) {
                        let attempt = try prepareServiceRequest(frame: encoded.frame, generation: generation)
                        prepared.finish(.success(attempt.request))
                        return try readReply(
                            outgoing.reader, queue: attempt.queue,
                            isUv: isUv, completion: outcome, deadline: deadline)
                    }
                    reading.finish(.success(copied))
                } catch {
                    prepared.finish(.failure(error))
                    reading.finish(.failure(error))
                }
            }
            DispatchQueue(label: "org.dust3d.core.fixture.request-writer").async {
                writing.finish(
                    Result {
                        let request = try prepared.wait(until: deadline)
                        try writeWorkerRequest(request.frame, to: incoming.writer)
                    })
            }
        }
        let submittedRequest: WorkerServiceRequest
        if isStream {
            submittedRequest = try prepared.wait(until: deadline)
        } else {
            guard let fileAttempt else { throw StaticFixtureError.invalid("Missing prepared file request") }
            submittedRequest = fileAttempt.request
        }
        let capturedRequest = submittedRequest.identity.request
        try withWorkerXPCPeer(requirement: try developmentRequirement(pins.service), deadline: deadline) { peer in
            Task.detached {
                do {
                    try await peer.submit(
                        input: input, output: output, isStream: isStream, request: capturedRequest,
                        generation: generation, outcome: outcome)
                } catch { outcome.finish(.failure(error)) }
            }
            let receipt = try outcome.wait(until: deadline)
            guard receipt.status == DUST3D_WORKER_JOB_COMPLETE.rawValue,
                receipt.operation == operation.rawValue, receipt.request == capturedRequest,
                receipt.systemError == 0, receipt.stage == 0, receipt.source == 0, receipt.code == 0,
                receipt.truncated == 0, receipt.sandboxEntitlementVerified, receipt.finalizationError == 0,
                receipt.admissionStatus == WorkerServiceAdmissionStatus.accepted.rawValue,
                receipt.process > 0, receipt.process != getpid(), receipt.process == receipt.observedProcess,
                fcntl(input.fileDescriptor, F_GETFD) >= 0, fcntl(output.fileDescriptor, F_GETFD) >= 0
            else {
                throw StaticFixtureError.invalid(
                    "Receipt/context/independent process/borrowed handles mismatch: \(receipt)")
            }
            print("XPC service runtime sandbox entitlement verified")
            print("Service one-slot native admission accepted")
            try input.close()
            try output.close()
            if outputChannel != nil {
                try writing.wait(until: deadline)
                let copied = try reading.wait(until: deadline)
                try validateCopiedReply(copied, isUv: isUv)
                printResult(receipt, isUv: isUv, isStream: true)
                return
            }
            guard let fileAttempt else { throw StaticFixtureError.invalid("Missing file service coordinator") }
            let stream = try FileHandle(forReadingFrom: outputURL)
            let copied = try withOwnedWorkerChannel(stream) {
                try readReply(
                    stream, queue: fileAttempt.queue,
                    isUv: isUv, completion: outcome, deadline: deadline)
            }
            try validateCopiedReply(copied, isUv: isUv)
            printResult(receipt, isUv: isUv, isStream: isStream)
        }
    }
}

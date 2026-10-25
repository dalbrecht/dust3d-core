// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

private struct ControlReceipt: Sendable {
    let status: UInt32
    let operation: UInt32
    let request: UInt64
    let systemError: Int32
    let stage: UInt32
    let source: UInt32
    let code: Int32
    let truncated: UInt8
    let process: Int32
    let sandboxEntitlementVerified: Bool
    let finalizationError: Int32
    let admissionStatus: UInt32
}
private final class WorkOutcome<Value: Sendable>: @unchecked Sendable {
    // NSXPC invokes callbacks on its private queue; all state crosses this lock.
    private let lock = NSLock()
    private let ready = DispatchSemaphore(value: 0)
    private var result: Result<Value, any Error>?
    func finish(_ result: Result<Value, any Error>) {
        lock.lock()
        guard self.result == nil else {
            lock.unlock()
            return
        }
        self.result = result
        lock.unlock()
        ready.signal()
    }
    func wait(until deadline: DispatchTime) throws -> Value {
        guard ready.wait(timeout: deadline) == .success else {
            throw StaticFixtureError.invalid("Ordinary XPC reply timed out; no process was terminated")
        }
        lock.lock()
        defer { lock.unlock() }
        guard let result else { throw StaticFixtureError.invalid("Missing XPC receipt") }
        return try result.get()
    }
}

private struct EncodedRequest: Sendable {
    let header: [UInt8]
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
    guard let header = dust3d_worker_request_header(owner, &headerSize), headerSize == 64,
        let frame = dust3d_worker_request_bytes(owner, &frameSize), frameSize > headerSize,
        frameSize <= 1_048_576 + headerSize
    else { throw StaticFixtureError.invalid("Owned request views rejected") }
    return EncodedRequest(
        header: Array(UnsafeBufferPointer(start: header, count: headerSize)),
        frame: Data(bytes: frame, count: frameSize))
}

private struct CopiedReply: Sendable {
    let vertexCount: Int
    let triangleCount: Int
    let vertexScalars: [Float]
    let uvScalars: [Double]
}
private func readReply(_ stream: FileHandle, header: [UInt8], isUv: Bool) throws -> CopiedReply {
    let limits = Dust3DWorkerWireLimits(
        maximum_input_bytes: 1_048_576, maximum_output_bytes: 1_048_576, maximum_failure_bytes: 4096)
    var framing = Dust3DWorkerReplyResult()
    let created = header.withUnsafeBufferPointer {
        dust3d_worker_reply_create($0.baseAddress, $0.count, limits, &framing)
    }
    guard let owner = created, framing.status == DUST3D_WORKER_REPLY_READY else {
        throw StaticFixtureError.invalid("Parent framing preflight failed")
    }
    defer { dust3d_worker_reply_destroy(owner) }
    while true {
        var count = 0
        guard dust3d_worker_reply_next_read(owner, 97, &count).status == DUST3D_WORKER_REPLY_READY, count > 0,
            count <= 97
        else {
            throw StaticFixtureError.invalid("Read sizing rejected")
        }
        guard let data = try stream.read(upToCount: count), !data.isEmpty else { break }
        let receipt = data.withUnsafeBytes {
            dust3d_worker_reply_append(owner, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
        }
        guard receipt.status == DUST3D_WORKER_REPLY_READY else {
            throw StaticFixtureError.invalid("Correlated descriptor frame rejected")
        }
    }
    guard dust3d_worker_reply_finish(owner).status == DUST3D_WORKER_REPLY_FRAMED else {
        throw StaticFixtureError.invalid("Actual channel EOF rejected")
    }
    var semantic = Dust3DWorkerDecodeResult()
    guard let decoded = dust3d_worker_reply_decode(owner, 1_048_576, &semantic),
        semantic.status == DUST3D_WORKER_DECODE_OK
    else {
        throw StaticFixtureError.invalid("Semantic mesh rejected")
    }
    defer { dust3d_worker_decoded_destroy(decoded) }
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
private func printResult(_ receipt: ControlReceipt, isUv: Bool, isStream: Bool) {
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
        let header = encoded.header
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
            try encoded.frame.write(to: inputURL)
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
        let reading = WorkOutcome<CopiedReply>()
        let writing = WorkOutcome<Void>()
        if let incoming = inputChannel, let outgoing = outputChannel {
            DispatchQueue(label: "org.dust3d.core.fixture.reply-reader").async {
                reading.finish(
                    Result {
                        try withOwnedWorkerChannel(outgoing.reader) {
                            try readReply(outgoing.reader, header: header, isUv: isUv)
                        }
                    })
            }
            DispatchQueue(label: "org.dust3d.core.fixture.request-writer").async {
                writing.finish(Result { try writeWorkerRequest(encoded.frame, to: incoming.writer) })
            }
        }
        let connection = NSXPCConnection(serviceName: staticServiceIdentifier)
        connection.setCodeSigningRequirement(try developmentRequirement(pins.service))
        connection.remoteObjectInterface = NSXPCInterface(with: StaticWorkerJob.self)
        let outcome = WorkOutcome<ControlReceipt>()
        connection.invalidationHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection invalidated")))
        }
        connection.interruptionHandler = {
            outcome.finish(.failure(StaticFixtureError.invalid("XPC connection interrupted")))
        }
        defer {
            let process = connection.processIdentifier
            connection.invalidate()
            print("XPC peer PID=\(process)")
        }
        connection.resume()
        guard
            let proxy = connection.remoteObjectProxyWithErrorHandler({ error in
                let original = error as NSError
                outcome.finish(
                    .failure(
                        StaticFixtureError.invalid(
                            "\(original.domain) \(original.code): \(original.localizedDescription)")))
            }) as? StaticWorkerJob
        else { throw StaticFixtureError.invalid("Missing typed XPC proxy") }
        proxy.serve(input, output: output, isStream: isStream) {
            status, operation, request, systemError, stage, source, code, truncated, process,
            sandboxEntitlementVerified, finalizationError, admissionStatus
            in
            outcome.finish(
                .success(
                    ControlReceipt(
                        status: status, operation: operation, request: request,
                        systemError: systemError, stage: stage, source: source, code: code, truncated: truncated,
                        process: process, sandboxEntitlementVerified: sandboxEntitlementVerified,
                        finalizationError: finalizationError, admissionStatus: admissionStatus)))
        }
        let receipt = try outcome.wait(until: deadline)
        guard receipt.status == DUST3D_WORKER_JOB_COMPLETE.rawValue,
            receipt.operation == operation.rawValue, receipt.request == request,
            receipt.systemError == 0, receipt.stage == 0, receipt.source == 0, receipt.code == 0,
            receipt.truncated == 0, receipt.sandboxEntitlementVerified, receipt.finalizationError == 0,
            receipt.admissionStatus == WorkerServiceAdmissionStatus.accepted.rawValue,
            receipt.process > 0, receipt.process != getpid(), receipt.process == connection.processIdentifier,
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
        let stream = try FileHandle(forReadingFrom: outputURL)
        let copied = try withOwnedWorkerChannel(stream) {
            try readReply(stream, header: header, isUv: isUv)
        }
        try validateCopiedReply(copied, isUv: isUv)
        printResult(receipt, isUv: isUv, isStream: isStream)
    }
}

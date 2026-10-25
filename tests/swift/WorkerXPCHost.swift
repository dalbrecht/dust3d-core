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
}
private final class ControlOutcome: @unchecked Sendable {
    // NSXPC invokes callbacks on its private queue; all state crosses this lock.
    private let lock = NSLock()
    private let ready = DispatchSemaphore(value: 0)
    private var result: Result<ControlReceipt, StaticFixtureError>?
    func finish(_ result: Result<ControlReceipt, StaticFixtureError>) {
        lock.lock()
        guard self.result == nil else {
            lock.unlock()
            return
        }
        self.result = result
        lock.unlock()
        ready.signal()
    }
    func wait() throws -> ControlReceipt {
        guard ready.wait(timeout: .now() + 20) == .success else {
            throw StaticFixtureError.invalid("Ordinary XPC reply timed out; no process was terminated")
        }
        lock.lock()
        defer { lock.unlock() }
        guard let result else { throw StaticFixtureError.invalid("Missing XPC receipt") }
        return try result.get()
    }
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
        guard CommandLine.arguments.count == 3 else {
            throw StaticFixtureError.invalid("Expected licensed fixture and owned scratch")
        }
        let pins = try readDevelopmentPins()
        let source = URL(fileURLWithPath: CommandLine.arguments[1])
        let attributes = try source.resourceValues(forKeys: [.fileSizeKey])
        guard let sourceSize = attributes.fileSize, sourceSize > 0, sourceSize < 1_048_576 else {
            throw StaticFixtureError.invalid("Ordinary licensed archive stays small")
        }
        let archive = try Data(contentsOf: source)
        var header = [UInt8](repeating: 0, count: 64)
        header.replaceSubrange(0..<8, with: "DS3WORK\0".utf8)
        header[9] = 1
        header[11] = 64
        header[17] = 1
        header[19] = 1
        header[31] = 42
        header[39] = 7
        header[40] = 1
        for byte in 0..<8 { header[63 - byte] = UInt8(truncatingIfNeeded: UInt64(archive.count) >> (byte * 8)) }
        let root = URL(fileURLWithPath: CommandLine.arguments[2])
        let inputURL = root.appendingPathComponent("request")
        let outputURL = root.appendingPathComponent("reply")
        try (Data(header) + archive).write(to: inputURL)
        try Data().write(to: outputURL)
        let input = try FileHandle(forReadingFrom: inputURL)
        let output = try FileHandle(forWritingTo: outputURL)
        defer {
            try? input.close()
            try? output.close()
        }
        let connection = NSXPCConnection(serviceName: staticServiceIdentifier)
        connection.setCodeSigningRequirement(try developmentRequirement(pins.service))
        connection.remoteObjectInterface = NSXPCInterface(with: StaticWorkerJob.self)
        let outcome = ControlOutcome()
        connection.invalidationHandler = { outcome.finish(.failure(.invalid("XPC connection invalidated"))) }
        connection.interruptionHandler = { outcome.finish(.failure(.invalid("XPC connection interrupted"))) }
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
                    .failure(.invalid("\(original.domain) \(original.code): \(original.localizedDescription)")))
            }) as? StaticWorkerJob
        else { throw StaticFixtureError.invalid("Missing typed XPC proxy") }
        proxy.serve(input, output: output) {
            status, operation, request, systemError, stage, source, code, truncated, process in
            outcome.finish(
                .success(
                    ControlReceipt(
                        status: status, operation: operation, request: request,
                        systemError: systemError, stage: stage, source: source, code: code, truncated: truncated,
                        process: process)))
        }
        let receipt = try outcome.wait()
        guard receipt.status == DUST3D_WORKER_JOB_COMPLETE.rawValue,
            receipt.operation == DUST3D_WORKER_OPERATION_MESH.rawValue, receipt.request == 42,
            receipt.systemError == 0, receipt.stage == 0, receipt.source == 0, receipt.code == 0,
            receipt.truncated == 0,
            receipt.process > 0, receipt.process != getpid(), receipt.process == connection.processIdentifier,
            fcntl(input.fileDescriptor, F_GETFD) >= 0, fcntl(output.fileDescriptor, F_GETFD) >= 0
        else {
            throw StaticFixtureError.invalid(
                "Receipt/context/independent process/borrowed handles mismatch: \(receipt)")
        }
        try input.close()
        try output.close()
        let stream = try FileHandle(forReadingFrom: outputURL)
        defer { try? stream.close() }
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
            throw StaticFixtureError.invalid("Actual file EOF rejected")
        }
        var semantic = Dust3DWorkerDecodeResult()
        guard let decoded = dust3d_worker_reply_decode(owner, 1_048_576, &semantic),
            semantic.status == DUST3D_WORKER_DECODE_OK
        else {
            throw StaticFixtureError.invalid("Semantic mesh rejected")
        }
        defer { dust3d_worker_decoded_destroy(decoded) }
        let summary = dust3d_worker_decoded_summary(decoded)
        guard summary.kind == DUST3D_WORKER_DECODED_MESH, summary.vertex_count == 240 else {
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
        print("Isolated XPC ordinary mesh passed: 240 vertices; servicePID=\(receipt.process); hostPID=\(getpid())")
    }
}

// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

private enum RuntimeSmokeError: Error { case invalid(String) }
private struct CopiedReceipt: Sendable, Equatable {
    let status: UInt32
    let operation: UInt32
    let request: UInt64
    let systemError: Int32
    let detailSource: UInt32
    let ioStage: UInt32
    let detailCode: Int32
    let truncated: Bool
    let message: Data
}
private func copyReceipt(_ value: Dust3DWorkerJobResult) -> CopiedReceipt {
    let message = withUnsafeBytes(of: value.message) {
        Data($0.prefix { $0 != 0 })
    }
    return CopiedReceipt(
        status: value.status.rawValue, operation: value.operation.rawValue, request: value.request_identity,
        systemError: value.system_error, detailSource: value.detail_source.rawValue,
        ioStage: value.io_stage.rawValue, detailCode: value.detail_code, truncated: value.message_truncated != 0,
        message: message)
}
private actor ReceiptReceiver {
    private var stored: CopiedReceipt?
    func store(_ receipt: CopiedReceipt) { stored = receipt }
    func snapshot() -> CopiedReceipt? { stored }
}
@main
private enum WorkerRuntimeSmoke {
    static func main() async {
        do { try await run() } catch {
            FileHandle.standardError.write(Data("Worker C importer failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
    static func run() async throws {
        guard CommandLine.arguments.count == 3,
            dust3d_worker_runtime_abi_version() == DUST3D_WORKER_RUNTIME_ABI_VERSION
        else { throw RuntimeSmokeError.invalid("Expected fixture, shared scratch and layout version one") }
        let fixture = URL(fileURLWithPath: CommandLine.arguments[1])
        let size = try fixture.resourceValues(forKeys: [.fileSizeKey]).fileSize
        guard let size, size > 0, size < 1_048_576 else { throw RuntimeSmokeError.invalid("Fixture stays small") }
        let archive = try Data(contentsOf: fixture)
        let directory = URL(fileURLWithPath: CommandLine.arguments[2]).appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: directory) }
        let inputURL = directory.appendingPathComponent("request")
        let outputURL = directory.appendingPathComponent("reply")
        var header = [UInt8](repeating: 0, count: 64)
        header.replaceSubrange(0..<8, with: "DS3WORK\0".utf8)
        header[9] = 1
        header[11] = 64
        header[17] = 1
        header[19] = 1
        header[31] = 42
        header[40] = 1
        for byte in 0..<8 { header[63 - byte] = UInt8(truncatingIfNeeded: UInt64(archive.count) >> (byte * 8)) }
        try (Data(header) + archive).write(to: inputURL)
        try Data().write(to: outputURL)
        let input = try FileHandle(forReadingFrom: inputURL)
        let output = try FileHandle(forWritingTo: outputURL)
        defer {
            try? input.close()
            try? output.close()
        }
        let inputFD = input.fileDescriptor
        let outputFD = output.fileDescriptor
        let receipt = await Task.detached { copyReceipt(dust3d_worker_serve(inputFD, outputFD)) }.value
        guard receipt.status == DUST3D_WORKER_JOB_COMPLETE.rawValue,
            receipt.operation == DUST3D_WORKER_OPERATION_MESH.rawValue, receipt.request == 42,
            receipt.systemError == 0, receipt.detailSource == DUST3D_WORKER_DETAIL_NONE.rawValue,
            receipt.ioStage == DUST3D_WORKER_IO_NONE.rawValue, receipt.detailCode == 0, !receipt.truncated,
            receipt.message.isEmpty, fcntl(inputFD, F_GETFD) >= 0, fcntl(outputFD, F_GETFD) >= 0
        else { throw RuntimeSmokeError.invalid("Original descriptor result or ownership changed") }
        try input.close()
        try output.close()
        let replySize = try outputURL.resourceValues(forKeys: [.fileSizeKey]).fileSize
        guard let replySize, replySize >= 104, replySize < 1_048_576 else {
            throw RuntimeSmokeError.invalid("Ordinary reply stays small")
        }
        let reply = [UInt8](try Data(contentsOf: outputURL))
        let vertexCount = reply[72..<80].reduce(UInt64(0)) { ($0 << 8) | UInt64($1) }
        guard reply[17] == 2, reply[19] == 1, reply[31] == 42, vertexCount == 240 else {
            throw RuntimeSmokeError.invalid("Independent response fixture lost base mesh or identity")
        }
        let receiver = ReceiptReceiver()
        await receiver.store(receipt)
        guard await receiver.snapshot() == receipt else {
            throw RuntimeSmokeError.invalid("Owned result changed across actors")
        }
        let invalid = copyReceipt(dust3d_worker_serve(-1, -1))
        guard invalid.status == DUST3D_WORKER_JOB_PROTOCOL_REJECTED.rawValue,
            invalid.operation == DUST3D_WORKER_OPERATION_UNKNOWN.rawValue, invalid.request == 0,
            invalid.detailSource == DUST3D_WORKER_DETAIL_FRAME.rawValue, !invalid.message.isEmpty
        else { throw RuntimeSmokeError.invalid("Original preflight failure lost across C importer") }
        print("Swift 6 worker C importer, borrowed channels and owned Sendable result passed")
    }
}

// SPDX-License-Identifier: MIT
import Darwin
import Foundation

private enum ResultRejection: Error { case operation, closing }
private func verifyResultComposition() throws {
    let value = try resolveWorkerChannelResults(operation: .success(7), closing: .success(()))
    guard value == 7 else { throw StaticFixtureError.invalid("Successful owned value lost") }
    do {
        let _: Int = try resolveWorkerChannelResults(
            operation: .failure(ResultRejection.operation), closing: .success(()))
        throw StaticFixtureError.invalid("Operation rejection lost")
    } catch ResultRejection.operation {}
    do {
        let _: Int = try resolveWorkerChannelResults(operation: .success(7), closing: .failure(ResultRejection.closing))
        throw StaticFixtureError.invalid("Close rejection lost")
    } catch ResultRejection.closing {}
    do {
        let _: Int = try resolveWorkerChannelResults(
            operation: .failure(ResultRejection.operation), closing: .failure(ResultRejection.closing))
        throw StaticFixtureError.invalid("Combined rejection lost")
    } catch let cause as WorkerChannelOperationFailure {
        guard case ResultRejection.operation = cause.operation,
            case ResultRejection.closing = cause.closing
        else {
            throw StaticFixtureError.invalid("Original operation/close causes changed")
        }
    }
}

@main
struct WorkerChannelSmoke {
    static func main() {
        do {
            try verifyResultComposition()
            let channel = try makeWorkerSocketChannel()
            for handle in [channel.reader, channel.writer] {
                let flags = fcntl(handle.fileDescriptor, F_GETFD)
                guard flags >= 0, flags & FD_CLOEXEC != 0 else {
                    throw StaticFixtureError.invalid("Channel must be close-on-exec")
                }
                var suppressed: Int32 = 0
                var size = socklen_t(MemoryLayout<Int32>.size)
                guard getsockopt(handle.fileDescriptor, SOL_SOCKET, SO_NOSIGPIPE, &suppressed, &size) == 0,
                    suppressed == 1
                else { throw StaticFixtureError.invalid("Per-socket suppression must be configured") }
            }
            try channel.reader.close()
            try channel.writer.close()
            print(
                "Owned directional channels have close-on-exec and per-socket signal suppression; no data or OS failure probe"
            )
        } catch {
            FileHandle.standardError.write(Data("Channel metadata fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}

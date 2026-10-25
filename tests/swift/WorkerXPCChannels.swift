// SPDX-License-Identifier: MIT
import Darwin
import Foundation

struct WorkerSocketChannel: Sendable {
    let reader: FileHandle
    let writer: FileHandle
}
private struct ChannelSetupFailure: Error {
    let primary: any Error
    let cleanupErrors: [Int32]
}
struct WorkerChannelOperationFailure: Error {
    let operation: any Error
    let closing: any Error
}

func makeWorkerSocketChannel() throws -> WorkerSocketChannel {
    var descriptors = [Int32](repeating: -1, count: 2)
    let created = descriptors.withUnsafeMutableBufferPointer { socketpair(AF_UNIX, SOCK_STREAM, 0, $0.baseAddress) }
    guard created == 0 else { throw StaticFixtureError.system("socketpair", errno) }
    do {
        for descriptor in descriptors {
            let flags = fcntl(descriptor, F_GETFD)
            guard flags >= 0, fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0 else {
                throw StaticFixtureError.system("channel close-on-exec", errno)
            }
            var enabled: Int32 = 1
            guard setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, socklen_t(MemoryLayout<Int32>.size)) == 0
            else {
                throw StaticFixtureError.system("channel per-socket signal suppression", errno)
            }
            var observed: Int32 = 0
            var size = socklen_t(MemoryLayout<Int32>.size)
            guard getsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &observed, &size) == 0 else {
                throw StaticFixtureError.system("inspect channel suppression", errno)
            }
            guard observed == 1, size == MemoryLayout<Int32>.size else {
                throw StaticFixtureError.invalid("Channel suppression was not installed")
            }
        }
        // These are orderly unused-direction closures, never failed-peer writes.
        guard shutdown(descriptors[0], SHUT_WR) == 0 else {
            throw StaticFixtureError.system("channel direction setup", errno)
        }
        return WorkerSocketChannel(
            reader: FileHandle(fileDescriptor: descriptors[0], closeOnDealloc: true),
            writer: FileHandle(fileDescriptor: descriptors[1], closeOnDealloc: true))
    } catch {
        var cleanup: [Int32] = []
        for descriptor in descriptors where Darwin.close(descriptor) != 0 { cleanup.append(errno) }
        if !cleanup.isEmpty { throw ChannelSetupFailure(primary: error, cleanupErrors: cleanup) }
        throw error
    }
}

func resolveWorkerChannelResults<Value>(operation: Result<Value, any Error>, closing: Result<Void, any Error>) throws
    -> Value
{
    switch closing {
    case .success:
        return try operation.get()
    case .failure(let closeCause):
        switch operation {
        case .success: throw closeCause
        case .failure(let operationCause):
            throw WorkerChannelOperationFailure(operation: operationCause, closing: closeCause)
        }
    }
}

func withOwnedWorkerChannel<Value>(_ handle: FileHandle, operation: () throws -> Value) throws -> Value {
    let result = Result { try operation() }
    let closing = Result { try handle.close() }
    return try resolveWorkerChannelResults(operation: result, closing: closing)
}

func writeWorkerRequest(_ frame: Data, to writer: FileHandle) throws {
    try withOwnedWorkerChannel(writer) {
        var offset = 0
        while offset < frame.count {
            let end = offset + min(4096, frame.count - offset)
            try writer.write(contentsOf: frame.subdata(in: offset..<end))
            offset = end
        }
    }
}

// SPDX-License-Identifier: MIT
import Darwin
import Foundation

/// Directional roles; close each handle only after its exclusive blocking operation returns.
public struct WorkerSocketChannel: Sendable {
    public let reader: FileHandle
    public let writer: FileHandle
}
/// Retain original setup cause and every cleanup errno.
public struct WorkerChannelSetupFailure: Error {
    public let primary: any Error
    public let cleanupErrors: [Int32]
}
/// Retain both original operation and owned-close failures.
public struct WorkerChannelOperationFailure: Error {
    public let operation: any Error
    public let closing: any Error
}

/// Create inspected close-on-exec/SO_NOSIGPIPE roles without transferring data or changing global signals.
/// - Returns: Owned directional handles.
/// - Throws: Typed setup/cleanup failures.
public func makeWorkerSocketChannel() throws -> WorkerSocketChannel {
    var descriptors = [Int32](repeating: -1, count: 2)
    let created = descriptors.withUnsafeMutableBufferPointer { socketpair(AF_UNIX, SOCK_STREAM, 0, $0.baseAddress) }
    guard created == 0 else { throw WorkerChannelSetupError.system("socketpair", errno) }
    do {
        for descriptor in descriptors {
            let flags = fcntl(descriptor, F_GETFD)
            guard flags >= 0, fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0 else {
                throw WorkerChannelSetupError.system("channel close-on-exec", errno)
            }
            var enabled: Int32 = 1
            guard setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &enabled, socklen_t(MemoryLayout<Int32>.size)) == 0
            else {
                throw WorkerChannelSetupError.system("channel per-socket signal suppression", errno)
            }
            var observed: Int32 = 0
            var size = socklen_t(MemoryLayout<Int32>.size)
            guard getsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &observed, &size) == 0 else {
                throw WorkerChannelSetupError.system("inspect channel suppression", errno)
            }
            guard observed == 1, size == MemoryLayout<Int32>.size else {
                throw WorkerChannelSetupError.invalid("Channel suppression was not installed")
            }
        }
        // These are orderly unused-direction closures, never failed-peer writes.
        guard shutdown(descriptors[0], SHUT_WR) == 0 else {
            throw WorkerChannelSetupError.system("channel direction setup", errno)
        }
        return WorkerSocketChannel(
            reader: FileHandle(fileDescriptor: descriptors[0], closeOnDealloc: true),
            writer: FileHandle(fileDescriptor: descriptors[1], closeOnDealloc: true))
    } catch {
        var cleanup: [Int32] = []
        for descriptor in descriptors where Darwin.close(descriptor) != 0 { cleanup.append(errno) }
        if !cleanup.isEmpty { throw WorkerChannelSetupFailure(primary: error, cleanupErrors: cleanup) }
        throw error
    }
}

/// Return the original value only when operation and close succeed; otherwise throw original/composed causes.
public func resolveWorkerChannelResults<Value>(operation: Result<Value, any Error>, closing: Result<Void, any Error>)
    throws
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

/// Close an exclusive synchronous handle only after operation returns, preserving both original causes.
public func withOwnedWorkerChannel<Value>(_ handle: FileHandle, operation: () throws -> Value) throws -> Value {
    let result = Result { try operation() }
    let closing = Result { try handle.close() }
    return try resolveWorkerChannelResults(operation: result, closing: closing)
}

/// Write bounded chunks and close the owned writer; run on a dedicated non-UI worker.
public func writeWorkerRequest(_ frame: Data, to writer: FileHandle) throws {
    try withOwnedWorkerChannel(writer) {
        var offset = 0
        while offset < frame.count {
            let end = offset + min(4096, frame.count - offset)
            try writer.write(contentsOf: frame.subdata(in: offset..<end))
            offset = end
        }
    }
}

/// Original per-descriptor classification and errno.
public enum WorkerChannelSetupError: Error {
    case system(String, Int32)
    case invalid(String)
}

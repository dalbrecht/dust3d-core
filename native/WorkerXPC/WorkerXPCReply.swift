// SPDX-License-Identifier: MIT
import Foundation

/// The lock protects only the continuation; resume outside it to avoid reentrant lock work.
/// Foundation may race error/reply callbacks, so no callback may resume a continuation twice.
final class WorkerXPCReply<Value: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Value, any Error>?
    init(_ continuation: CheckedContinuation<Value, any Error>) { self.continuation = continuation }
    func finish(_ result: Result<Value, any Error>) {
        let pending = lock.withLock {
            let pending = continuation
            continuation = nil
            return pending
        }
        pending?.resume(with: result)
    }
}

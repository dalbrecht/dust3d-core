// SPDX-License-Identifier: MIT
import Foundation

enum WorkerOutcomeError: LocalizedError {
    case deadlineExceeded, missingResult
    var errorDescription: String? {
        switch self {
        case .deadlineExceeded: "Ordinary fixture outcome deadline elapsed; work was not retired or restarted."
        case .missingResult: "Completed fixture event has no retained result."
        }
    }
}
/// One retained result, with a monotonic deadline for each observer and no consuming wake.
final class WorkerOutcome<Value: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private let ready = DispatchGroup()
    private var result: Result<Value, any Error>?
    init() { ready.enter() }
    @discardableResult
    func finish(_ value: Result<Value, any Error>) -> Bool {
        lock.lock()
        guard result == nil else {
            lock.unlock()
            return false
        }
        result = value
        lock.unlock()
        ready.leave()
        return true
    }
    func wait(until deadline: DispatchTime) throws -> Value {
        guard ready.wait(timeout: deadline) == .success else { throw WorkerOutcomeError.deadlineExceeded }
        return try lock.withLock {
            guard let result else { throw WorkerOutcomeError.missingResult }
            return try result.get()
        }
    }
}

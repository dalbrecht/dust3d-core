// SPDX-License-Identifier: MIT
import Darwin
import Foundation

private enum OutcomeCheckError: Error { case invalid(String) }
private func require(_ condition: Bool, _ message: String) throws {
    if !condition { throw OutcomeCheckError.invalid(message) }
}
private final class RecordedOutcomeValues: @unchecked Sendable {
    private let lock = NSLock()
    private var stored: [Result<Int, any Error>] = []
    func append(_ value: Result<Int, any Error>) { lock.withLock { stored.append(value) } }
    func values() -> [Result<Int, any Error>] { lock.withLock { stored } }
}
@main
struct WorkerOutcomeSmoke {
    static func main() {
        do {
            let ready = WorkerOutcome<Int>()
            try require(ready.finish(.success(42)), "First finish must win")
            try require(!ready.finish(.success(99)), "Duplicate finish cannot replace original")
            try require(try ready.wait(until: .now()) == 42, "Completed value readable immediately")
            try require(try ready.wait(until: .now()) == 42, "Read does not consume completion")
            let delayed = WorkerOutcome<Int>()
            do {
                _ = try delayed.wait(until: .now())
                throw OutcomeCheckError.invalid("Unfinished outcome did not time out")
            } catch WorkerOutcomeError.deadlineExceeded {}
            try require(delayed.finish(.success(7)), "Observation timeout never retires work")
            try require(try delayed.wait(until: .now()) == 7, "Late completion remains readable")
            let failure = WorkerOutcome<Int>()
            let original = NSError(
                domain: "worker.fixture", code: 17, userInfo: [NSLocalizedDescriptionKey: "original"])
            _ = failure.finish(.failure(original))
            for _ in 0..<2 {
                do {
                    _ = try failure.wait(until: .now())
                    throw OutcomeCheckError.invalid("Original failure lost")
                } catch let cause as NSError {
                    try require(cause === original, "Every reader retains original cause")
                }
            }
            let shared = WorkerOutcome<Int>()
            let done = DispatchGroup()
            let values = RecordedOutcomeValues()
            let deadline = DispatchTime.now() + 2
            for _ in 0..<2 {
                done.enter()
                DispatchQueue.global().async {
                    defer { done.leave() }
                    values.append(Result { try shared.wait(until: deadline) })
                }
            }
            _ = shared.finish(.success(12))
            try require(done.wait(timeout: deadline) == .success, "Both readers must wake")
            let recorded = try values.values().map { try $0.get() }
            try require(recorded == [12, 12], "Both readers see one retained result")
            print("Pure retained outcome and two-reader synchronization passed; no native work")
        } catch {
            FileHandle.standardError.write(Data("Outcome checks failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}

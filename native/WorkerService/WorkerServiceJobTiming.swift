// SPDX-License-Identifier: MIT
import Foundation

protocol WorkerServiceJobTimerCancellation: Sendable { func cancel() }
struct WorkerServiceJobTimeout: Sendable { let fire: @Sendable () -> Void }
struct WorkerServiceJobTiming: Sendable {
    // Both factories must be nonblocking; timer must never invoke the callback inline.
    let clock: @Sendable () -> UInt64
    let timer: @Sendable (UInt64, WorkerServiceJobTimeout) -> any WorkerServiceJobTimerCancellation
    static func system() -> Self {
        let queue = DispatchQueue(label: "org.dust3d.worker.job-deadline", qos: .utility)
        return Self(
            clock: { DispatchTime.now().uptimeNanoseconds },
            timer: { milliseconds, timeout in
                let source = DispatchSource.makeTimerSource(queue: queue)
                // Validated budgets and checked remainders fit Int on the supported 64-bit macOS target.
                source.schedule(deadline: .now() + .milliseconds(Int(milliseconds)))
                source.setEventHandler(handler: timeout.fire)
                source.resume()
                return WorkerServiceJobDispatchTimer(source)
            })
    }
}
private final class WorkerServiceJobDispatchTimer: WorkerServiceJobTimerCancellation, @unchecked Sendable {
    private let source: any DispatchSourceTimer
    init(_ source: any DispatchSourceTimer) { self.source = source }
    func cancel() { source.cancel() }
    deinit { source.cancel() }
}

/// Original service-side deadline failure, without document contents or a false native-cleanup claim.
public struct WorkerServiceJobFailure: Error, Sendable {
    public let ownerGeneration: UInt64
    public let requestIdentity: UInt64
    public let leaseGeneration: UInt64
    public let budgetMilliseconds: UInt64
    public let cause: WorkerServiceJobDeadlineError
}

// SPDX-License-Identifier: MIT
import Foundation

enum WorkerXPCObservationEvent: Sendable {
    case registered(UInt64)
    case exited(UInt64)
    case unexpected(UInt)
    case timer(WorkerXPCExitObservationPhase, UInt64)
    enum Slot: Hashable { case registration, exit, unexpected, registrationTimer, exitTimer }
    var slot: Slot {
        switch self {
        case .registered: return .registration
        case .exited: return .exit
        case .unexpected: return .unexpected
        case .timer(.registration, _): return .registrationTimer
        case .timer(.exit, _): return .exitTimer
        }
    }
}

/// Every mutable value uses this lock. Capture and enqueue happen synchronously before scheduling
/// the sole actor drain. At most one pending value per five roles bounds storage under duplicates.
final class WorkerXPCObservationInbox: @unchecked Sendable {
    private let lock = NSLock()
    private var values: [WorkerXPCObservationEvent] = []
    private var slots: Set<WorkerXPCObservationEvent.Slot> = []
    private var isScheduled = false
    private var isClosed = false
    func record(_ makeEvent: () -> WorkerXPCObservationEvent) -> Bool {
        lock.withLock {
            guard !isClosed else { return false }
            let event = makeEvent()
            guard slots.insert(event.slot).inserted else { return false }
            values.append(event)
            guard !isScheduled else { return false }
            isScheduled = true
            return true
        }
    }
    func take() -> WorkerXPCObservationEvent? {
        lock.withLock {
            guard !values.isEmpty else {
                isScheduled = false
                return nil
            }
            let event = values.removeFirst()
            slots.remove(event.slot)
            return event
        }
    }
    func close() {
        lock.withLock {
            isClosed = true
            isScheduled = false
            values.removeAll()
            slots.removeAll()
        }
    }
}

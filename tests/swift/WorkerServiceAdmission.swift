// SPDX-License-Identifier: MIT
// Pure immutable values, independent of Foundation, transport and the native engine.
struct WorkerServiceLease: Equatable, Sendable {
    let generation: UInt64
    fileprivate init(generation: UInt64) { self.generation = generation }
}
enum WorkerServiceAdmissionStatus: UInt32, Sendable {
    case accepted = 0
    case busy = 1
    case closed = 2
    case staleCompletion = 3
    case notOwner = 4
}
enum WorkerServiceAdmissionDecision: Equatable, Sendable {
    case accepted(WorkerServiceLease)
    case busy, closed, notOwner
    var status: WorkerServiceAdmissionStatus {
        switch self {
        case .accepted: .accepted
        case .busy: .busy
        case .closed: .closed
        case .notOwner: .notOwner
        }
    }
}
struct WorkerServiceAdmissionState: Equatable, Sendable {
    let lastIssuedGeneration: UInt64
    let activeLease: WorkerServiceLease?
    let isRetired: Bool
    init(lastIssuedGeneration: UInt64 = 0) {
        self.init(lastIssuedGeneration: lastIssuedGeneration, activeLease: nil, isRetired: false)
    }
    private init(lastIssuedGeneration: UInt64, activeLease: WorkerServiceLease?, isRetired: Bool) {
        self.lastIssuedGeneration = lastIssuedGeneration
        self.activeLease = activeLease
        self.isRetired = isRetired
    }
    func acquiring() -> (state: Self, decision: WorkerServiceAdmissionDecision) {
        if isRetired { return (self, .closed) }
        if activeLease != nil { return (self, .busy) }
        guard lastIssuedGeneration < UInt64.max else { return (retired(), .closed) }
        let lease = WorkerServiceLease(generation: lastIssuedGeneration + 1)
        return (Self(lastIssuedGeneration: lease.generation, activeLease: lease, isRetired: false), .accepted(lease))
    }
    func completing(_ lease: WorkerServiceLease) -> (state: Self, didComplete: Bool) {
        guard activeLease == lease else { return (self, false) }
        return (Self(lastIssuedGeneration: lastIssuedGeneration, activeLease: nil, isRetired: isRetired), true)
    }
    func retired() -> Self {
        Self(lastIssuedGeneration: lastIssuedGeneration, activeLease: activeLease, isRetired: true)
    }
}

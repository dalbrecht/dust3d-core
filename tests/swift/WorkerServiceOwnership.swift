// SPDX-License-Identifier: MIT
// Routing candidates are internal values, never authentication credentials.
struct WorkerServiceConnection: Equatable, Sendable {
    let generation: UInt64
    fileprivate init(generation: UInt64) { self.generation = generation }
}
struct WorkerServiceOwnershipState: Equatable, Sendable {
    let lastIssuedGeneration: UInt64
    let owner: WorkerServiceConnection?
    let isRetired: Bool
    init(lastIssuedGeneration: UInt64 = 0) {
        self.init(lastIssuedGeneration: lastIssuedGeneration, owner: nil, isRetired: false)
    }
    private init(lastIssuedGeneration: UInt64, owner: WorkerServiceConnection?, isRetired: Bool) {
        self.lastIssuedGeneration = lastIssuedGeneration
        self.owner = owner
        self.isRetired = isRetired
    }
    func issuingCandidate() -> (state: Self, candidate: WorkerServiceConnection?) {
        guard !isRetired, lastIssuedGeneration < UInt64.max else { return (self, nil) }
        let candidate = WorkerServiceConnection(generation: lastIssuedGeneration + 1)
        return (Self(lastIssuedGeneration: candidate.generation, owner: owner, isRetired: false), candidate)
    }
    func claiming(_ candidate: WorkerServiceConnection) -> (state: Self, didClaim: Bool) {
        guard !isRetired, candidate.generation <= lastIssuedGeneration else { return (self, false) }
        if let owner { return (self, owner == candidate) }
        return (Self(lastIssuedGeneration: lastIssuedGeneration, owner: candidate, isRetired: false), true)
    }
    func owns(_ candidate: WorkerServiceConnection) -> Bool { owner == candidate }
    func retiring(_ candidate: WorkerServiceConnection) -> (state: Self, didRetire: Bool) {
        guard !isRetired, owns(candidate) else { return (self, false) }
        return (Self(lastIssuedGeneration: lastIssuedGeneration, owner: owner, isRetired: true), true)
    }
}

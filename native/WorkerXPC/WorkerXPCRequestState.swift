// SPDX-License-Identifier: MIT

/// Actor-local RPC correlation only. Retirement does not establish native/process completion;
/// the owning transport still drains independently. Fake tests exercise state without IPC.
struct WorkerXPCRequestState {
    private var lastToken: UInt64
    private var boundGeneration: UInt64?
    private var boundPeer: Int32?
    private(set) var activeToken: UInt64?
    private(set) var isRetired = false
    init(lastToken: UInt64 = 0) { self.lastToken = lastToken }
    mutating func begin(generation: UInt64) throws -> UInt64 {
        guard !isRetired else { throw WorkerXPCTransportError.retired }
        guard activeToken == nil else { throw WorkerXPCTransportError.busy }
        guard generation > 0 else { throw WorkerXPCTransportError.invalidGeneration }
        if let boundGeneration, boundGeneration != generation { throw WorkerXPCTransportError.generationMismatch }
        guard lastToken < UInt64.max else { throw WorkerXPCTransportError.sequenceExhausted }
        lastToken += 1
        boundGeneration = generation
        activeToken = lastToken
        return lastToken
    }
    /// Called only for the matching active callback using an independent connection PID.
    /// A transport failure must retire the actor before another process can reuse its generation.
    mutating func observePeer(_ process: Int32) throws {
        guard process > 0 else { throw WorkerXPCTransportError.missingPeer }
        if let boundPeer, boundPeer != process {
            throw WorkerXPCTransportError.peerChanged(expected: boundPeer, observed: process)
        }
        boundPeer = process
    }
    mutating func finish(_ token: UInt64) -> Bool {
        guard activeToken == token else { return false }
        activeToken = nil
        return true
    }
    mutating func close() throws {
        guard activeToken == nil else { throw WorkerXPCTransportError.busy }
        isRetired = true
    }
    @discardableResult
    mutating func retire() -> UInt64? {
        let token = activeToken
        activeToken = nil
        isRetired = true
        return token
    }
}

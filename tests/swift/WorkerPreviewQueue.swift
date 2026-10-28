// SPDX-License-Identifier: MIT
import Foundation

enum WorkerPreviewQueueError: Error { case closed, foreignScope, replayed, unexpectedCompletion }
/// Collecting-thread metadata owner: one active transport and the newest waiting identity.
/// Caller retains immutable archive/context/policies for these identities, drains transport
/// before complete/abandon and authenticates peer observations. No frames or archive copies,
/// I/O, native interruption, reconnect or application publication occur here.
final class WorkerPreviewPublicationQueue {
    private var boundScope: (document: UUID, generation: UInt64)?
    private var lastRequest: UInt64 = 0
    private var active: WorkerPreviewPublicationGate?
    private var current: WorkerPreviewIdentity?
    private(set) var waitingIdentity: WorkerPreviewIdentity?
    private(set) var isClosed = false
    var activeIdentity: WorkerPreviewIdentity? { active?.identity }
    /// Validate scope and monotonic IDs before replacing waiting work. Returns displaced identity.
    @discardableResult
    func submit(_ identity: WorkerPreviewIdentity) throws -> WorkerPreviewIdentity? {
        guard !isClosed else { throw WorkerPreviewQueueError.closed }
        if let boundScope {
            guard boundScope.document == identity.document, boundScope.generation == identity.generation else {
                throw WorkerPreviewQueueError.foreignScope
            }
        }
        guard identity.request > lastRequest else { throw WorkerPreviewQueueError.replayed }
        let replaced = waitingIdentity
        boundScope = (identity.document, identity.generation)
        lastRequest = identity.request
        current = identity
        waitingIdentity = identity
        return replaced
    }
    /// Freeze one active identity without starting I/O. New waiting work does not replace active transport.
    func begin() -> WorkerPreviewIdentity? {
        guard active == nil, let waitingIdentity else { return nil }
        active = WorkerPreviewPublicationGate(identity: waitingIdentity)
        self.waitingIdentity = nil
        return waitingIdentity
    }
    /// Retire publication/drop matching waiting work only; accepted native work still drains.
    func cancel(_ identity: WorkerPreviewIdentity) {
        if current == identity { current = nil }
        if waitingIdentity == identity { waitingIdentity = nil }
        if active?.identity == identity { active?.retire() }
    }
    /// Reject future submissions and publication; preserve the active transport slot for cleanup.
    func close() {
        isClosed = true
        current = nil
        waitingIdentity = nil
        active?.retire()
    }
    /// Called after owned channel cleanup. Foreign completions retain active/waiting work;
    /// matching completions release only active before original admission checks can throw.
    func complete(_ identity: WorkerPreviewIdentity, receipt: WorkerPreviewControlReceipt, reachedEOF: Bool) throws {
        guard let active, active.identity == identity else { throw WorkerPreviewQueueError.unexpectedCompletion }
        self.active = nil
        guard let current else {
            active.retire()
            throw WorkerPreviewAdmissionError.retired
        }
        try active.admit(current: current, receipt: receipt, reachedEOF: reachedEOF)
    }
    /// Release matching failed transport after cleanup without hiding its caller-retained original error.
    func abandon(_ identity: WorkerPreviewIdentity) throws {
        guard active?.identity == identity else { throw WorkerPreviewQueueError.unexpectedCompletion }
        active = nil
        if current == identity { current = nil }
    }
}

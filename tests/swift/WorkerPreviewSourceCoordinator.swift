// SPDX-License-Identifier: MIT

/// Wrong-lifetime source lookup is a caller error, never permission to reuse waiting/obsolete bytes.
enum WorkerPreviewSourceError: Error { case notActive }
/// Collecting-thread source owner. Source must be an immutable, independently owned Sendable
/// snapshot containing archive and captured policies, never borrowed native memory. The metadata
/// queue remains authoritative; at most its active and newest waiting sources are retained.
/// No archive encoding/copy, I/O, task launch or native cancellation occurs here.
final class WorkerPreviewSourceCoordinator<Source: Sendable> {
    private let queue = WorkerPreviewPublicationQueue()
    private var sources: [UInt64: Source] = [:]
    var heldSourceCount: Int { sources.count }
    var activeIdentity: WorkerPreviewIdentity? { queue.activeIdentity }
    var waitingIdentity: WorkerPreviewIdentity? { queue.waitingIdentity }
    var isClosed: Bool { queue.isClosed }
    /// Reject first; only accepted metadata can replace a caller-owned source snapshot.
    @discardableResult
    func submit(_ identity: WorkerPreviewIdentity, source: Source) throws -> WorkerPreviewIdentity? {
        let replaced = try queue.submit(identity)
        if let replaced { sources.removeValue(forKey: replaced.request) }
        sources[identity.request] = source
        return replaced
    }
    func begin() -> WorkerPreviewIdentity? { queue.begin() }
    /// Return only the active immutable snapshot; caller may retain it across isolated work.
    func source(for identity: WorkerPreviewIdentity) throws -> Source {
        guard queue.activeIdentity == identity, let source = sources[identity.request] else {
            throw WorkerPreviewSourceError.notActive
        }
        return source
    }
    func cancel(_ identity: WorkerPreviewIdentity) {
        queue.cancel(identity)
        releaseRetiredSources()
    }
    func close() {
        queue.close()
        releaseRetiredSources()
    }
    /// Caller has finished channel cleanup. Release matched active ownership even when admission throws.
    func complete(_ identity: WorkerPreviewIdentity, receipt: WorkerPreviewControlReceipt, reachedEOF: Bool) throws {
        defer { releaseRetiredSources() }
        try queue.complete(identity, receipt: receipt, reachedEOF: reachedEOF)
    }
    /// Caller preserves its primary transport error; this only releases matching drained source ownership.
    func abandon(_ identity: WorkerPreviewIdentity) throws {
        defer { releaseRetiredSources() }
        try queue.abandon(identity)
    }
    private func releaseRetiredSources() {
        let active = queue.activeIdentity?.request
        let waiting = queue.waitingIdentity?.request
        // Keys are globally increasing within the queue's bound document/lifetime.
        // Snapshot keys before removal; there are at most two source owners.
        for request in Array(sources.keys) where request != active && request != waiting {
            sources.removeValue(forKey: request)
        }
    }
}

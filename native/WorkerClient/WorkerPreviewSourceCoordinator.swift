// SPDX-License-Identifier: MIT

/// Wrong-lifetime source lookup is a caller error, never permission to reuse waiting/obsolete bytes.
public enum WorkerPreviewSourceError: Error { case notActive }
/// Collecting-thread source owner. Source must be an immutable, independently owned Sendable
/// snapshot containing archive and captured policies, never borrowed native memory. The metadata
/// queue remains authoritative; at most its active and newest waiting sources are retained.
/// No archive encoding/copy, I/O, task launch or native cancellation occurs here.
public final class WorkerPreviewSourceCoordinator<Source: Sendable> {
    /// Create an empty externally serialized owner; this does not launch a worker.
    public init() {}
    private let queue = WorkerPreviewPublicationQueue()
    private var sources: [UInt64: Source] = [:]
    var heldSourceCount: Int { sources.count }
    /// Active transport identity, retained until matching cleanup completes.
    public var activeIdentity: WorkerPreviewIdentity? { queue.activeIdentity }
    /// Newest accepted waiting identity, if any.
    public var waitingIdentity: WorkerPreviewIdentity? { queue.waitingIdentity }
    /// Whether future submissions and publication are permanently retired.
    public var isClosed: Bool { queue.isClosed }
    /// Reject first; only accepted metadata can replace a caller-owned source snapshot.
    /// - Parameters:
    ///   - identity: Host-selected document/request/revision/lifetime correlation.
    ///   - source: Independently owned immutable archive/control snapshot.
    /// - Returns: Displaced waiting identity, or nil.
    /// - Throws: Original WorkerPreviewQueueError; rejection preserves existing owners.
    @discardableResult
    public func submit(_ identity: WorkerPreviewIdentity, source: Source) throws -> WorkerPreviewIdentity? {
        let replaced = try queue.submit(identity)
        if let replaced { sources.removeValue(forKey: replaced.request) }
        sources[identity.request] = source
        return replaced
    }
    /// Freeze one waiting identity when no transport is active; never launch work.
    /// - Returns: Captured active identity, or nil when busy/empty.
    public func begin() -> WorkerPreviewIdentity? { queue.begin() }
    /// Return only the active immutable snapshot; caller may retain it across isolated work.
    /// - Parameter identity: Captured active identity, never reply-derived.
    /// - Returns: Retained Sendable snapshot.
    /// - Throws: WorkerPreviewSourceError.notActive for a foreign/wrong-lifetime lookup.
    public func source(for identity: WorkerPreviewIdentity) throws -> Source {
        guard queue.activeIdentity == identity, let source = sources[identity.request] else {
            throw WorkerPreviewSourceError.notActive
        }
        return source
    }
    /// Retire matching publication/remove waiting ownership; active work must still drain.
    /// - Parameter identity: Host identity to retire; unrelated work is preserved.
    public func cancel(_ identity: WorkerPreviewIdentity) {
        queue.cancel(identity)
        releaseRetiredSources()
    }
    /// Reject future submissions and publication; preserve active source until cleanup.
    public func close() {
        queue.close()
        releaseRetiredSources()
    }
    /// Caller has finished channel cleanup. Release matched active ownership even when admission throws.
    /// - Parameters:
    ///   - identity: Captured active identity, not reconstructed from the reply.
    ///   - receipt: Original control fields plus independently authenticated host observations.
    ///   - reachedEOF: Actual complete semantic/framing/descriptor EOF observation.
    /// - Throws: Original WorkerPreviewQueueError or WorkerPreviewAdmissionError; receipt causes are retained.
    public func complete(_ identity: WorkerPreviewIdentity, receipt: WorkerPreviewControlReceipt, reachedEOF: Bool)
        throws
    {
        defer { releaseRetiredSources() }
        try queue.complete(identity, receipt: receipt, reachedEOF: reachedEOF)
    }
    /// Caller preserves its primary transport error; this only releases matching drained source ownership.
    /// - Parameter identity: Captured identity whose owned transport cleanup finished.
    /// - Throws: WorkerPreviewQueueError.unexpectedCompletion; foreign work remains held.
    public func abandon(_ identity: WorkerPreviewIdentity) throws {
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

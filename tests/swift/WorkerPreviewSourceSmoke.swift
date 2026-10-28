// SPDX-License-Identifier: MIT
import Foundation

private enum FixtureError: Error { case expectation }
private func require(_ value: Bool) throws { if !value { throw FixtureError.expectation } }
private final class Source: Sendable {
    let text: String
    init(_ text: String) { self.text = text }
}
private actor Receiver {
    private var source: Source?
    func retain(_ source: Source) { self.source = source }
    func read() -> String? { source?.text }
    func clear() { source = nil }
}
private struct ValueSnapshot: Sendable {
    let archive: Data
    let indexedBudget: UInt64
    let sourceBudget: UInt64
}
private func receipt(_ identity: WorkerPreviewIdentity, status: UInt32 = 0, cause: Data = Data())
    -> WorkerPreviewControlReceipt
{
    WorkerPreviewControlReceipt(
        status: status, request: identity.request, systemError: 0, stage: 0,
        source: 0, code: 0, truncated: 0, process: 42, sandboxEntitlementVerified: true,
        finalizationError: 0, admissionStatus: 0, boundaryStatus: 0,
        connectionGeneration: identity.generation, observedProcess: 42, cause: cause)
}
@main
private enum PreviewSourceSmoke {
    static func main() async throws {
        let document = UUID()
        func identity(_ request: UInt64) throws -> WorkerPreviewIdentity {
            try WorkerPreviewIdentity(document: document, request: request, revision: request, generation: 9)
        }
        let owner = WorkerPreviewSourceCoordinator<Source>()
        let first = try identity(1)
        let second = try identity(2)
        let third = try identity(3)
        weak var activeSource: Source?
        weak var waitingSource: Source?
        do {
            let source = Source("first")
            activeSource = source
            try owner.submit(first, source: source)
        }
        try require(owner.begin() == first)
        do {
            let source = Source("second")
            waitingSource = source
            try owner.submit(second, source: source)
        }
        try require(activeSource != nil && waitingSource != nil && owner.heldSourceCount == 2)
        try require(owner.activeIdentity == first && owner.waitingIdentity == second && !owner.isClosed)
        for invalid in [
            first,
            try WorkerPreviewIdentity(document: UUID(), request: 3, revision: 3, generation: 9),
        ] {
            do {
                try owner.submit(invalid, source: Source("must not replace"))
                throw FixtureError.expectation
            } catch WorkerPreviewQueueError.replayed {} catch WorkerPreviewQueueError.foreignScope {}
            try require(activeSource != nil && waitingSource != nil && owner.waitingIdentity == second)
        }
        do {
            _ = try owner.source(for: second)
            throw FixtureError.expectation
        } catch WorkerPreviewSourceError.notActive {}
        do {
            try owner.complete(second, receipt: receipt(second), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.unexpectedCompletion {}
        do {
            try owner.abandon(second)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.unexpectedCompletion {}
        try require(owner.heldSourceCount == 2 && activeSource != nil && waitingSource != nil)
        try require(try owner.submit(third, source: Source("third")) == second)
        try require(waitingSource == nil && activeSource != nil && owner.heldSourceCount == 2)
        try require(try owner.source(for: first).text == "first")
        do {
            try owner.complete(first, receipt: receipt(first), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.stale {}
        try require(activeSource == nil && owner.heldSourceCount == 1)
        try require(owner.begin() == third)
        owner.close()
        try require(owner.heldSourceCount == 1)
        try owner.abandon(third)
        try require(owner.heldSourceCount == 0)
        do {
            try owner.submit(try identity(4), source: Source("closed"))
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.closed {}
        try require(owner.isClosed && owner.heldSourceCount == 0)

        let cancelled = WorkerPreviewSourceCoordinator<Source>()
        weak var cancelledActive: Source?
        weak var cancelledWaiting: Source?
        do {
            let source = Source("draining")
            cancelledActive = source
            try cancelled.submit(first, source: source)
        }
        try require(cancelled.begin() == first)
        do {
            let source = Source("waiting")
            cancelledWaiting = source
            try cancelled.submit(second, source: source)
        }
        cancelled.cancel(second)
        try require(cancelledWaiting == nil && cancelledActive != nil)
        cancelled.cancel(first)
        try require(cancelledActive != nil && cancelled.heldSourceCount == 1)
        try require(try cancelled.source(for: first).text == "draining")
        do {
            try cancelled.complete(first, receipt: receipt(first), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        try require(cancelledActive == nil && cancelled.heldSourceCount == 0)

        let failed = WorkerPreviewSourceCoordinator<Source>()
        weak var failedSource: Source?
        do {
            let source = Source("original error")
            failedSource = source
            try failed.submit(first, source: source)
        }
        try require(failed.begin() == first)
        do {
            try failed.complete(first, receipt: receipt(first, status: 4, cause: Data([255])), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.receipt(let original) {
            try require(original.status == 4 && original.cause == Data([255]))
        }
        try require(failedSource == nil && failed.heldSourceCount == 0)

        let detached = WorkerPreviewSourceCoordinator<Source>()
        let receiver = Receiver()
        weak var transferred: Source?
        do {
            let source = Source("isolated snapshot")
            transferred = source
            try detached.submit(first, source: source)
        }
        try require(detached.begin() == first)
        try await receiver.retain(detached.source(for: first))
        try detached.abandon(first)
        try require(detached.heldSourceCount == 0 && transferred != nil)
        try require(await receiver.read() == "isolated snapshot")
        await receiver.clear()
        try require(transferred == nil)

        let values = WorkerPreviewSourceCoordinator<ValueSnapshot>()
        var original = Data([1, 2, 3])
        try values.submit(first, source: ValueSnapshot(archive: original, indexedBudget: UInt64.max, sourceBudget: 0))
        original[0] = 9
        try require(values.begin() == first)
        let snapshot = try values.source(for: first)
        try require(
            snapshot.archive == Data([1, 2, 3]) && snapshot.indexedBudget == UInt64.max
                && snapshot.sourceBudget == 0)
        try values.complete(first, receipt: receipt(first), reachedEOF: true)
        try require(values.heldSourceCount == 0)
    }
}

// SPDX-License-Identifier: MIT
import Foundation

private enum FixtureError: Error { case expectation }
private func require(_ value: Bool) throws { if !value { throw FixtureError.expectation } }
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
private enum PreviewQueueSmoke {
    static func main() throws {
        let document = UUID()
        func identity(_ request: UInt64) throws -> WorkerPreviewIdentity {
            try WorkerPreviewIdentity(document: document, request: request, revision: request, generation: 9)
        }
        let first = try identity(1)
        let second = try identity(2)
        let third = try identity(3)
        let queue = WorkerPreviewPublicationQueue()
        try require(queue.begin() == nil)
        try require(try queue.submit(first) == nil)
        try require(queue.begin() == first)
        try require(try queue.submit(second) == nil)
        try require(try queue.submit(third) == second)
        try require(queue.begin() == nil)
        do {
            try queue.complete(first, receipt: receipt(first), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.stale {}
        try require(queue.waitingIdentity == third && queue.activeIdentity == nil)
        try require(queue.begin() == third)
        try queue.complete(third, receipt: receipt(third), reachedEOF: true)
        try require(queue.begin() == nil)
        let fourth = try identity(4)
        let fifth = try identity(5)
        try queue.submit(fourth)
        try require(queue.begin() == fourth)
        try queue.submit(fifth)
        do {
            try queue.complete(fifth, receipt: receipt(fifth), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.unexpectedCompletion {}
        do {
            try queue.abandon(fifth)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.unexpectedCompletion {}
        try require(queue.activeIdentity == fourth && queue.waitingIdentity == fifth)
        try queue.abandon(fourth)
        try require(queue.begin() == fifth)
        let failure = receipt(fifth, status: 4, cause: Data([255]))
        do {
            try queue.complete(fifth, receipt: failure, reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.receipt(let original) {
            try require(original.status == 4 && original.cause == Data([255]))
        }
        try require(queue.activeIdentity == nil)
        do {
            try queue.submit(fifth)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.replayed {}
        let sixth = try identity(6)
        let seventh = try identity(7)
        try queue.submit(sixth)
        try require(queue.begin() == sixth)
        try queue.submit(seventh)
        queue.cancel(sixth)
        try require(queue.activeIdentity == sixth && queue.waitingIdentity == seventh)
        do {
            try queue.complete(sixth, receipt: receipt(sixth), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        try require(queue.begin() == seventh)
        let eighth = try identity(8)
        try queue.submit(eighth)
        queue.cancel(eighth)
        try require(queue.waitingIdentity == nil)
        do {
            try queue.complete(seventh, receipt: receipt(seventh), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        let ninth = try identity(9)
        let tenth = try identity(10)
        try queue.submit(ninth)
        try require(queue.begin() == ninth)
        try queue.submit(tenth)
        queue.close()
        queue.close()
        try require(queue.isClosed && queue.waitingIdentity == nil && queue.activeIdentity == ninth)
        try require(queue.begin() == nil)
        do {
            try queue.submit(tenth)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.closed {}
        do {
            try queue.complete(ninth, receipt: receipt(ninth), reachedEOF: true)
            throw FixtureError.expectation
        } catch WorkerPreviewAdmissionError.retired {}
        try require(queue.begin() == nil)
        let scoped = WorkerPreviewPublicationQueue()
        try scoped.submit(first)
        for foreign in [
            try WorkerPreviewIdentity(document: UUID(), request: 2, revision: 0, generation: 9),
            try WorkerPreviewIdentity(document: document, request: 2, revision: 0, generation: 10),
        ] {
            do {
                try scoped.submit(foreign)
                throw FixtureError.expectation
            } catch WorkerPreviewQueueError.foreignScope {}
            try require(scoped.waitingIdentity == first)
        }
        try require(try scoped.submit(second) == first)
        scoped.cancel(first)
        try require(scoped.waitingIdentity == second)
        scoped.cancel(second)
        try require(scoped.begin() == nil)
        let maximum = try WorkerPreviewIdentity(document: document, request: UInt64.max, revision: 0, generation: 9)
        try scoped.submit(maximum)
        try require(scoped.begin() == maximum)
        try scoped.abandon(maximum)
        do {
            try scoped.submit(maximum)
            throw FixtureError.expectation
        } catch WorkerPreviewQueueError.replayed {}
    }
}

// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Foundation
import XCTest

private enum FakeExecutionError: Error { case original, timeout }

private actor FakePreviewExecutor: WorkerPreviewExecuting {
    typealias Source = String
    typealias Value = String
    private var calls: [UInt64: (String, Bool)] = [:]
    private var pending: [UInt64: CheckedContinuation<WorkerPreviewExecutionCompletion<String>, any Error>] = [:]
    private var starters: [UInt64: CheckedContinuation<Void, Never>] = [:]
    func execute(identity: WorkerPreviewIdentity, source: String) async throws -> WorkerPreviewExecutionCompletion<
        String
    > {
        calls[identity.request] = (source, Task.isCancelled)
        starters.removeValue(forKey: identity.request)?.resume()
        let value = try await withCheckedThrowingContinuation { pending[identity.request] = $0 }
        calls[identity.request] = (source, Task.isCancelled)
        return value
    }
    func waitForStart(_ request: UInt64) async {
        if calls[request] != nil { return }
        await withCheckedContinuation { starters[request] = $0 }
    }
    func fail(_ request: UInt64) {
        pending.removeValue(forKey: request)?.resume(throwing: FakeExecutionError.original)
    }
    func source(_ request: UInt64) -> String? { calls[request]?.0 }
    func wasCancelled(_ request: UInt64) -> Bool? { calls[request]?.1 }
    func finish(
        _ identity: WorkerPreviewIdentity, value: String, written: Bool = true, eof: Bool = true, status: UInt32 = 0,
        cause: Data = Data()
    ) {
        let receipt = WorkerPreviewControlReceipt(
            status: status, request: identity.request, systemError: 0, stage: 0, source: 0, code: 0, truncated: 0,
            process: 42, sandboxEntitlementVerified: true, finalizationError: 0, admissionStatus: 0,
            boundaryStatus: 0, connectionGeneration: identity.generation, observedProcess: 42, cause: cause)
        pending.removeValue(forKey: identity.request)?.resume(
            returning: WorkerPreviewExecutionCompletion(
                value: value, receipt: receipt, requestWritten: written, reachedEOF: eof))
    }
}

private actor StartGate {
    private var isOpen = false
    private var continuation: CheckedContinuation<Void, Never>?
    func wait() async {
        if isOpen { return }
        await withCheckedContinuation { continuation = $0 }
    }
    func open() {
        isOpen = true
        continuation?.resume()
        continuation = nil
    }
}

@MainActor
final class WorkerClientPipelineTests: XCTestCase {
    func testShouldReturnOwnedValueOnlyAfterCompleteObservations() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 0, generation: 9)
        let task = Task { try await pipeline.generate(identity: identity, source: "captured archive") }
        await backend.waitForStart(1)
        let source = await backend.source(1)
        XCTAssertEqual(source, "captured archive")
        await backend.finish(identity, value: "owned result")
        let value = try await task.value
        XCTAssertEqual(value, "owned result")
        try await pipeline.retire()
    }

    private func identity(_ request: UInt64, document: UUID) throws -> WorkerPreviewIdentity {
        try WorkerPreviewIdentity(document: document, request: request, revision: request, generation: 9)
    }
    private func waitUntil(_ condition: @MainActor () async -> Bool) async throws {
        let deadline = ContinuousClock.now.advanced(by: .seconds(3))
        while !(await condition()) {
            guard ContinuousClock.now < deadline else { throw FakeExecutionError.timeout }
            try await Task.sleep(for: .milliseconds(1))
        }
    }
    private func expectCancellation(_ task: Task<String, any Error>) async {
        do {
            _ = try await task.value
            XCTFail("Expected cancellation")
        } catch is CancellationError {} catch { XCTFail("Unexpected error: \(error)") }
    }
    func testShouldReplaceWaitingWorkAndRejectStaleActivePublication() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let second = try identity(2, document: document)
        let third = try identity(3, document: document)
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        let displaced = Task { try await pipeline.generate(identity: second, source: "replaced") }
        try await waitUntil { await pipeline.waitingIdentity == second }
        let latest = Task { try await pipeline.generate(identity: third, source: "latest") }
        try await waitUntil { await pipeline.waitingIdentity == third }
        await expectCancellation(displaced)
        let unstarted = await backend.source(2)
        XCTAssertNil(unstarted)
        await backend.finish(first, value: "stale")
        do {
            _ = try await active.value
            XCTFail("Stale result published")
        } catch WorkerPreviewAdmissionError.stale {}
        await backend.waitForStart(3)
        await backend.finish(third, value: "latest result")
        let value = try await latest.value
        XCTAssertEqual(value, "latest result")
        try await pipeline.retire()
    }
    func testShouldCancelActivePublicationWithoutCancellingExecutionOrReleasingSlot() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let next = try identity(2, document: document)
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        active.cancel()
        await pipeline.cancelPublication(for: first)
        let waiting = Task { try await pipeline.generate(identity: next, source: "waiting") }
        try await waitUntil { await pipeline.waitingIdentity == next }
        let held = await pipeline.activeIdentity
        let unstarted = await backend.source(2)
        XCTAssertEqual(held, first)
        XCTAssertNil(unstarted)
        await backend.finish(first, value: "cancelled")
        await expectCancellation(active)
        let cancelled = await backend.wasCancelled(1)
        XCTAssertEqual(cancelled, false)
        await backend.waitForStart(2)
        await backend.finish(next, value: "success")
        let value = try await waiting.value
        XCTAssertEqual(value, "success")
        try await pipeline.retire()
    }
    func testShouldCancelWaitingWithoutResurrectingOlderActiveSource() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let next = try identity(2, document: document)
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        let waiting = Task { try await pipeline.generate(identity: next, source: "waiting") }
        try await waitUntil { await pipeline.waitingIdentity == next }
        await pipeline.cancelPublication(for: next)
        await expectCancellation(waiting)
        await backend.finish(first, value: "obsolete")
        do {
            _ = try await active.value
            XCTFail("Old source resurrected")
        } catch WorkerPreviewAdmissionError.retired {}
        let unstarted = await backend.source(2)
        XCTAssertNil(unstarted)
        try await pipeline.retire()
    }
    func testShouldPreserveOriginalBackendErrorAndStartNewestWaitingWork() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let next = try identity(2, document: document)
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        let waiting = Task { try await pipeline.generate(identity: next, source: "waiting") }
        try await waitUntil { await pipeline.waitingIdentity == next }
        await backend.fail(1)
        do {
            _ = try await active.value
            XCTFail("Original failure hidden")
        } catch FakeExecutionError.original {}
        await backend.waitForStart(2)
        await backend.finish(next, value: "recovered")
        let value = try await waiting.value
        XCTAssertEqual(value, "recovered")
        try await pipeline.retire()
    }
    func testShouldRejectForeignScopeReplayAndForeignCancellationWithoutReplacingWaiting() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let next = try identity(2, document: document)
        let foreign = try identity(2, document: UUID())
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        let waiting = Task { try await pipeline.generate(identity: next, source: "waiting") }
        try await waitUntil { await pipeline.waitingIdentity == next }
        do {
            _ = try await pipeline.generate(identity: foreign, source: "foreign")
            XCTFail("Foreign accepted")
        } catch WorkerPreviewQueueError.foreignScope {}
        do {
            _ = try await pipeline.generate(identity: next, source: "replay")
            XCTFail("Replay accepted")
        } catch WorkerPreviewQueueError.replayed {}
        await pipeline.cancelPublication(for: foreign)
        let retained = await pipeline.waitingIdentity
        XCTAssertEqual(retained, next)
        await backend.fail(1)
        do {
            _ = try await active.value
            XCTFail("Original failure hidden")
        } catch FakeExecutionError.original {}
        await backend.waitForStart(2)
        let source = await backend.source(2)
        XCTAssertEqual(source, "waiting")
        await backend.finish(next, value: "preserved")
        _ = try await waiting.value
        try await pipeline.retire()
    }
    func testShouldRejectIncompleteWriteOrEOFAndReleaseActiveCapacity() async throws {
        for written in [false, true] {
            let backend = FakePreviewExecutor()
            let pipeline = WorkerPreviewPipeline(executor: backend)
            let document = UUID()
            let first = try identity(1, document: document)
            let active = Task { try await pipeline.generate(identity: first, source: "archive") }
            await backend.waitForStart(1)
            await backend.finish(first, value: "not admitted", written: written, eof: false)
            do {
                _ = try await active.value
                XCTFail("Incomplete observation admitted")
            } catch WorkerPreviewExecutionError.incompleteRequest(let receipt) {
                XCTAssertFalse(written)
                XCTAssertEqual(receipt.request, 1)
            } catch WorkerPreviewAdmissionError.observation { XCTAssertTrue(written) }
            let next = try identity(2, document: document)
            let recovered = Task { try await pipeline.generate(identity: next, source: "next") }
            await backend.waitForStart(2)
            await backend.finish(next, value: "recovered")
            _ = try await recovered.value
            try await pipeline.retire()
        }
    }
    func testShouldRetainOriginalReceiptFailureBytes() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let captured = try identity(1, document: UUID())
        let active = Task { try await pipeline.generate(identity: captured, source: "archive") }
        await backend.waitForStart(1)
        let original = Data([0x80, 0, 7])
        await backend.finish(captured, value: "rejected", status: 7, cause: original)
        do {
            _ = try await active.value
            XCTFail("Bad receipt admitted")
        } catch WorkerPreviewAdmissionError.receipt(let receipt) {
            XCTAssertEqual(receipt.status, 7)
            XCTAssertEqual(receipt.cause, original)
        }
        try await pipeline.retire()
    }
    func testShouldBoundRetirementWaiterAndPreserveActiveFailureAfterDrain() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let document = UUID()
        let first = try identity(1, document: document)
        let next = try identity(2, document: document)
        let active = Task { try await pipeline.generate(identity: first, source: "active") }
        await backend.waitForStart(1)
        let waiting = Task { try await pipeline.generate(identity: next, source: "waiting") }
        try await waitUntil { await pipeline.waitingIdentity == next }
        let retirement = Task { try await pipeline.retire() }
        try await waitUntil { await pipeline.isRetired }
        do {
            _ = try await waiting.value
            XCTFail("Waiting work survived close")
        } catch WorkerPreviewQueueError.closed {}
        do {
            try await pipeline.retire()
            XCTFail("Second waiter retained")
        } catch WorkerPreviewExecutionError.retirementPending {}
        do {
            _ = try await pipeline.generate(identity: next, source: "closed")
            XCTFail("Retired accepted work")
        } catch WorkerPreviewQueueError.closed {}
        let held = await pipeline.activeIdentity
        XCTAssertEqual(held, first)
        await backend.fail(1)
        do {
            _ = try await active.value
            XCTFail("Original error hidden")
        } catch FakeExecutionError.original {}
        try await retirement.value
        try await pipeline.retire()
    }

    func testShouldRejectAlreadyCancelledCallerBeforeAdmission() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let gate = StartGate()
        let captured = try identity(1, document: UUID())
        let cancelled = Task {
            await gate.wait()
            return try await pipeline.generate(identity: captured, source: "not submitted")
        }
        cancelled.cancel()
        await gate.open()
        await expectCancellation(cancelled)
        let unstarted = await backend.source(1)
        XCTAssertNil(unstarted)
        let accepted = Task { try await pipeline.generate(identity: captured, source: "still first") }
        await backend.waitForStart(1)
        await backend.finish(captured, value: "accepted")
        _ = try await accepted.value
        try await pipeline.retire()
    }
    func testShouldWaitForSuccessfulActiveDrainBeforeRetirementFinishes() async throws {
        let backend = FakePreviewExecutor()
        let pipeline = WorkerPreviewPipeline(executor: backend)
        let captured = try identity(1, document: UUID())
        let active = Task { try await pipeline.generate(identity: captured, source: "active") }
        await backend.waitForStart(1)
        let retirement = Task { try await pipeline.retire() }
        try await waitUntil { await pipeline.isRetired }
        let held = await pipeline.activeIdentity
        XCTAssertEqual(held, captured)
        await backend.finish(captured, value: "retired")
        do {
            _ = try await active.value
            XCTFail("Retired result published")
        } catch WorkerPreviewAdmissionError.retired {}
        try await retirement.value
        let drained = await pipeline.activeIdentity
        XCTAssertNil(drained)
        try await pipeline.retire()
    }
}

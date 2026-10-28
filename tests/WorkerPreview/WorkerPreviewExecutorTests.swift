// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Foundation
import XCTest

@testable import Dust3DWorkerPreview
@testable import Dust3DWorkerXPC

private enum ExecutorTestError: Error, Equatable { case unexpected, writing, reading, control, retirement }
private final class ExecutorEvents: @unchecked Sendable {
    // Test-only synchronous recorder; every access uses the same lock.
    private let lock = NSLock()
    private var values: [String] = []
    func append(_ value: String) { lock.withLock { values.append(value) } }
    var snapshot: [String] { lock.withLock { values } }
}
private actor ExecutorTransport: WorkerPreviewTransport {
    let events: ExecutorEvents
    private var invalidated = false
    private var invalidationWaiter: CheckedContinuation<Void, Never>?
    func waitUntilInvalidated() async {
        if invalidated { return }
        await withCheckedContinuation { invalidationWaiter = $0 }
    }
    private let failsHandshake: Bool
    private let failsRetirement: Bool
    private let handshakeProcess: Int32
    private let resetStatus: UInt32
    init(
        _ events: ExecutorEvents, failsHandshake: Bool = false, failsRetirement: Bool = false,
        handshakeProcess: Int32 = 42, resetStatus: UInt32 = 0
    ) {
        self.events = events
        self.failsHandshake = failsHandshake
        self.failsRetirement = failsRetirement
        self.handshakeProcess = handshakeProcess
        self.resetStatus = resetStatus
    }
    func handshake(generation: UInt64) async throws -> WorkerXPCHandshakeReceipt {
        events.append("handshake")
        if failsHandshake { throw ExecutorTestError.unexpected }
        return WorkerXPCHandshakeReceipt(
            process: handshakeProcess, sandboxEntitlementVerified: true, admissionStatus: 0,
            connectionGeneration: generation, observedProcess: handshakeProcess)
    }
    func submitPreview(
        input: FileHandle, output: FileHandle, isStream: Bool, identity: WorkerPreviewIdentity,
        options: WorkerXPCPreviewOptions
    ) async throws -> WorkerPreviewControlReceipt {
        throw ExecutorTestError.unexpected
    }
    func resetSession(
        document: UUID, request: UInt64, generation: UInt64,
        maximumCachePayloadBytes: UInt64
    ) async throws -> WorkerXPCLegacyReceipt {
        events.append("reset:\(request):\(maximumCachePayloadBytes)")
        return WorkerXPCLegacyReceipt(
            status: resetStatus, operation: 0, request: request, systemError: 0, stage: 0,
            source: 0, code: 0, truncated: 0, process: 42, sandboxEntitlementVerified: true,
            finalizationError: 0, admissionStatus: 0, connectionGeneration: generation,
            observedProcess: 42, cause: Data())
    }
    func invalidate() async throws -> Int32 {
        events.append("invalidate")
        invalidated = true
        invalidationWaiter?.resume()
        invalidationWaiter = nil
        if failsRetirement { throw ExecutorTestError.retirement }
        return 42
    }
}
private struct ImmediatePreviewJob: WorkerPreviewJob {
    typealias Value = String
    let identity: WorkerPreviewIdentity
    let events: ExecutorEvents
    var process: Int32 = 42
    func write() async throws { events.append("write") }
    func read() async throws -> String {
        events.append("read")
        return "owned value"
    }
    func control() async throws -> WorkerPreviewControlReceipt {
        events.append("control")
        return WorkerPreviewControlReceipt(
            status: 0, request: identity.request, systemError: 0, stage: 0,
            source: 0, code: 0, truncated: 0, process: process, sandboxEntitlementVerified: true,
            finalizationError: 0, admissionStatus: 0, boundaryStatus: 0, connectionGeneration: identity.generation,
            observedProcess: process, cause: Data())
    }
}
// Fake phase rendezvous: no descriptors, native calls, deadlines or cancellation probes.
private actor HeldPreviewReader {
    private var continuation: CheckedContinuation<String, any Error>?
    private var startedWaiter: CheckedContinuation<Void, Never>?
    private var started = false
    func read() async throws -> String {
        try await withCheckedThrowingContinuation {
            continuation = $0
            started = true
            startedWaiter?.resume()
            startedWaiter = nil
        }
    }
    func waitUntilStarted() async {
        if started { return }
        await withCheckedContinuation { startedWaiter = $0 }
    }
    func fail() {
        continuation?.resume(throwing: ExecutorTestError.reading)
        continuation = nil
    }
}
private struct FailingPreviewJob: WorkerPreviewJob {
    let reader: HeldPreviewReader
    func write() async throws { throw ExecutorTestError.writing }
    func read() async throws -> String { try await reader.read() }
    func control() async throws -> WorkerPreviewControlReceipt { throw ExecutorTestError.control }
}
@MainActor
final class WorkerPreviewExecutorTests: XCTestCase {
    func testShouldHandshakeBeforeFactoryAndJoinAllOwnedPhases() async throws {
        let events = ExecutorEvents()
        let executor = WorkerXPCPreviewExecutor<String>(transport: ExecutorTransport(events)) { identity, _, _ in
            events.append("factory")
            return ImmediatePreviewJob(identity: identity, events: events)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
            maximumOutputTransferBytes: 1024, outputChunkBytes: 64)
        let source = try WorkerPreviewSource(archive: Data([1, 2, 3]), options: options)
        let completion = try await executor.execute(identity: identity, source: source)
        XCTAssertEqual(completion.value, "owned value")
        XCTAssertTrue(completion.requestWritten)
        XCTAssertTrue(completion.reachedEOF)
        XCTAssertEqual(Array(events.snapshot.prefix(2)), ["handshake", "factory"])
        XCTAssertEqual(Set(events.snapshot.suffix(3)), Set(["write", "read", "control"]))
        let retirement = try await executor.retire()
        XCTAssertTrue(retirement.invalidationRequested)
        XCTAssertEqual(retirement.peer?.observedProcess, 42)
    }
    func testShouldKeepFailedLifetimeBusyUntilEveryFakePhaseJoins() async throws {
        let events = ExecutorEvents()
        let transport = ExecutorTransport(events)
        let reader = HeldPreviewReader()
        let executor = WorkerXPCPreviewExecutor<String>(transport: transport) { _, _, _ in
            FailingPreviewJob(reader: reader)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
            maximumOutputTransferBytes: 1024, outputChunkBytes: 64)
        let source = try WorkerPreviewSource(archive: Data([1, 2, 3]), options: options)
        let execution = Task { try await executor.execute(identity: identity, source: source) }
        await reader.waitUntilStarted()
        await transport.waitUntilInvalidated()
        let active = await executor.activeIdentity
        XCTAssertEqual(active, identity)
        do {
            _ = try await executor.retire()
            XCTFail("Retirement must wait for the owned reader")
        } catch WorkerPreviewExecutorError.busy {
        } catch { XCTFail("Unexpected retirement error: \(error)") }
        await reader.fail()
        do {
            _ = try await execution.value
            XCTFail("No partial value may escape failed phases")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.writing, .reading, .control])
            XCTAssertEqual(failures.compactMap { $0.cause as? ExecutorTestError }, [.writing, .reading, .control])
        }
        let retired = await executor.isRetired
        let remaining = await executor.activeIdentity
        XCTAssertTrue(retired)
        XCTAssertNil(remaining)
        _ = try await executor.retire()
        XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
        do {
            _ = try await executor.execute(identity: identity, source: source)
            XCTFail("A failed lifetime must not reconnect")
        } catch WorkerPreviewExecutorError.retired {
        } catch { XCTFail("Unexpected retired error: \(error)") }
        XCTAssertEqual(events.snapshot.filter { $0 == "handshake" }.count, 1)
    }

    func testShouldPreservePreparationAndRetirementFailuresWithoutCreatingChannels() async throws {
        let events = ExecutorEvents()
        let transport = ExecutorTransport(events, failsHandshake: true, failsRetirement: true)
        let executor = WorkerXPCPreviewExecutor<String>(transport: transport) { identity, _, _ in
            events.append("factory")
            return ImmediatePreviewJob(identity: identity, events: events)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
            maximumOutputTransferBytes: 1024, outputChunkBytes: 64)
        let source = try WorkerPreviewSource(archive: Data([1, 2, 3]), options: options)
        do {
            _ = try await executor.execute(identity: identity, source: source)
            XCTFail("Preparation failure must not create channels")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.preparation, .retirement])
            XCTAssertEqual(failures.compactMap { $0.cause as? ExecutorTestError }, [.unexpected, .retirement])
        }
        do {
            _ = try await executor.retire()
            XCTFail("Original retirement cause must remain observable")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.retirement])
            XCTAssertEqual(failures.first?.cause as? ExecutorTestError, .retirement)
        }
        XCTAssertEqual(events.snapshot, ["handshake", "invalidate"])
    }

    func testShouldBindScopeAndMonotonicRequestsAcrossResetWithoutRehandshake() async throws {
        let events = ExecutorEvents()
        let executor = WorkerXPCPreviewExecutor<String>(transport: ExecutorTransport(events)) { identity, _, _ in
            events.append("factory")
            return ImmediatePreviewJob(identity: identity, events: events)
        }
        let document = UUID()
        func identity(_ request: UInt64, document: UUID, generation: UInt64 = 9) throws -> WorkerPreviewIdentity {
            try WorkerPreviewIdentity(document: document, request: request, revision: 7, generation: generation)
        }
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
            maximumOutputTransferBytes: 1024, outputChunkBytes: 64)
        let source = try WorkerPreviewSource(archive: Data([1, 2, 3]), options: options)
        do {
            _ = try await executor.reset(request: 1)
            XCTFail("Reset requires a bound document")
        } catch WorkerPreviewExecutorError.uninitialized {
        }
        _ = try await executor.execute(identity: identity(1, document: document), source: source)
        do {
            _ = try await executor.execute(identity: identity(1, document: document), source: source)
            XCTFail("Replayed request must not create a job")
        } catch WorkerPreviewExecutorError.replayed {
        }
        for foreign in [try identity(2, document: UUID()), try identity(2, document: document, generation: 10)] {
            do {
                _ = try await executor.execute(identity: foreign, source: source)
                XCTFail("Foreign document or generation must not replace the lifetime")
            } catch WorkerPreviewExecutorError.foreignScope {
            }
        }
        let reset = try await executor.reset(request: 2)
        XCTAssertEqual(reset.request, 2)
        do {
            _ = try await executor.reset(request: 2)
            XCTFail("Reset shares the monotonic request domain")
        } catch WorkerPreviewExecutorError.replayed {
        }
        _ = try await executor.execute(identity: identity(3, document: document), source: source)
        XCTAssertEqual(events.snapshot.filter { $0 == "handshake" }.count, 1)
        XCTAssertEqual(events.snapshot.filter { $0 == "factory" }.count, 2)
        XCTAssertEqual(events.snapshot.filter { $0.hasPrefix("reset:") }, ["reset:2:0"])
        let retired = await executor.isRetired
        XCTAssertFalse(retired)
        _ = try await executor.retire()
        _ = try await executor.retire()
        XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
    }

    private func source() throws -> WorkerPreviewSource {
        try WorkerPreviewSource(
            archive: Data([1, 2, 3]),
            options: WorkerXPCPreviewOptions(
                maximumCachePayloadBytes: 0, archiveBytes: 3,
                maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
                maximumOutputTransferBytes: 1024, outputChunkBytes: 64))
    }
    func testShouldRejectDifferentReceiptPeerEvenWhenReceiptFieldsAgreeWithEachOther() async throws {
        let events = ExecutorEvents()
        let executor = WorkerXPCPreviewExecutor<String>(transport: ExecutorTransport(events)) { identity, _, _ in
            ImmediatePreviewJob(identity: identity, events: events, process: 77)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        do {
            _ = try await executor.execute(identity: identity, source: source())
            XCTFail("Receipt must match the independently retained handshake peer")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.control])
            guard let cause = failures.first?.cause as? WorkerPreviewExecutorError,
                case .peerReceipt(let expected, let received) = cause
            else {
                return XCTFail("Original peer observations must be retained")
            }
            XCTAssertEqual(expected.observedProcess, 42)
            XCTAssertEqual(received.observedProcess, 77)
        }
        XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
    }
    func testShouldRetainResetRejectionAndSealLifetime() async throws {
        let events = ExecutorEvents()
        let executor = WorkerXPCPreviewExecutor<String>(transport: ExecutorTransport(events, resetStatus: 7)) {
            identity, _, _ in
            ImmediatePreviewJob(identity: identity, events: events)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        _ = try await executor.execute(identity: identity, source: source())
        do {
            _ = try await executor.reset(request: 2)
            XCTFail("Rejected reset must not leave a reusable lifetime")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.control])
            guard let cause = failures.first?.cause as? WorkerPreviewExecutorError,
                case .resetReceipt(let receipt) = cause
            else {
                return XCTFail("Original reset receipt must be retained")
            }
            XCTAssertEqual(receipt.status, 7)
            XCTAssertEqual(receipt.request, 2)
        }
        let retired = await executor.isRetired
        XCTAssertTrue(retired)
        XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
    }
    func testShouldRejectMissingHandshakeObservationBeforeFactory() async throws {
        let events = ExecutorEvents()
        let executor = WorkerXPCPreviewExecutor<String>(transport: ExecutorTransport(events, handshakeProcess: 0)) {
            identity, _, _ in
            events.append("factory")
            return ImmediatePreviewJob(identity: identity, events: events)
        }
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        do {
            _ = try await executor.execute(identity: identity, source: source())
            XCTFail("Missing OS peer observation must prevent channel creation")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.preparation])
            guard let cause = failures.first?.cause as? WorkerPreviewExecutorError,
                case .handshake(let receipt) = cause
            else {
                return XCTFail("Original handshake receipt must be retained")
            }
            XCTAssertEqual(receipt.observedProcess, 0)
        }
        XCTAssertEqual(events.snapshot, ["handshake", "invalidate"])
    }

}

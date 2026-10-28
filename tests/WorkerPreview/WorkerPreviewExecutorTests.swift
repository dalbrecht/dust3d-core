// SPDX-License-Identifier: MIT
import Dust3DWorkerClient
import Foundation
import XCTest

@testable import Dust3DWorkerPreview
@testable import Dust3DWorkerXPC

private enum ExecutorTestError: Error, Equatable {
    case unexpected, writing, reading, control, retirement, registration
}
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
    private let handshakeFailure: WorkerXPCHandshakeFailure?
    private let failedAttempt: Int
    private let failsHandshake: Bool
    private let failsRetirement: Bool
    private let handshakeProcess: Int32
    private let resetStatus: UInt32
    private let changesBarrierInstance: Bool
    private var handshakes = 0
    private(set) var handshakeBudgets: [UInt64] = []
    init(
        _ events: ExecutorEvents, failsHandshake: Bool = false, failsRetirement: Bool = false,
        handshakeProcess: Int32 = 42, resetStatus: UInt32 = 0, changesBarrierInstance: Bool = false,
        handshakeFailure: WorkerXPCHandshakeFailure? = nil, failedAttempt: Int = 1
    ) {
        self.events = events
        self.handshakeFailure = handshakeFailure
        self.failedAttempt = failedAttempt
        self.failsHandshake = failsHandshake
        self.failsRetirement = failsRetirement
        self.handshakeProcess = handshakeProcess
        self.resetStatus = resetStatus
        self.changesBarrierInstance = changesBarrierInstance
    }
    func handshake(generation: UInt64, timeoutMilliseconds: UInt64) async throws -> WorkerXPCHandshakeReceipt {
        events.append("handshake")
        handshakes += 1
        handshakeBudgets.append(timeoutMilliseconds)
        if handshakes == failedAttempt, let handshakeFailure { throw handshakeFailure }
        if failsHandshake { throw ExecutorTestError.unexpected }
        return WorkerXPCHandshakeReceipt(
            instance: WorkerXPCServiceInstance(high: 1, low: changesBarrierInstance && handshakes > 1 ? 2 : 1),
            process: handshakeProcess,
            sandboxEntitlementVerified: true, admissionStatus: 0,
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
private actor ExecutorExitWatch: WorkerPreviewExitWatching {
    let peer: WorkerXPCHandshakeReceipt
    let events: ExecutorEvents
    let observationPeer: WorkerXPCHandshakeReceipt?
    let failsRegistration: Bool
    init(
        peer: WorkerXPCHandshakeReceipt, events: ExecutorEvents,
        observationPeer: WorkerXPCHandshakeReceipt? = nil, failsRegistration: Bool = false
    ) {
        self.peer = peer
        self.events = events
        self.observationPeer = observationPeer
        self.failsRegistration = failsRegistration
    }
    func register(timeoutMilliseconds: UInt64) async throws {
        events.append("register")
        if failsRegistration { throw ExecutorTestError.registration }
    }
    func waitForExit(timeoutMilliseconds: UInt64) async throws -> WorkerXPCExitObservation {
        events.append("exit")
        return WorkerXPCExitObservation(peer: observationPeer ?? peer)
    }
}
private actor HeldExitRegistration: WorkerPreviewExitWatching {
    let peer: WorkerXPCHandshakeReceipt
    let events: ExecutorEvents
    private var pending: CheckedContinuation<Void, Never>?
    private var started: CheckedContinuation<Void, Never>?
    init(peer: WorkerXPCHandshakeReceipt, events: ExecutorEvents) {
        self.peer = peer
        self.events = events
    }
    func register(timeoutMilliseconds: UInt64) async throws {
        events.append("register-start")
        await withCheckedContinuation { continuation in
            pending = continuation
            started?.resume()
            started = nil
        }
    }
    func waitUntilStarted() async {
        if pending != nil { return }
        await withCheckedContinuation { started = $0 }
    }
    func finish() {
        events.append("register-finish")
        pending?.resume()
        pending = nil
    }
    func waitForExit(timeoutMilliseconds: UInt64) async throws -> WorkerXPCExitObservation {
        WorkerXPCExitObservation(peer: peer)
    }
}
private func makePreviewExecutor(
    events: ExecutorEvents, transport: any WorkerPreviewTransport,
    makeExitWatch: WorkerXPCPreviewExecutor<String>.ExitFactory? = nil,
    makeJob: @escaping WorkerXPCPreviewExecutor<String>.Factory
) throws -> WorkerXPCPreviewExecutor<String> {
    try WorkerXPCPreviewExecutor(
        transport: transport, registrationTimeoutMilliseconds: 50, handshakeTimeoutMilliseconds: 40,
        makeExitWatch: makeExitWatch ?? { ExecutorExitWatch(peer: $0, events: events) }, makeJob: makeJob)
}
@MainActor
final class WorkerPreviewExecutorTests: XCTestCase {
    func testShouldHandshakeBeforeFactoryAndJoinAllOwnedPhases() async throws {
        let events = ExecutorEvents()
        let transport = ExecutorTransport(events)
        let executor = try makePreviewExecutor(
            events: events, transport: transport,
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27, inputChunkBytes: 64,
            maximumOutputTransferBytes: 1024, outputChunkBytes: 64)
        let source = try WorkerPreviewSource(archive: Data([1, 2, 3]), options: options)
        let completion = try await executor.execute(identity: identity, source: source)
        let budgets = await transport.handshakeBudgets
        XCTAssertEqual(budgets, [40, 40])
        XCTAssertEqual(completion.value, "owned value")
        XCTAssertTrue(completion.requestWritten)
        XCTAssertTrue(completion.reachedEOF)
        XCTAssertEqual(Array(events.snapshot.prefix(4)), ["handshake", "register", "handshake", "factory"])
        XCTAssertEqual(Set(events.snapshot.suffix(3)), Set(["write", "read", "control"]))
        let retirement = try await executor.retire()
        XCTAssertTrue(retirement.invalidationRequested)
        XCTAssertEqual(retirement.peer?.observedProcess, 42)
    }
    func testShouldKeepFailedLifetimeBusyUntilEveryFakePhaseJoins() async throws {
        let events = ExecutorEvents()
        let transport = ExecutorTransport(events)
        let reader = HeldPreviewReader()
        let executor = try makePreviewExecutor(events: events, transport: transport) { _, _, _ in
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
        XCTAssertEqual(events.snapshot.filter { $0 == "handshake" }.count, 2)
    }

    func testShouldPreservePreparationAndRetirementFailuresWithoutCreatingChannels() async throws {
        let events = ExecutorEvents()
        let transport = ExecutorTransport(events, failsHandshake: true, failsRetirement: true)
        let executor = try makePreviewExecutor(
            events: events, transport: transport,
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
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
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
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
        XCTAssertEqual(events.snapshot.filter { $0 == "handshake" }.count, 2)
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
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeJob: { identity, _, _ in
                ImmediatePreviewJob(identity: identity, events: events, process: 77)
            })
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
        let executor = try makePreviewExecutor(events: events, transport: ExecutorTransport(events, resetStatus: 7)) {
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
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events, handshakeProcess: 0)
        ) {
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

    func testShouldObserveExitOnlyAfterIdleNormalInvalidation() async throws {
        let events = ExecutorEvents()
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        _ = try await executor.execute(identity: identity, source: source())
        let observed = try await executor.retireAndObserve(timeoutMilliseconds: 75)
        XCTAssertEqual(observed.peer.observedProcess, 42)
        XCTAssertEqual(Array(events.snapshot.prefix(4)), ["handshake", "register", "handshake", "factory"])
        XCTAssertEqual(Array(events.snapshot.suffix(2)), ["invalidate", "exit"])
    }

    func testShouldNeverClaimAuthenticatedExitWhenPostRegistrationBarrierFailed() async throws {
        let events = ExecutorEvents()
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events, changesBarrierInstance: true),
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        do {
            _ = try await executor.execute(identity: identity, source: source())
            XCTFail("Changed incarnation cannot cross the channel factory barrier")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.preparation])
            guard let cause = failures.first?.cause as? WorkerPreviewExecutorError,
                case .handshakeBarrier(let expected, let received) = cause
            else { return XCTFail("Retain both original barrier receipts") }
            XCTAssertEqual(expected.instance.low, 1)
            XCTAssertEqual(received.instance.low, 2)
        }
        XCTAssertEqual(events.snapshot, ["handshake", "register", "handshake", "invalidate"])
        let retained = await executor.peerIdentity
        XCTAssertEqual(retained?.instance.low, 1)
        do {
            _ = try await executor.retireAndObserve(timeoutMilliseconds: 75)
            XCTFail("An unverified observer may not emit authenticated process-exit evidence")
        } catch WorkerPreviewExecutorError.uninitialized {
        }
        XCTAssertFalse(events.snapshot.contains("exit"))
    }

    func testShouldRetainActiveSlotAndDelayFactoryUntilRegistrationAndBarrierFinish() async throws {
        let events = ExecutorEvents()
        let captured = WorkerXPCHandshakeReceipt(
            instance: WorkerXPCServiceInstance(high: 1, low: 1), process: 42,
            sandboxEntitlementVerified: true, admissionStatus: 0, connectionGeneration: 9, observedProcess: 42)
        let watch = HeldExitRegistration(peer: captured, events: events)
        let executor = try WorkerXPCPreviewExecutor<String>(
            transport: ExecutorTransport(events),
            registrationTimeoutMilliseconds: 50, handshakeTimeoutMilliseconds: 40, makeExitWatch: { _ in watch },
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let source = try source()
        let execution = Task { try await executor.execute(identity: identity, source: source) }
        await watch.waitUntilStarted()
        XCTAssertEqual(events.snapshot, ["handshake", "register-start"])
        do {
            _ = try await executor.execute(identity: identity, source: source)
            XCTFail("Registration retains the active capacity")
        } catch WorkerPreviewExecutorError.busy {
        }
        do {
            _ = try await executor.retireAndObserve(timeoutMilliseconds: 75)
            XCTFail("Retirement cannot overtake active setup")
        } catch WorkerPreviewExecutorError.busy {
        }
        await watch.finish()
        _ = try await execution.value
        XCTAssertEqual(
            Array(events.snapshot.prefix(5)),
            ["handshake", "register-start", "register-finish", "handshake", "factory"])
        _ = try await executor.retireAndObserve(timeoutMilliseconds: 75)
    }

    func testShouldRetainRegistrationFailureWithoutRunningBarrierOrFactory() async throws {
        let events = ExecutorEvents()
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeExitWatch: { ExecutorExitWatch(peer: $0, events: events, failsRegistration: true) },
            makeJob: { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        do {
            _ = try await executor.execute(identity: identity, source: source())
            XCTFail("Unregistered observers must not allow job channels")
        } catch WorkerPreviewExecutorError.phases(let failures) {
            XCTAssertEqual(failures.map(\.phase), [.preparation])
            XCTAssertEqual(failures.first?.cause as? ExecutorTestError, .registration)
        }
        XCTAssertEqual(events.snapshot, ["handshake", "register", "invalidate"])
        let captured = await executor.peerIdentity
        XCTAssertEqual(captured?.observedProcess, 42)
    }
    func testShouldRejectForeignExitObservationWithoutReplacingOriginalPeer() async throws {
        let events = ExecutorEvents()
        let foreign = WorkerXPCHandshakeReceipt(
            instance: WorkerXPCServiceInstance(high: 9, low: 9), process: 42,
            sandboxEntitlementVerified: true, admissionStatus: 0, connectionGeneration: 9, observedProcess: 42)
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeExitWatch: { ExecutorExitWatch(peer: $0, events: events, observationPeer: foreign) },
            makeJob: { identity, _, _ in
                ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        _ = try await executor.execute(identity: identity, source: source())
        do {
            _ = try await executor.retireAndObserve(timeoutMilliseconds: 75)
            XCTFail("Foreign incarnation cannot provide exit evidence")
        } catch WorkerPreviewExecutorError.exitObservation(let expected, let received) {
            XCTAssertEqual(expected.instance.low, 1)
            XCTAssertEqual(received.peer.instance.low, 9)
        }
        let captured = await executor.peerIdentity
        XCTAssertEqual(captured?.instance.low, 1)
        XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
    }
    func testShouldValidateExitBudgetBeforeRequestingInvalidation() async throws {
        let events = ExecutorEvents()
        let executor = try makePreviewExecutor(
            events: events, transport: ExecutorTransport(events),
            makeJob: { identity, _, _ in
                ImmediatePreviewJob(identity: identity, events: events)
            })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        _ = try await executor.execute(identity: identity, source: source())
        do {
            _ = try await executor.retireAndObserve(timeoutMilliseconds: 0)
            XCTFail("Invalid observation policy cannot retire a healthy lifetime")
        } catch WorkerXPCExitObservationError.invalidBudget {
        }
        let retired = await executor.isRetired
        XCTAssertFalse(retired)
        XCTAssertFalse(events.snapshot.contains("invalidate"))
        _ = try await executor.retireAndObserve(timeoutMilliseconds: 75)
    }

    func testShouldRetainDeadlineFailureFromEitherHandshakeBeforeFactory() async throws {
        for failedAttempt in [1, 2] {
            let events = ExecutorEvents()
            let original = WorkerXPCHandshakeFailure(
                token: UInt64(failedAttempt), generation: 9, budgetMilliseconds: 40,
                cause: .expired(start: 10, deadline: 40_000_010, observed: 40_000_010),
                originalTransportCause: nil)
            let transport = ExecutorTransport(events, handshakeFailure: original, failedAttempt: failedAttempt)
            let executor = try makePreviewExecutor(events: events, transport: transport) { identity, _, _ in
                events.append("factory")
                return ImmediatePreviewJob(identity: identity, events: events)
            }
            let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
            do {
                _ = try await executor.execute(identity: identity, source: source())
                XCTFail("A handshake deadline must prevent native job creation")
            } catch WorkerPreviewExecutorError.phases(let failures) {
                XCTAssertEqual(failures.map(\.phase), [.preparation])
                let retained = failures.first?.cause as? WorkerXPCHandshakeFailure
                XCTAssertEqual(retained?.token, original.token)
                XCTAssertEqual(retained?.generation, original.generation)
                XCTAssertEqual(retained?.budgetMilliseconds, original.budgetMilliseconds)
                XCTAssertEqual(retained?.cause, original.cause)
            }
            let budgets = await transport.handshakeBudgets
            XCTAssertEqual(budgets, Array(repeating: 40, count: failedAttempt))
            XCTAssertFalse(events.snapshot.contains("factory"))
            let retired = await executor.isRetired
            XCTAssertTrue(retired)
            XCTAssertEqual(events.snapshot.filter { $0 == "invalidate" }.count, 1)
        }
    }
    func testShouldRejectInvalidHandshakeBudgetBeforeConstructingPublicTransport() throws {
        let configuration = try WorkerXPCConfiguration(
            serviceIdentifier: "org.dust3d.core.xpc-static-service.qa",
            signingRequirement: "identifier \"org.dust3d.core.xpc-static-service.qa\"")
        for invalid: UInt64 in [0, 10001] {
            XCTAssertThrowsError(
                try WorkerXPCPreviewExecutor<String>(
                    configuration: configuration, registrationTimeoutMilliseconds: 50,
                    handshakeTimeoutMilliseconds: invalid, copy: { _ in "unused" })
            ) { cause in
                XCTAssertEqual(cause as? WorkerXPCHandshakeDeadlineError, .invalidBudget(milliseconds: invalid))
            }
        }
    }
    func testShouldConstructPublicExecutorWithoutAuthenticatingOrLaunchingWork() async throws {
        // Local requirement compilation and an unresumed Foundation object only; no service contact.
        let configuration = try WorkerXPCConfiguration(
            serviceIdentifier: "org.dust3d.core.xpc-static-service.qa",
            signingRequirement: "identifier \"org.dust3d.core.xpc-static-service.qa\"")
        for invalid: UInt64 in [0, 10001] {
            do {
                _ = try WorkerXPCPreviewExecutor<String>(
                    configuration: configuration,
                    registrationTimeoutMilliseconds: invalid, handshakeTimeoutMilliseconds: 40, copy: { _ in "unused" })
                XCTFail("Registration policy must validate before transport construction")
            } catch WorkerXPCExitObservationError.invalidBudget {
            }
        }
        let executor = try WorkerXPCPreviewExecutor<String>(
            configuration: configuration,
            registrationTimeoutMilliseconds: 50, handshakeTimeoutMilliseconds: 40, copy: { _ in "unused" })
        let active = await executor.activeIdentity
        let peer = await executor.peerIdentity
        let retired = await executor.isRetired
        XCTAssertNil(active)
        XCTAssertNil(peer)
        XCTAssertFalse(retired)
    }

}

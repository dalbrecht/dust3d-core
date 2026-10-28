import Dust3DWorkerClient
import Foundation
import XCTest

// SPDX-License-Identifier: MIT
@testable import Dust3DWorkerXPC

private final class MissingChannel: WorkerXPCChannel {
    var processIdentifier: Int32 { 42 }
    func installHandlers(invalidated: @escaping @Sendable () -> Void, interrupted: @escaping @Sendable () -> Void) {}
    func resume() {}
    func invalidate() {}
    func proxy(errorHandler: @escaping @Sendable (any Error) -> Void) -> (any Dust3DWorkerXPCProtocol)? { nil }
}
@MainActor
final class WorkerXPCConnectionTests: XCTestCase {
    func testMissingProxyRetiresConnectionWithoutNativeWork() async throws {
        let connection = WorkerXPCConnection(factory: { MissingChannel() })
        do {
            _ = try await connection.closeSession(request: 1, generation: 9)
            XCTFail("Missing proxy must fail")
        } catch WorkerXPCTransportError.missingProxy {}
        do {
            _ = try await connection.closeSession(request: 2, generation: 9)
            XCTFail("Retired connection must reject")
        } catch WorkerXPCTransportError.retired {}
        let process = try await connection.invalidate()
        XCTAssertEqual(process, 42)
    }
    func testDuplicateRepliesAndPeerRebindingAreCorrelated() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .duplicate
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let first = try await connection.closeSession(request: 1, generation: 9)
        XCTAssertEqual(first.observedProcess, 42)
        let second = try await connection.closeSession(request: 2, generation: 9)
        XCTAssertEqual(second.request, 2)
        facts.process = 43
        do {
            _ = try await connection.closeSession(request: 3, generation: 9)
            XCTFail("Changed independent peer must fail")
        } catch WorkerXPCTransportError.peerChanged(let expected, let observed) {
            XCTAssertEqual(expected, 42)
            XCTAssertEqual(observed, 43)
        }
    }
    func testPendingRequestRejectsOverlapAndIdleInvalidation() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .hold
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let first = Task { try await connection.closeSession(request: 1, generation: 9) }
        await facts.waitUntilHeld()
        do {
            _ = try await connection.invalidate()
            XCTFail("Pending RPC must remain owned")
        } catch WorkerXPCTransportError.busy {}
        do {
            _ = try await connection.closeSession(request: 2, generation: 9)
            XCTFail("Overlap must reject")
        } catch WorkerXPCTransportError.busy {}
        facts.release()
        let receipt = try await first.value
        XCTAssertEqual(receipt.request, 1)
        let process = try await connection.invalidate()
        XCTAssertEqual(process, 42)
    }
    func testRemoteCauseAndInterruptedLifetimeRemainOriginal() async throws {
        let remoteFacts = FakeXPCFacts()
        remoteFacts.mode = .remote
        let remote = WorkerXPCConnection(factory: { FakeXPCChannel(remoteFacts) })
        do {
            _ = try await remote.closeSession(request: 1, generation: 9)
            XCTFail("Remote error expected")
        } catch WorkerXPCTransportError.remote(let cause) {
            XCTAssertEqual((cause as NSError).domain, "FakeXPC")
            XCTAssertEqual((cause as NSError).code, -18)
            XCTAssertEqual((cause as NSError).localizedDescription, "Original cause")
        }
        let facts = FakeXPCFacts()
        facts.mode = .hold
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let pending = Task { try await connection.closeSession(request: 1, generation: 9) }
        await facts.waitUntilHeld()
        facts.interrupt()
        do {
            _ = try await pending.value
            XCTFail("Interruption must retire")
        } catch WorkerXPCTransportError.connectionInterrupted {}
        facts.release()
        do {
            _ = try await connection.closeSession(request: 2, generation: 9)
            XCTFail("No automatic reconnect")
        } catch WorkerXPCTransportError.retired {}
    }
    func testPreviewReceiptAndRawOptionsKeepTheirDomains() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .failureReceipt
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 7, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: UInt64.max, maximumSourceBytes: 0, maximumInputTransferBytes: 27,
            inputChunkBytes: 64, maximumOutputTransferBytes: 128, outputChunkBytes: 256)
        let receipt = try await connection.submitPreview(
            input: .standardInput, output: .standardOutput,
            identity: identity, options: options)
        XCTAssertEqual(facts.recordedBudget, UInt64.max)
        XCTAssertEqual(receipt.status, 7)
        XCTAssertEqual(receipt.boundaryStatus, 3)
        XCTAssertEqual(receipt.source, 33)
        XCTAssertEqual(receipt.code, -4)
        XCTAssertEqual(receipt.cause, Data([255]))
        XCTAssertEqual(receipt.process, 52)
        XCTAssertEqual(receipt.observedProcess, 42)
        XCTAssertEqual(receipt.connectionGeneration, 9)
    }
    func testOversizedFakeDiagnosticRetiresWithoutNativeWork() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .oversized
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let identity = try WorkerPreviewIdentity(document: UUID(), request: 1, revision: 0, generation: 9)
        let options = WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: 0, archiveBytes: 3,
            maximumIndexedBytes: 0, maximumSourceBytes: 0, maximumInputTransferBytes: 27,
            inputChunkBytes: 64, maximumOutputTransferBytes: 128, outputChunkBytes: 256)
        do {
            _ = try await connection.submitPreview(
                input: .standardInput, output: .standardOutput,
                identity: identity, options: options)
            XCTFail("Diagnostic bound expected")
        } catch WorkerXPCTransportError.diagnosticExceeded(let actual) { XCTAssertEqual(actual, 512) }
    }

    func testInvalidationFailsPendingOnceAndMissingPeerIsTyped() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .hold
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let pending = Task { try await connection.closeSession(request: 1, generation: 9) }
        await facts.waitUntilHeld()
        facts.invalidate()
        do {
            _ = try await pending.value
            XCTFail("Invalidation must fail pending control")
        } catch WorkerXPCTransportError.connectionInvalidated {}
        facts.release()
        let missingFacts = FakeXPCFacts()
        missingFacts.process = 0
        let missing = WorkerXPCConnection(factory: { FakeXPCChannel(missingFacts) })
        do {
            _ = try await missing.closeSession(request: 1, generation: 9)
            XCTFail("OS peer required")
        } catch WorkerXPCTransportError.missingPeer {}
    }
    func testPreflightAndGenerationRejectionDoNotReplaceConnectionWork() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        do {
            _ = try await connection.closeSession(request: 0, generation: 9)
            XCTFail("Zero request must reject")
        } catch WorkerXPCTransportError.invalidRequest {}
        do {
            _ = try await connection.submit(
                input: .standardInput, output: .standardInput,
                isStream: false, request: 1, generation: 9)
            XCTFail("Aliased channel must reject before RPC")
        } catch WorkerXPCTransportError.invalidChannels {}
        do {
            _ = try await connection.closeSession(request: 1, generation: 0)
            XCTFail("Zero generation must reject")
        } catch WorkerXPCTransportError.invalidGeneration {}
        _ = try await connection.closeSession(request: 1, generation: 9)
        do {
            _ = try await connection.closeSession(request: 2, generation: 10)
            XCTFail("Connection cannot rebind")
        } catch WorkerXPCTransportError.generationMismatch {}
        let recovered = try await connection.closeSession(request: 3, generation: 9)
        XCTAssertEqual(recovered.request, 3)
    }

    func testAllProfilesUseOneConnectionWithoutNormalizingOptions() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let document = UUID()
        let base = try await connection.submit(
            input: .standardInput, output: .standardOutput,
            isStream: false, request: 1, generation: 9)
        let sessionIdentity = try WorkerPreviewIdentity(document: document, request: 2, revision: 7, generation: 9)
        let session = try await connection.submitSession(
            input: .standardInput, output: .standardOutput,
            identity: sessionIdentity,
            options: WorkerXPCSessionOptions(
                maximumCachePayloadBytes: 0,
                maximumWireBytes: UInt64.max, maximumDecodedBytes: UInt64.max, maximumSourceBytes: 0))
        let previewIdentity = try WorkerPreviewIdentity(document: document, request: 3, revision: 7, generation: 9)
        let preview = try await connection.submitPreview(
            input: .standardInput, output: .standardOutput,
            identity: previewIdentity,
            options: WorkerXPCPreviewOptions(
                maximumCachePayloadBytes: 0,
                archiveBytes: 3, maximumIndexedBytes: UInt64.max, maximumSourceBytes: 0,
                maximumInputTransferBytes: 27, inputChunkBytes: 64,
                maximumOutputTransferBytes: UInt64.max, outputChunkBytes: 256))
        let reset = try await connection.resetSession(
            document: document, request: 4,
            generation: 9, maximumCachePayloadBytes: 0)
        let close = try await connection.closeSession(request: 5, generation: 9)
        XCTAssertEqual(base.operation, 1)
        XCTAssertEqual(session.operation, 9)
        XCTAssertEqual(preview.boundaryStatus, 0)
        XCTAssertEqual(facts.recordedBudget, UInt64.max)
        for receipt in [base, session, reset, close] {
            XCTAssertEqual(receipt.observedProcess, 42)
            XCTAssertEqual(receipt.connectionGeneration, 9)
            XCTAssertEqual(receipt.status, 0)
        }
        XCTAssertEqual(preview.observedProcess, 42)
        let zeroDocument = UUID(uuid: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        do {
            _ = try await connection.resetSession(
                document: zeroDocument, request: 6,
                generation: 9, maximumCachePayloadBytes: 0)
            XCTFail("Invalid scope must reject before RPC")
        } catch WorkerXPCTransportError.invalidRequest {}
        _ = try await connection.invalidate()
    }

}

// SPDX-License-Identifier: MIT
import Foundation
import XCTest

@testable import Dust3DWorkerXPC

@MainActor
final class WorkerXPCHandshakeTests: XCTestCase {
    func testShouldBindPeerBeforeAnyJobAndKeepGenerationStable() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let first = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        XCTAssertEqual(first.process, 42)
        XCTAssertEqual(first.observedProcess, 42)
        XCTAssertEqual(first.connectionGeneration, 9)
        XCTAssertTrue(first.sandboxEntitlementVerified)
        XCTAssertEqual(first.admissionStatus, 0)
        let repeated = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        XCTAssertEqual(repeated.process, first.process)
        let job = try await connection.closeSession(request: 1, generation: 9)
        XCTAssertEqual(job.observedProcess, first.observedProcess)
        _ = try await connection.invalidate()
        // Captured identity remains owned even after connection invalidation.
        XCTAssertEqual(first.observedProcess, 42)
    }

    func testShouldRetainOriginalRejectedHandshakeFieldsAndRetire() async throws {
        for (process, sandbox, admission): (Int32, Bool, UInt32) in [
            (43, true, 0), (0, true, 0), (42, false, 0), (42, true, 4),
        ] {
            let facts = FakeXPCFacts()
            facts.configureHandshake(process: process, sandbox: sandbox, admission: admission)
            let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
            do {
                _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
                XCTFail("Invalid handshake accepted")
            } catch WorkerXPCTransportError.handshakeRejected(let receipt) {
                XCTAssertEqual(receipt.process, process)
                XCTAssertEqual(receipt.observedProcess, 42)
                XCTAssertEqual(receipt.sandboxEntitlementVerified, sandbox)
                XCTAssertEqual(receipt.admissionStatus, admission)
                XCTAssertEqual(receipt.connectionGeneration, 9)
            }
            do {
                _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
                XCTFail("Rejected lifetime reused")
            } catch WorkerXPCTransportError.retired {}
        }
    }
    func testShouldRejectMissingIndependentPeerWithoutAcceptingServerPID() async throws {
        let facts = FakeXPCFacts()
        facts.process = 0
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Missing OS peer accepted")
        } catch WorkerXPCTransportError.missingPeer {}
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Retired lifetime reused")
        } catch WorkerXPCTransportError.retired {}
    }
    func testShouldRejectGenerationChangeWithoutReplacingBoundLifetime() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        do {
            _ = try await connection.handshake(generation: 0, timeoutMilliseconds: 5000)
            XCTFail("Zero generation accepted")
        } catch WorkerXPCTransportError.invalidGeneration {}
        _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        do {
            _ = try await connection.handshake(generation: 10, timeoutMilliseconds: 5000)
            XCTFail("Generation changed")
        } catch WorkerXPCTransportError.generationMismatch {}
        let retained = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        XCTAssertEqual(retained.connectionGeneration, 9)
        _ = try await connection.invalidate()
    }
    func testShouldRejectChangedIndependentPeerAfterHandshake() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let captured = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        facts.process = 43
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("OS peer rebound")
        } catch WorkerXPCTransportError.peerChanged(let expected, let observed) {
            XCTAssertEqual(expected, 42)
            XCTAssertEqual(observed, 43)
        }
        XCTAssertEqual(captured.observedProcess, 42)
    }
    func testShouldCorrelateDuplicateHandshakeBeforeNextRPC() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .duplicate
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        let job = try await connection.closeSession(request: 1, generation: 9)
        XCTAssertEqual(job.request, 1)
        XCTAssertEqual(job.observedProcess, 42)
        _ = try await connection.invalidate()
    }
    func testShouldRetainPendingHandshakeOnCallerCancellationAndRejectOverlap() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .hold
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 5000) }
        await facts.waitUntilHeld()
        pending.cancel()
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Overlap accepted")
        } catch WorkerXPCTransportError.busy {}
        do {
            _ = try await connection.invalidate()
            XCTFail("Pending handshake invalidated")
        } catch WorkerXPCTransportError.busy {}
        facts.release()
        let captured = try await pending.value
        XCTAssertEqual(captured.observedProcess, 42)
        _ = try await connection.invalidate()
    }
    func testShouldKeepCapturedIdentityAfterOriginalControlFailure() async throws {
        let facts = FakeXPCFacts()
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let captured = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        facts.mode = .remote
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Expected original remote cause")
        } catch WorkerXPCTransportError.remote(let cause) {
            XCTAssertEqual((cause as NSError).domain, "FakeXPC")
            XCTAssertEqual((cause as NSError).code, -18)
        }
        facts.process = 0
        _ = try await connection.invalidate()
        XCTAssertEqual(captured.observedProcess, 42)
        XCTAssertEqual(captured.connectionGeneration, 9)
    }
    func testShouldRetireInterruptedHandshakeAndIgnoreLateReply() async throws {
        let facts = FakeXPCFacts()
        facts.mode = .hold
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let pending = Task { try await connection.handshake(generation: 9, timeoutMilliseconds: 5000) }
        await facts.waitUntilHeld()
        facts.interrupt()
        do {
            _ = try await pending.value
            XCTFail("Interrupted handshake succeeded")
        } catch WorkerXPCTransportError.connectionInterrupted {}
        facts.release()
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Interrupted lifetime reused")
        } catch WorkerXPCTransportError.retired {}
    }

    func testShouldFailMissingProxyHandshakeWithoutHoldingLifetimeOpen() async throws {
        let connection = WorkerXPCConnection(factory: { MissingChannel() })
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Missing proxy accepted")
        } catch WorkerXPCTransportError.missingProxy {}
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Missing-proxy lifetime reused")
        } catch WorkerXPCTransportError.retired {}
        _ = try await connection.invalidate()
    }
    func testShouldRetainStableServiceIncarnationAcrossFreshHandshakeBarrier() async throws {
        let facts = FakeXPCFacts()
        facts.configureInstance(high: 7, low: 11)
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let first = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        let barrier = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        XCTAssertEqual(first.instance.high, 7)
        XCTAssertEqual(first.instance.low, 11)
        XCTAssertEqual(barrier.instance, first.instance)
        _ = try await connection.invalidate()
    }
    func testShouldRejectChangedIncarnationEvenWhenOperatingSystemPIDMatches() async throws {
        let facts = FakeXPCFacts()
        facts.configureInstance(high: 7, low: 11)
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        let captured = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
        facts.configureInstance(high: 7, low: 12)
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("PID equality cannot replace process incarnation correlation")
        } catch WorkerXPCTransportError.handshakeInstanceChanged(let expected, let receipt) {
            XCTAssertEqual(expected, captured.instance)
            XCTAssertEqual(receipt.instance.low, 12)
            XCTAssertEqual(receipt.observedProcess, captured.observedProcess)
        }
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Rebound lifetime must stay retired")
        } catch WorkerXPCTransportError.retired {
        }
    }
    func testShouldRejectZeroIncarnationWithOriginalHandshakeReceipt() async throws {
        let facts = FakeXPCFacts()
        facts.configureInstance(high: 0, low: 0)
        let connection = WorkerXPCConnection(factory: { FakeXPCChannel(facts) })
        do {
            _ = try await connection.handshake(generation: 9, timeoutMilliseconds: 5000)
            XCTFail("Missing process incarnation must not authenticate")
        } catch WorkerXPCTransportError.handshakeRejected(let receipt) {
            XCTAssertEqual(receipt.instance.high, 0)
            XCTAssertEqual(receipt.instance.low, 0)
            XCTAssertEqual(receipt.observedProcess, 42)
        }
    }

}

// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import XCTest

@testable import Dust3DWorkerService

/// Synthetic receipt values only. Never invokes the real resource bootstrap.
final class WorkerServiceResourceBootstrapTests: XCTestCase {
    private func policy() throws -> WorkerServiceResourcePolicy {
        try WorkerServiceResourcePolicy(
            cpuSeconds: .init(soft: 100, hard: 200), fileBytes: .init(soft: 1024, hard: 2048),
            descriptors: .init(soft: 128, hard: 256), minimumDescriptorCeiling: 64)
    }
    private func ready() -> Dust3DWorkerResourceResult {
        var result = Dust3DWorkerResourceResult()
        result.abi_version = 1
        result.status = DUST3D_WORKER_RESOURCE_READY
        result.runtime = 1
        result.application_started = 1
        result.applied_count = 4
        result.verified_count = 4
        result.effective = (
            .init(soft: 50, hard: 100), .init(soft: 512, hard: 1024), .init(soft: 0, hard: 0),
            .init(soft: 96, hard: 128)
        )
        return result
    }
    func testShouldAcceptOnlyFullyVerifiedEffectiveCapsIncludingStricterInheritance() throws {
        let receipt = try WorkerServicePreparedResources(result: ready(), policy: policy())
        XCTAssertEqual(receipt.cpuSeconds, .init(soft: 50, hard: 100))
        XCTAssertEqual(receipt.fileBytes, .init(soft: 512, hard: 1024))
        XCTAssertEqual(receipt.coreBytes, .init(soft: 0, hard: 0))
        XCTAssertEqual(receipt.descriptors, .init(soft: 96, hard: 128))
    }
    func testShouldRejectIncompleteOrInconsistentSuccessWithoutAnySystemCalls() throws {
        let mutations: [(inout Dust3DWorkerResourceResult) -> Void] = [
            { $0.abi_version = 2 }, { $0.runtime = 0 }, { $0.application_started = 0 },
            { $0.applied_count = 3 }, { $0.verified_count = 3 },
            { $0.operation = DUST3D_WORKER_RESOURCE_OPERATION_VERIFY },
            { $0.has_detail_resource = 1 }, { $0.system_error = 13 }, { $0.detail_code = 4 },
            { $0.effective.0.soft = 0 }, { $0.effective.0.hard = 201 },
            { $0.effective.1.soft = 2049 }, { $0.effective.2.hard = 1 },
            { $0.effective.3.soft = 63 }, { $0.effective.3.hard = 257 },
        ]
        for mutate in mutations {
            var result = ready()
            mutate(&result)
            XCTAssertThrowsError(try WorkerServicePreparedResources(result: result, policy: policy()))
        }
    }
    func testShouldRetainBoundedOriginalDiagnosticBytesAndExplicitTruncation() {
        var result = ready()
        result.message_truncated = 1
        withUnsafeMutableBytes(of: &result.message) { bytes in
            bytes.initializeMemory(as: UInt8.self, repeating: 255)
        }
        let observation = WorkerServiceResourceObservation(result)
        XCTAssertEqual(observation.messageBytes, Array(repeating: 255, count: 256))
        XCTAssertTrue(observation.messageTruncated)
        XCTAssertEqual(observation.runtime, 1)
        XCTAssertEqual(observation.abiVersion, 1)
        XCTAssertNil(observation.resource)
    }
    func testShouldPreserveOriginalFailureAndPartialProgressInsteadOfReturningProof() throws {
        var result = ready()
        result.status = DUST3D_WORKER_RESOURCE_SYSTEM_FAILED
        result.operation = DUST3D_WORKER_RESOURCE_OPERATION_APPLY
        result.has_detail_resource = 1
        result.detail_resource = 1
        result.system_error = 1
        result.applied_count = 1
        result.verified_count = 0
        do {
            _ = try WorkerServicePreparedResources(result: result, policy: policy())
            XCTFail("Partial preparation cannot authorize service work")
        } catch WorkerServiceResourceBootstrapError.setup(let failure) {
            XCTAssertEqual(failure.status, DUST3D_WORKER_RESOURCE_SYSTEM_FAILED.rawValue)
            XCTAssertEqual(failure.operation, DUST3D_WORKER_RESOURCE_OPERATION_APPLY.rawValue)
            XCTAssertEqual(failure.resource, 1)
            XCTAssertEqual(failure.systemError, 1)
            XCTAssertEqual(failure.appliedCount, 1)
            XCTAssertEqual(failure.verifiedCount, 0)
            XCTAssertTrue(failure.applicationStarted)
        }
    }
}

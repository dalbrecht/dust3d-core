// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation
import XCTest

@testable import Dust3DWorkerService

final class WorkerServiceFailureMetadataTests: XCTestCase {
    func testShouldLogOriginalJobAttemptAndEveryScalarDeadlineCause() {
        let cases: [(WorkerServiceJobDeadlineError, String)] = [
            (.invalidBudget(milliseconds: 0), "jobPolicyMilliseconds=0"),
            (.clockOverflow(start: 100, milliseconds: 2), "jobClockOverflowStart=100 milliseconds=2"),
            (.clockReversed(previous: 300, observed: 200), "jobClockReversedPrevious=300 observed=200"),
            (.expired(start: 100, deadline: 200, observed: 300), "jobExpiredStart=100 deadline=200 observed=300"),
        ]
        for (cause, text) in cases {
            XCTAssertEqual(WorkerServiceFailureMetadata(cause).description, text)
            let failure = WorkerServiceJobFailure(
                ownerGeneration: 2, requestIdentity: 3, leaseGeneration: 4, budgetMilliseconds: 5, cause: cause)
            XCTAssertEqual(
                WorkerServiceFailureMetadata(failure).description,
                "jobOwner=2 request=3 lease=4 budgetMilliseconds=5 " + text)
        }
    }
    private enum OriginalCause: Error { case original }
    func testShouldPreserveNativeClassificationWithoutDocumentDiagnosticBytes() {
        var receipt = Dust3DWorkerDecodeResult()
        receipt.status = DUST3D_WORKER_DECODE_LIMIT_EXCEEDED
        receipt.detail_source = DUST3D_WORKER_DETAIL_RUNTIME
        receipt.detail_code = 19
        receipt.message_truncated = 1
        withUnsafeMutableBytes(of: &receipt.message) { bytes in
            bytes.copyBytes(from: Data("private document text\0".utf8))
        }
        let original = WorkerSessionBoundaryError(receipt)
        let metadata = WorkerServiceFailureMetadata(original).description
        XCTAssertEqual(metadata, "status=\(original.status) source=\(original.source) code=19 truncated=true")
        XCTAssertFalse(metadata.contains("private document"))
        XCTAssertEqual(original.cause, Data("private document text".utf8))
    }
    func testShouldLogResourceFailureProgressWithoutDiagnosticBytes() {
        var result = Dust3DWorkerResourceResult()
        result.abi_version = 1
        result.status = DUST3D_WORKER_RESOURCE_SYSTEM_FAILED
        result.operation = DUST3D_WORKER_RESOURCE_OPERATION_APPLY
        result.runtime = 1
        result.has_detail_resource = 1
        result.detail_resource = 3
        result.system_error = 13
        result.application_started = 1
        result.applied_count = 3
        withUnsafeMutableBytes(of: &result.message) { bytes in
            bytes.copyBytes(from: Data("original private setup text\0".utf8))
        }
        let observation = WorkerServiceResourceObservation(result)
        let cause = WorkerServiceResourceBootstrapError.setup(observation)
        let text = WorkerServiceFailureMetadata(cause).description
        XCTAssertEqual(
            text,
            "resourceStatus=2 operation=2 runtime=1 resource=3 code=0 errno=13 started=true applied=3 verified=0 truncated=false"
        )
        XCTAssertEqual(observation.message, "original private setup text")
        XCTAssertFalse(text.contains("private"))
    }
    func testShouldDescribePurePolicyAndInconsistentReceiptCauses() {
        let request = WorkerServiceResourceBudget(soft: 9, hard: 7)
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceResourcePolicyError.field("cpuSoftSeconds")).description,
            "resourcePolicyField=cpuSoftSeconds")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(
                WorkerServiceResourcePolicyError.budget(resource: .cpuSeconds, requested: request)
            ).description,
            "resourcePolicyBudget=cpuSeconds soft=9 hard=7")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(
                WorkerServiceResourcePolicyError.descriptorFloor(minimum: 10, requested: request)
            ).description,
            "resourcePolicyDescriptorFloor=10 soft=9 hard=7")
        let observation = WorkerServiceResourceObservation(Dust3DWorkerResourceResult())
        let description = WorkerServiceFailureMetadata(
            WorkerServiceResourceBootstrapError.receipt(field: "progress", observation: observation)
        ).description
        XCTAssertEqual(
            description,
            "resourceReceiptField=progress resourceStatus=0 operation=0 runtime=0 resource=none code=0 errno=0 started=false applied=0 verified=0 truncated=false"
        )
    }
    func testShouldDescribeTypedPreflightMetadataAndPreserveUnknownErrorType() {
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceBundleError.field("Info.plist")).description,
            "bundleField=Info.plist")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceBootstrapError.signingRequirement(-7)).description,
            "signingRequirementStatus=-7")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceBootstrapError.emptyHostRequirement).description,
            "emptyHostRequirement")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceBootstrapError.sandboxPreflight("query unavailable")).description,
            "sandboxPreflight=query unavailable")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(WorkerServiceBootstrapError.invalidRequest("opaque bytes")).description,
            "invalidRequest")
        XCTAssertEqual(
            WorkerServiceFailureMetadata(OriginalCause.original).description,
            "type=\(String(reflecting: OriginalCause.self))")
    }
}

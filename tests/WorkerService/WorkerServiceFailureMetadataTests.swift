// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation
import XCTest

@testable import Dust3DWorkerService

final class WorkerServiceFailureMetadataTests: XCTestCase {
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

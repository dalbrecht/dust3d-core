// SPDX-License-Identifier: MIT
import Dust3DWorkerService
import XCTest

final class WorkerServiceConfigurationTests: XCTestCase {
    func testShouldPreserveExplicitHostRequirement() throws {
        let expression = "cdhash H\"0000000000000000000000000000000000000000\""
        let configuration = try WorkerServiceConfiguration(hostSigningRequirement: expression)
        XCTAssertEqual(configuration.hostSigningRequirement, expression)
    }
    func testShouldRejectEmptyRequirementBeforeSecurityCompilation() {
        XCTAssertThrowsError(try WorkerServiceConfiguration(hostSigningRequirement: "")) { error in
            guard case WorkerServiceBootstrapError.emptyHostRequirement = error else {
                return XCTFail("Unexpected error: \(error)")
            }
        }
    }
}

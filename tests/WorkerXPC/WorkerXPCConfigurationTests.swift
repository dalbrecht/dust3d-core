// SPDX-License-Identifier: MIT
import Dust3DWorkerXPC
import XCTest

final class WorkerXPCConfigurationTests: XCTestCase {
    func testConfigurationRetainsExplicitServiceAndRequirement() throws {
        let requirement = "cdhash H\"0000000000000000000000000000000000000000\""
        let configuration = try WorkerXPCConfiguration(
            serviceIdentifier: "org.example.worker", signingRequirement: requirement)
        XCTAssertEqual(configuration.serviceIdentifier, "org.example.worker")
        XCTAssertEqual(configuration.signingRequirement, requirement)
    }
    func testInvalidServiceAndEmptyRequirementRejectBeforeConnection() {
        for identifier in ["", "single", "org..worker", "org.example/worker"] {
            XCTAssertThrowsError(
                try WorkerXPCConfiguration(serviceIdentifier: identifier, signingRequirement: "unused"))
        }
        XCTAssertThrowsError(
            try WorkerXPCConfiguration(serviceIdentifier: "org.example.worker", signingRequirement: ""))
    }
}

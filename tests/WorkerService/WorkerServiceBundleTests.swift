// SPDX-License-Identifier: MIT
import Dust3DWorkerService
import Foundation
import XCTest

final class WorkerServiceBundleTests: XCTestCase {
    private let bundleURL = URL(fileURLWithPath: "/example/org.dust3d.worker.xpc")
    private var valid: [String: Any] {
        [
            "CFBundleIdentifier": "org.dust3d.worker", "CFBundleExecutable": "dust3d-xpc-service",
            "CFBundlePackageType": "XPC!", "CFBundleVersion": "1", "CFBundleInfoDictionaryVersion": "6.0",
            "XPCService": ["ServiceType": "Application", "RunLoopType": "dispatch_main"],
            "Dust3DWorkerConfigurationVersion": "3",
            "Dust3DWorkerJobWallMilliseconds": "300000",
            "Dust3DWorkerResourcePolicy": [
                "cpuSoftSeconds": "300", "cpuHardSeconds": "600", "fileSoftBytes": "67108864",
                "fileHardBytes": "134217728", "descriptorSoftCeiling": "256", "descriptorHardCeiling": "512",
                "minimumDescriptorCeiling": "128",
            ],
            "Dust3DWorkerHostRequirement": "cdhash H\"0000000000000000000000000000000000000000\"",
        ]
    }
    func testShouldPreserveExplicitBundleIdentityAndHostRequirement() throws {
        let value = try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: valid)
        XCTAssertEqual(value.jobBudget.milliseconds, 300000)
        XCTAssertEqual(value.serviceIdentifier, "org.dust3d.worker")
        XCTAssertEqual(value.resourcePolicy.cpuSeconds, .init(soft: 300, hard: 600))
        XCTAssertEqual(value.resourcePolicy.minimumDescriptorCeiling, 128)
        XCTAssertEqual(value.hostSigningRequirement, valid["Dust3DWorkerHostRequirement"] as? String)
    }
    func testShouldRejectAbsentLegacyOrNoncanonicalJobWallPolicyBeforeSystemCalls() {
        for invalid: Any in ["0", "0300", "3600001", "-1", "1.5", " 300", "300 ", "٣٠٠", 300, true] {
            var values = valid
            values["Dust3DWorkerJobWallMilliseconds"] = invalid
            XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: values))
        }
        var legacy = valid
        legacy["Dust3DWorkerConfigurationVersion"] = "2"
        XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: legacy))
    }
    func testShouldAcceptDottedVersionAndAsciiIdentifierVariants() throws {
        var values = valid
        values["CFBundleVersion"] = "1.2.3"
        values["CFBundleIdentifier"] = "org.dust3d.Worker-2"
        let url = URL(fileURLWithPath: "/example/org.dust3d.Worker-2.xpc")
        let metadata = try WorkerServiceBundleMetadata(bundleURL: url, infoDictionary: values)
        XCTAssertEqual(metadata.serviceIdentifier, "org.dust3d.Worker-2")
    }
    func testShouldRejectMissingOrWrongTypedFieldsBeforeSecurity() {
        for key in valid.keys {
            var missing = valid
            missing.removeValue(forKey: key)
            XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: missing), key)
            var wrong = valid
            wrong[key] = false
            XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: wrong), key)
        }
    }
    func testShouldRejectNestedNumericAndIncompleteResourcePolicyBeforeSecurity() {
        for resourceFields: [String: Any] in [
            ["cpuSoftSeconds": 300], ["cpuSoftSeconds": true], [:],
            ["unknown": "300"], ["cpuSoftSeconds": "0300"],
        ] {
            var values = valid
            values["Dust3DWorkerResourcePolicy"] = resourceFields
            XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: values))
        }
    }
    func testShouldRejectUnpackagedOrMismatchedBundleBeforeSecurity() {
        for path in ["/example/service", "/example/org.other.worker.xpc", "/example/org.dust3d.worker.app"] {
            XCTAssertThrowsError(
                try WorkerServiceBundleMetadata(bundleURL: URL(fileURLWithPath: path), infoDictionary: valid))
        }
    }
    func testShouldRejectUnsupportedPoliciesBeforeSecurity() {
        let replacements: [(String, Any)] = [
            ("CFBundleIdentifier", "../worker"), ("CFBundleIdentifier", ".org.worker"),
            ("CFBundleExecutable", "../dust3d-xpc-service"), ("CFBundlePackageType", "APPL"),
            ("CFBundleVersion", ""), ("CFBundleVersion", "v1"), ("CFBundleVersion", "."), ("CFBundleVersion", "1..2"),
            ("CFBundleInfoDictionaryVersion", "5.0"),
            ("Dust3DWorkerConfigurationVersion", "1"), ("Dust3DWorkerHostRequirement", ""),
            ("Dust3DWorkerHostRequirement", " \n"), ("Dust3DWorkerHostRequirement", "bad\0value"),
            ("Dust3DWorkerHostRequirement", String(repeating: "a", count: 4097)),
            ("XPCService", ["ServiceType": "Daemon", "RunLoopType": "dispatch_main"]),
            ("XPCService", ["ServiceType": "Application", "RunLoopType": "NSRunLoop"]),
        ]
        for (key, replacement) in replacements {
            var values = valid
            values[key] = replacement
            XCTAssertThrowsError(try WorkerServiceBundleMetadata(bundleURL: bundleURL, infoDictionary: values), key)
        }
    }
}

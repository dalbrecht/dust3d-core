import XCTest

// SPDX-License-Identifier: MIT
@testable import Dust3DWorkerService

/// Pure requested values only. These tests never inspect or change process limits.
final class WorkerServiceResourcePolicyTests: XCTestCase {
    func testShouldMapRequestedScalarsWithoutInvokingResourceSetup() throws {
        let policy = try WorkerServiceResourcePolicy(
            cpuSeconds: .init(soft: 11, hard: 22), fileBytes: .init(soft: 333, hard: 444),
            descriptors: .init(soft: 128, hard: 256), minimumDescriptorCeiling: 64)
        let profile = policy.nativeProfile
        XCTAssertEqual(profile.abi_version, 1)
        XCTAssertEqual(profile.cpu_seconds.soft, 11)
        XCTAssertEqual(profile.cpu_seconds.hard, 22)
        XCTAssertEqual(profile.file_bytes.soft, 333)
        XCTAssertEqual(profile.file_bytes.hard, 444)
        XCTAssertEqual(profile.descriptors.soft, 128)
        XCTAssertEqual(profile.descriptors.hard, 256)
        XCTAssertEqual(profile.minimum_descriptor_ceiling, 64)
    }
    func testShouldPreserveExplicitUnitsAndAlwaysDisableCoreDumps() throws {
        let cpu = WorkerServiceResourceBudget(soft: 300, hard: 600)
        let file = WorkerServiceResourceBudget(soft: UInt64.max - 1, hard: UInt64.max)
        let descriptors = WorkerServiceResourceBudget(soft: 128, hard: 256)
        let policy = try WorkerServiceResourcePolicy(
            cpuSeconds: cpu, fileBytes: file, descriptors: descriptors, minimumDescriptorCeiling: 64)
        XCTAssertEqual(policy.cpuSeconds, cpu)
        XCTAssertEqual(policy.fileBytes, file)
        XCTAssertEqual(policy.descriptors, descriptors)
        XCTAssertEqual(policy.minimumDescriptorCeiling, 64)
        XCTAssertEqual(policy.coreBytes, WorkerServiceResourceBudget(soft: 0, hard: 0))
        // UInt64.max is a requested scalar here, not infinity; the platform planner must reject unrepresentable caps.
        XCTAssertEqual(policy.fileBytes.hard, UInt64.max)
    }
    func testShouldRetainOriginalInvalidRequestedBudgetWithoutAnySystemCalls() {
        let normal = WorkerServiceResourceBudget(soft: 128, hard: 256)
        for resource: WorkerServiceResourceKind in [.cpuSeconds, .fileBytes, .descriptors] {
            for invalid in [
                WorkerServiceResourceBudget(soft: 0, hard: 1),
                WorkerServiceResourceBudget(soft: 2, hard: 1), WorkerServiceResourceBudget(soft: 1, hard: 0),
            ] {
                do {
                    _ = try WorkerServiceResourcePolicy(
                        cpuSeconds: resource == .cpuSeconds ? invalid : normal,
                        fileBytes: resource == .fileBytes ? invalid : normal,
                        descriptors: resource == .descriptors ? invalid : normal,
                        minimumDescriptorCeiling: 1)
                    XCTFail("Invalid requested budget must reject before any OS adapter")
                } catch WorkerServiceResourcePolicyError.budget(let kind, let original) {
                    XCTAssertEqual(kind, resource)
                    XCTAssertEqual(original, invalid)
                } catch { XCTFail("Unexpected policy cause: \(error)") }
            }
        }
    }
    func testShouldRequireAnExplicitUsableRequestedDescriptorFloor() {
        let normal = WorkerServiceResourceBudget(soft: 128, hard: 256)
        for minimum: UInt64 in [0, 129, UInt64.max] {
            do {
                _ = try WorkerServiceResourcePolicy(
                    cpuSeconds: normal, fileBytes: normal,
                    descriptors: normal, minimumDescriptorCeiling: minimum)
                XCTFail("A missing or unsatisfied startup floor cannot become a default")
            } catch WorkerServiceResourcePolicyError.descriptorFloor(let original, let budget) {
                XCTAssertEqual(original, minimum)
                XCTAssertEqual(budget, normal)
            } catch { XCTFail("Unexpected floor cause: \(error)") }
        }
    }
    private func policy() throws -> WorkerServiceResourcePolicy {
        try WorkerServiceResourcePolicy(
            cpuSeconds: WorkerServiceResourceBudget(soft: 300, hard: 600),
            fileBytes: WorkerServiceResourceBudget(soft: UInt64.max - 1, hard: UInt64.max),
            descriptors: WorkerServiceResourceBudget(soft: 128, hard: 256), minimumDescriptorCeiling: 64)
    }
    func testShouldRoundTripExactDecimalBundleFieldsWithoutPlatformConversion() throws {
        let original = try policy()
        XCTAssertEqual(
            original.fields,
            [
                "cpuSoftSeconds": "300", "cpuHardSeconds": "600",
                "fileSoftBytes": "18446744073709551614", "fileHardBytes": "18446744073709551615",
                "descriptorSoftCeiling": "128", "descriptorHardCeiling": "256", "minimumDescriptorCeiling": "64",
            ])
        XCTAssertEqual(try WorkerServiceResourcePolicy(fields: original.fields), original)
    }
    func testShouldRejectMissingUnknownAndNoncanonicalNumericFields() throws {
        let original = try policy().fields
        for key in original.keys {
            var missing = original
            missing.removeValue(forKey: key)
            XCTAssertThrowsError(try WorkerServiceResourcePolicy(fields: missing))
            for invalid in ["", "-1", "+1", " 1", "1 ", "1.0", "1e3", "01", "١", "18446744073709551616"] {
                var fields = original
                fields[key] = invalid
                do {
                    _ = try WorkerServiceResourcePolicy(fields: fields)
                    XCTFail("Numeric policy fields must be canonical ASCII UInt64 values")
                } catch WorkerServiceResourcePolicyError.field(let name) {
                    XCTAssertEqual(name, key)
                }
            }
        }
        var unsupported = original
        unsupported["residentMemoryBytes"] = "1024"
        XCTAssertThrowsError(try WorkerServiceResourcePolicy(fields: unsupported))
    }

    func testShouldPreserveDecodedZeroAsAnInvalidRequestRatherThanDefaultIt() throws {
        var fields = try policy().fields
        fields["cpuSoftSeconds"] = "0"
        do {
            _ = try WorkerServiceResourcePolicy(fields: fields)
            XCTFail("Zero CPU request cannot become an implicit limit")
        } catch WorkerServiceResourcePolicyError.budget(let resource, let original) {
            XCTAssertEqual(resource, .cpuSeconds)
            XCTAssertEqual(original.soft, 0)
            XCTAssertEqual(original.hard, 600)
        }
        fields = try policy().fields
        fields["minimumDescriptorCeiling"] = "0"
        do {
            _ = try WorkerServiceResourcePolicy(fields: fields)
            XCTFail("Zero startup floor cannot become a default")
        } catch WorkerServiceResourcePolicyError.descriptorFloor(let minimum, _) {
            XCTAssertEqual(minimum, 0)
        }
    }

}

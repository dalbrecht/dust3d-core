// SPDX-License-Identifier: MIT
import Dust3DWorkerService
import Foundation

@main
struct WorkerServiceBundleSmoke {
    static func main() throws {
        guard CommandLine.arguments.count == 2 else { throw WorkerServiceBundleError.field("test arguments") }
        let bundle = URL(fileURLWithPath: CommandLine.arguments[1], isDirectory: true)
        let data = try Data(contentsOf: bundle.appendingPathComponent("Contents/Info.plist"))
        guard let info = try PropertyListSerialization.propertyList(from: data, format: nil) as? [String: Any] else {
            throw WorkerServiceBundleError.field("Info.plist")
        }
        let metadata = try WorkerServiceBundleMetadata(bundleURL: bundle, infoDictionary: info)
        guard metadata.serviceIdentifier == "org.dust3d.worker.bundle-qa",
            metadata.hostSigningRequirement == "cdhash H\"0000000000000000000000000000000000000000\"",
            metadata.resourcePolicy.cpuSeconds == WorkerServiceResourceBudget(soft: 300, hard: 600),
            metadata.resourcePolicy.minimumDescriptorCeiling == 256, metadata.jobBudget.milliseconds == 300000
        else { throw WorkerServiceBundleError.field("test fixture identity") }
        print("Actual service module accepted staged explicit metadata; no listener or native work started")
    }
}

// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerService
import Foundation

@main
struct WorkerServiceMain {
    static func main() {
        do {
            guard CommandLine.arguments.count == 1 else { throw WorkerServiceBundleError.field("arguments") }
            guard let info = Bundle.main.infoDictionary else { throw WorkerServiceBundleError.field("Info.plist") }
            let metadata = try WorkerServiceBundleMetadata(bundleURL: Bundle.main.bundleURL, infoDictionary: info)
            let configuration = try WorkerServiceConfiguration(hostSigningRequirement: metadata.hostSigningRequirement)
            let delegate = try WorkerServiceListenerDelegate(configuration: configuration) { result in
                switch result {
                case .success: exit(EXIT_SUCCESS)
                case .failure(let cause):
                    writeFailure(operation: "retire", cause: cause)
                    exit(EXIT_FAILURE)
                }
            }
            let listener = NSXPCListener.service()
            listener.delegate = delegate
            withExtendedLifetime(delegate) { listener.resume() }
        } catch {
            writeFailure(operation: "preflight", cause: error)
            exit(EXIT_FAILURE)
        }
    }
    private static func writeFailure(operation: String, cause: any Error) {
        let metadata = WorkerServiceFailureMetadata(cause)
        FileHandle.standardError.write(Data("Dust3D worker operation=\(operation) \(metadata.description)\n".utf8))
    }
}

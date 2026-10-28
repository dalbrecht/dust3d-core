// SPDX-License-Identifier: MIT
import Darwin
import Foundation

#if canImport(Dust3DWorkerService)
    import Dust3DWorkerService
#endif

@main
struct WorkerXPCService {
    static func main() {
        do {
            let pins = try readDevelopmentPins()
            let configuration = try WorkerServiceConfiguration(
                hostSigningRequirement: developmentRequirement(pins.host))
            // Explicit trusted QA policy in this fresh service process only, never the host/test runner.
            let policy = try WorkerServiceResourcePolicy(
                cpuSeconds: .init(soft: 300, hard: 600), fileBytes: .init(soft: 67_108_864, hard: 134_217_728),
                descriptors: .init(soft: 1024, hard: 2048), minimumDescriptorCeiling: 256)
            let resources = try WorkerServicePreparedResources.prepareFreshHelper(policy: policy)
            let delegate = try WorkerServiceListenerDelegate(
                configuration: configuration, resources: resources,
                jobBudget: try WorkerServiceJobBudget(milliseconds: 300000),
                onDeadline: { _ in _exit(EXIT_FAILURE) },
                onRetired: { result in
                    switch result {
                    case .success: exit(EXIT_SUCCESS)
                    case .failure(let cause):
                        // Never log native document diagnostics; preserve typed cause in the library callback.
                        let description = "type=\(String(reflecting: type(of: cause)))"
                        FileHandle.standardError.write(Data("Static XPC retirement failed: \(description)\n".utf8))
                        exit(EXIT_FAILURE)
                    }
                })
            let listener = NSXPCListener.service()
            listener.delegate = delegate
            withExtendedLifetime(delegate) { listener.resume() }
        } catch {
            FileHandle.standardError.write(Data("Static XPC service preflight failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}

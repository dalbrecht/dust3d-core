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
            let delegate = try WorkerServiceListenerDelegate(configuration: configuration) { result in
                switch result {
                case .success: exit(EXIT_SUCCESS)
                case .failure(let cause):
                    // Never log native document diagnostics; preserve typed cause in the library callback.
                    let description = "type=\(String(reflecting: type(of: cause)))"
                    FileHandle.standardError.write(Data("Static XPC retirement failed: \(description)\n".utf8))
                    exit(EXIT_FAILURE)
                }
            }
            let listener = NSXPCListener.service()
            listener.delegate = delegate
            withExtendedLifetime(delegate) { listener.resume() }
        } catch {
            FileHandle.standardError.write(Data("Static XPC service preflight failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}

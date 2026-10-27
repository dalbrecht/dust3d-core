// SPDX-License-Identifier: MIT
import Darwin
import Foundation
import Security

let staticHostIdentifier = "org.dust3d.core.xpc-static-host.qa"
let staticServiceIdentifier = "org.dust3d.core.xpc-static-service.qa"

@objc protocol StaticWorkerJob {
    func serve(
        _ input: FileHandle, output: FileHandle, isStream: Bool, requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32
            ) ->
            Void)
    func serveSession(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, maximumWireBytes: UInt64, maximumDecodedBytes: UInt64,
        maximumSourceBytes: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void)
    func closeSession(
        _ requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void)
    func cancelQueued(_ requestIdentity: UInt64, withReply reply: @escaping @Sendable (UInt32) -> Void)
}

enum StaticFixtureError: Error {
    case invalid(String)
    case system(String, Int32)
    case security(OSStatus)
}

struct DevelopmentPins: Decodable {
    let host: String
    let service: String
}

func developmentRequirement(_ hash: String) throws -> String {
    guard hash.utf8.count == 40,
        hash.utf8.allSatisfy({ (48...57).contains($0) || (97...102).contains($0) })
    else { throw StaticFixtureError.invalid("Controller hash must be exactly 20 bytes of lower-case hex") }
    let expression = "cdhash H\"\(hash)\""
    var compiled: SecRequirement?
    let status = SecRequirementCreateWithString(expression as CFString, [], &compiled)
    guard status == errSecSuccess, compiled != nil else { throw StaticFixtureError.security(status) }
    return expression
}

func readDevelopmentPins() throws -> DevelopmentPins {
    var app = Bundle.main.bundleURL
    for _ in 0..<5 {
        if app.pathExtension == "app" { break }
        app.deleteLastPathComponent()
    }
    guard app.pathExtension == "app" else { throw StaticFixtureError.invalid("Expected owned fixture app bundle") }
    let url = app.deletingLastPathComponent().appendingPathComponent("peers.json")
    let descriptor = open(url.path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)
    guard descriptor >= 0 else { throw StaticFixtureError.system("open controller peer configuration", errno) }
    let handle = FileHandle(fileDescriptor: descriptor, closeOnDealloc: true)
    var attributes = stat()
    guard fstat(descriptor, &attributes) == 0 else { throw StaticFixtureError.system("stat peer configuration", errno) }
    guard attributes.st_uid == getuid(), attributes.st_mode & 0o777 == 0o600,
        attributes.st_mode & S_IFMT == S_IFREG, attributes.st_size > 0, attributes.st_size <= 512
    else { throw StaticFixtureError.invalid("Expected private regular controller peer configuration") }
    guard let data = try handle.read(upToCount: 513), data.count == attributes.st_size else {
        throw StaticFixtureError.invalid("Peer configuration length changed")
    }
    try handle.close()
    let pins = try JSONDecoder().decode(DevelopmentPins.self, from: data)
    _ = try developmentRequirement(pins.host)
    _ = try developmentRequirement(pins.service)
    return pins
}

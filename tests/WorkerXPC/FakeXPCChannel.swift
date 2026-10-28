import Foundation

// SPDX-License-Identifier: MIT
@testable import Dust3DWorkerXPC

/// All mutable facts and continuations are protected by lock; callbacks resume outside it.
final class FakeXPCFacts: @unchecked Sendable {
    enum Mode: Sendable { case success, duplicate, hold, remote, oversized, failureReceipt }
    private let lock = NSLock()
    private var selectedMode: Mode = .success
    private var observed: Int32 = 42
    private var held: (@Sendable () -> Void)?
    private var ready: CheckedContinuation<Void, Never>?
    private var invalidated: (@Sendable () -> Void)?
    private var interrupted: (@Sendable () -> Void)?
    private var greeting: (Int32, Bool, UInt32, UInt64, UInt64) = (42, true, 0, 1, 1)
    var handshakeFields: (Int32, Bool, UInt32, UInt64, UInt64) { lock.withLock { greeting } }
    func configureHandshake(process: Int32 = 42, sandbox: Bool = true, admission: UInt32 = 0) {
        lock.withLock { greeting = (process, sandbox, admission, greeting.3, greeting.4) }
    }
    func configureInstance(high: UInt64, low: UInt64) {
        lock.withLock { greeting = (greeting.0, greeting.1, greeting.2, high, low) }
    }
    private var stream: Bool?
    var recordedStream: Bool? { lock.withLock { stream } }
    func recordStream(_ value: Bool) { lock.withLock { stream = value } }
    private var rawBudget: UInt64?
    var mode: Mode {
        get { lock.withLock { selectedMode } }
        set { lock.withLock { selectedMode = newValue } }
    }
    var process: Int32 {
        get { lock.withLock { observed } }
        set { lock.withLock { observed = newValue } }
    }
    var recordedBudget: UInt64? { lock.withLock { rawBudget } }
    func record(_ value: UInt64) { lock.withLock { rawBudget = value } }
    func install(invalidated: @escaping @Sendable () -> Void, interrupted: @escaping @Sendable () -> Void) {
        lock.withLock {
            self.invalidated = invalidated
            self.interrupted = interrupted
        }
    }
    func interrupt() {
        let callback = lock.withLock { interrupted }
        callback?()
    }
    func invalidate() {
        let callback = lock.withLock { invalidated }
        callback?()
    }
    func deliver(_ reply: @escaping @Sendable () -> Void) {
        let selected = mode
        if selected == .remote { return }
        if selected == .hold {
            let pending = lock.withLock {
                held = reply
                let pending = ready
                ready = nil
                return pending
            }
            pending?.resume()
        } else {
            reply()
            if selected == .duplicate { reply() }
        }
    }
    func waitUntilHeld() async {
        await withCheckedContinuation { continuation in
            let isHeld = lock.withLock {
                if held != nil { return true }
                ready = continuation
                return false
            }
            if isHeld { continuation.resume() }
        }
    }
    func release() {
        let callback = lock.withLock {
            let callback = held
            held = nil
            return callback
        }
        callback?()
    }
}
final class FakeXPCChannel: WorkerXPCChannel {
    let facts: FakeXPCFacts
    init(_ facts: FakeXPCFacts) { self.facts = facts }
    var processIdentifier: Int32 { facts.process }
    func installHandlers(invalidated: @escaping @Sendable () -> Void, interrupted: @escaping @Sendable () -> Void) {
        facts.install(invalidated: invalidated, interrupted: interrupted)
    }
    func resume() {}
    func invalidate() { facts.invalidate() }
    func proxy(errorHandler: @escaping @Sendable (any Error) -> Void) -> (any Dust3DWorkerXPCProtocol)? {
        if facts.mode == .remote {
            errorHandler(NSError(domain: "FakeXPC", code: -18, userInfo: [NSLocalizedDescriptionKey: "Original cause"]))
        }
        return FakeXPCProxy(facts)
    }
}
private final class FakeXPCProxy: NSObject, Dust3DWorkerXPCProtocol {
    let facts: FakeXPCFacts
    init(_ facts: FakeXPCFacts) { self.facts = facts }
    func handshake(withReply reply: @escaping @Sendable (Int32, Bool, UInt32, UInt64, UInt64) -> Void) {
        let fields = facts.handshakeFields
        facts.deliver { reply(fields.0, fields.1, fields.2, fields.3, fields.4) }
    }
    func serve(
        _ input: FileHandle, output: FileHandle, isStream: Bool, requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32
            ) ->
            Void
    ) {
        facts.deliver { reply(0, 1, requestIdentity, 0, 0, 0, 0, 0, 42, true, 0, 0) }
    }
    func serveSession(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, maximumWireBytes: UInt64, maximumDecodedBytes: UInt64,
        maximumSourceBytes: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void
    ) {
        facts.recordStream(isStream)
        facts.deliver { reply(0, 9, requestIdentity, 0, 0, 0, 0, 0, 42, true, 0, 0, Data()) }
    }
    func servePreview(
        _ input: FileHandle, output: FileHandle, isStream: Bool, document: NSUUID,
        requestIdentity: UInt64, revision: UInt64, connectionGeneration: UInt64,
        maximumCachePayloadBytes: UInt64, archiveBytes: UInt64, maximumIndexedBytes: UInt64,
        maximumSourceBytes: UInt64, maximumInputTransferBytes: UInt64, inputChunkBytes: UInt32,
        maximumOutputTransferBytes: UInt64, outputChunkBytes: UInt32,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, UInt32, Data
            ) -> Void
    ) {
        facts.recordStream(isStream)
        facts.record(maximumIndexedBytes)
        let mode = facts.mode
        let cause = mode == .oversized ? Data(repeating: 1, count: 512) : mode == .failureReceipt ? Data([255]) : Data()
        facts.deliver {
            if mode == .failureReceipt {
                reply(7, requestIdentity, 5, 2, 33, -4, 1, 52, true, 6, 8, 3, cause)
            } else {
                reply(0, requestIdentity, 0, 0, 0, 0, 0, 42, true, 0, 0, 0, cause)
            }
        }
    }
    func resetSession(
        _ document: NSUUID, requestIdentity: UInt64, connectionGeneration: UInt64, maximumCachePayloadBytes: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void
    ) {
        facts.deliver { reply(0, 0, requestIdentity, 0, 0, 0, 0, 0, 42, true, 0, 0, Data()) }
    }
    func closeSession(
        _ requestIdentity: UInt64,
        withReply reply:
            @escaping @Sendable (
                UInt32, UInt32, UInt64, Int32, UInt32, UInt32, Int32, UInt8, Int32, Bool, Int32, UInt32, Data
            ) -> Void
    ) {
        facts.deliver { reply(0, 0, requestIdentity, 0, 0, 0, 0, 0, 42, true, 0, 0, Data()) }
    }
    func cancelQueued(_ requestIdentity: UInt64, withReply reply: @escaping @Sendable (UInt32) -> Void) {
        reply(0)
    }
}

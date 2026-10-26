// SPDX-License-Identifier: MIT
import Foundation
import os

private enum FakeError: Error { case creation, serving, closing }
private struct Facts {
    var events: [String] = []
    var shouldFailCreation = false
    var shouldFailClose = false
    var shouldFailServing = false
}
private struct FakeBackend: WorkerServiceSessionBackend {
    let facts: OSAllocatedUnfairLock<Facts>
    func create(_ scope: WorkerServiceSessionScope) throws -> Int {
        try facts.withLock {
            $0.events.append("create")
            if $0.shouldFailCreation { throw FakeError.creation }
            return 7
        }
    }
    func serve(_ owner: Int, scope: WorkerServiceSessionScope, job: Int) throws -> Int {
        try facts.withLock {
            $0.events.append("serve")
            if $0.shouldFailServing { throw FakeError.serving }
        }
        return owner + job
    }
    func reset(_ owner: Int, scope: WorkerServiceSessionScope) { facts.withLock { $0.events.append("reset") } }
    func close(_ owner: Int) throws {
        try facts.withLock {
            $0.events.append("close")
            if $0.shouldFailClose { throw FakeError.closing }
        }
    }
    func destroy(_ owner: Int) { facts.withLock { $0.events.append("destroy") } }
}
private func require(_ value: Bool) throws {
    if !value { throw CocoaError(.coderInvalidValue) }
}
@main
private enum WorkerSessionLifecycleSmoke {
    static func useAndRelease(_ scope: WorkerServiceSessionScope, _ facts: OSAllocatedUnfairLock<Facts>) throws {
        let temporary = WorkerServiceSession(backend: FakeBackend(facts: facts))
        try require(try temporary.serve(scope: scope, job: 1) == 8)
    }
    static func main() throws {
        let scope = try WorkerServiceSessionScope(
            document: UUID(), connectionGeneration: 9, maximumCachePayloadBytes: 4096)
        let facts = OSAllocatedUnfairLock(initialState: Facts())
        let owner = WorkerServiceSession(backend: FakeBackend(facts: facts))
        try require(facts.withLock { $0.events.isEmpty })
        do {
            try owner.reset(scope: scope)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.uninitialized {}
        try require(try owner.serve(scope: scope, job: 1) == 8)
        try require(try owner.serve(scope: scope, job: 2) == 9)
        let foreign = try WorkerServiceSessionScope(
            document: UUID(), connectionGeneration: 9, maximumCachePayloadBytes: 4096)
        do {
            _ = try owner.serve(scope: foreign, job: 3)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.changedScope {}
        try owner.reset(scope: scope)
        for changed in [
            try WorkerServiceSessionScope(
                document: scope.document, connectionGeneration: 10, maximumCachePayloadBytes: 4096),
            try WorkerServiceSessionScope(
                document: scope.document, connectionGeneration: 9, maximumCachePayloadBytes: 0),
        ] {
            do {
                _ = try owner.serve(scope: changed, job: 3)
                throw CocoaError(.coderInvalidValue)
            } catch WorkerServiceSessionError.changedScope {}
        }
        do {
            try owner.reset(scope: foreign)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.changedScope {}
        try owner.close()
        try owner.close()
        do {
            try owner.reset(scope: scope)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.closed {}
        do {
            _ = try owner.serve(scope: scope, job: 4)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.closed {}
        try require(facts.withLock { $0.events == ["create", "serve", "serve", "reset", "close", "destroy"] })
        let failures = OSAllocatedUnfairLock(initialState: Facts(shouldFailCreation: true))
        let retry = WorkerServiceSession(backend: FakeBackend(facts: failures))
        do {
            _ = try retry.serve(scope: scope, job: 1)
            throw CocoaError(.coderInvalidValue)
        } catch FakeError.creation {}
        do {
            _ = try retry.serve(scope: foreign, job: 2)
            throw CocoaError(.coderInvalidValue)
        } catch WorkerServiceSessionError.changedScope {}
        failures.withLock {
            $0.shouldFailCreation = false
            $0.shouldFailClose = true
        }
        try require(try retry.serve(scope: scope, job: 3) == 10)
        do {
            try retry.close()
            throw CocoaError(.coderInvalidValue)
        } catch FakeError.closing {}
        try retry.close()
        try require(failures.withLock { $0.events == ["create", "create", "serve", "close", "destroy"] })
        let emptyFacts = OSAllocatedUnfairLock(initialState: Facts())
        let empty = WorkerServiceSession(backend: FakeBackend(facts: emptyFacts))
        try empty.close()
        try require(emptyFacts.withLock { $0.events.isEmpty })
        let fallbackFacts = OSAllocatedUnfairLock(initialState: Facts(shouldFailServing: true))
        var fallback: WorkerServiceSession<FakeBackend>? = WorkerServiceSession(
            backend: FakeBackend(facts: fallbackFacts))
        guard let retained = fallback else { throw CocoaError(.coderInvalidValue) }
        do {
            _ = try retained.serve(scope: scope, job: 1)
            throw CocoaError(.coderInvalidValue)
        } catch FakeError.serving {}
        fallbackFacts.withLock { $0.shouldFailServing = false }
        try require(try retained.serve(scope: scope, job: 2) == 9)
        try retained.close()
        try require(fallbackFacts.withLock { $0.events == ["create", "serve", "serve", "close", "destroy"] })
        fallback = nil
        // Explicitly check deinitialization in a separate lexical lifetime below.
        let destructionFacts = OSAllocatedUnfairLock(initialState: Facts())
        try useAndRelease(scope, destructionFacts)
        try require(destructionFacts.withLock { $0.events == ["create", "serve", "destroy"] })
        let zero = UUID(uuid: (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0))
        let invalidScopes: [(UUID, UInt64, UInt64)] = [
            (zero, UInt64(9), UInt64(0)), (scope.document, 0, 0), (scope.document, 9, 256 * 1024 * 1024 + 1),
        ]
        for values in invalidScopes {
            do {
                _ = try WorkerServiceSessionScope(
                    document: values.0, connectionGeneration: values.1, maximumCachePayloadBytes: values.2)
                throw CocoaError(.coderInvalidValue)
            } catch WorkerServiceSessionError.invalidScope {}
        }
        print("Fake-only connection session lifetime passed")
    }
}

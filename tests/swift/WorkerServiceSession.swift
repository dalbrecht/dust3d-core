// SPDX-License-Identifier: MIT
import Foundation
import os

/// Immutable host-selected scope, supplied only after connection authentication.
struct WorkerServiceSessionScope: Equatable, Sendable {
    let document: UUID
    let connectionGeneration: UInt64
    let maximumCachePayloadBytes: UInt64
    init(document: UUID, connectionGeneration: UInt64, maximumCachePayloadBytes: UInt64) throws {
        var bytes = document.uuid
        guard connectionGeneration > 0, maximumCachePayloadBytes <= 256 * 1024 * 1024,
            withUnsafeBytes(of: &bytes, { $0.contains { $0 != 0 } })
        else { throw WorkerServiceSessionError.invalidScope }
        self.document = document
        self.connectionGeneration = connectionGeneration
        self.maximumCachePayloadBytes = maximumCachePayloadBytes
    }
}
enum WorkerServiceSessionError: Error { case invalidScope, changedScope, closed, uninitialized }
/// Exclusive synchronous backend. Methods must not reenter the owning coordinator.
/// Fakes exercise lifetime errors without inducing native failures.
protocol WorkerServiceSessionBackend: Sendable {
    associatedtype Handle
    associatedtype Job: Sendable
    associatedtype Receipt: Sendable
    func create(_ scope: WorkerServiceSessionScope) throws -> Handle
    func serve(_ owner: Handle, scope: WorkerServiceSessionScope, job: Job) throws -> Receipt
    func reset(_ owner: Handle, scope: WorkerServiceSessionScope) throws
    func close(_ owner: Handle) throws
    func destroy(_ owner: Handle)
}
private struct WorkerSessionState<Handle> {
    var scope: WorkerServiceSessionScope?
    var handle: Handle?
    var isClosed = false
}
/// Lazy connection owner. No handles escape, and all backend calls/lifetime changes
/// share a lock. Call off UI; close waits for accepted synchronous work, never interrupts.
final class WorkerServiceSession<Backend: WorkerServiceSessionBackend>: Sendable {
    private let backend: Backend
    private let state: OSAllocatedUnfairLock<WorkerSessionState<Backend.Handle>>
    init(backend: Backend) {
        self.backend = backend
        // The backend handle may not be Sendable. It is accessed only under this lock.
        state = OSAllocatedUnfairLock(uncheckedState: WorkerSessionState())
    }
    deinit {
        state.withLock {
            if let handle = $0.handle { backend.destroy(handle) }
        }
    }
    func serve(scope: WorkerServiceSessionScope, job: Backend.Job) throws -> Backend.Receipt {
        try state.withLock {
            guard !$0.isClosed else { throw WorkerServiceSessionError.closed }
            if let bound = $0.scope, bound != scope { throw WorkerServiceSessionError.changedScope }
            // First authenticated scope remains bound even if backend creation fails.
            $0.scope = scope
            let handle: Backend.Handle
            if let existing = $0.handle {
                handle = existing
            } else {
                handle = try backend.create(scope)
                $0.handle = handle
            }
            return try backend.serve(handle, scope: scope, job: job)
        }
    }
    func reset(scope: WorkerServiceSessionScope) throws {
        try state.withLock {
            guard !$0.isClosed else { throw WorkerServiceSessionError.closed }
            guard let bound = $0.scope, let handle = $0.handle else { throw WorkerServiceSessionError.uninitialized }
            guard bound == scope else { throw WorkerServiceSessionError.changedScope }
            try backend.reset(handle, scope: scope)
        }
    }
    func close() throws {
        try state.withLock {
            guard !$0.isClosed else { return }
            $0.isClosed = true
            guard let handle = $0.handle else { return }
            $0.handle = nil
            defer { backend.destroy(handle) }
            try backend.close(handle)
        }
    }
}

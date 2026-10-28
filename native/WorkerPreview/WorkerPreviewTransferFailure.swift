// SPDX-License-Identifier: MIT
import Foundation

/// Original channel boundary receipt; diagnostic bytes are opaque data.
public struct WorkerPreviewTransferFailure: Error, Sendable {
    public enum Operation: Sendable { case archive, reply }
    public let operation: Operation
    public let status: UInt32
    public let source: UInt32
    public let code: Int32
    public let systemError: Int32
    public let stage: UInt32
    public let truncated: UInt8
    public let cause: Data
    /// Preserve raw boundary fields and opaque bytes without enum coercion.
    public init(
        operation: Operation, status: UInt32, source: UInt32, code: Int32,
        systemError: Int32, stage: UInt32, truncated: UInt8, cause: Data
    ) {
        self.operation = operation
        self.status = status
        self.source = source
        self.code = code
        self.systemError = systemError
        self.stage = stage
        self.truncated = truncated
        self.cause = cause
    }
}

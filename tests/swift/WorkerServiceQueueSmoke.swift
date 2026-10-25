// SPDX-License-Identifier: MIT
import Dust3DWorkerRuntime
import Foundation

private enum QueueSmokeError: Error { case invalid(String) }
private func require(_ value: Bool, _ message: String) throws {
    if !value { throw QueueSmokeError.invalid(message) }
}
private func write(_ value: UInt64, _ width: Int, at offset: Int, into bytes: inout [UInt8]) {
    for index in 0..<width {
        bytes[offset + index] = UInt8(truncatingIfNeeded: value >> ((width - index - 1) * 8))
    }
}
private func request(_ number: UInt64) -> Data {
    var bytes = [UInt8](repeating: 0, count: 67)
    bytes.replaceSubrange(0..<8, with: Array("DS3WORK".utf8) + [0])
    write(1, 2, at: 8, into: &bytes)
    write(64, 2, at: 10, into: &bytes)
    write(1, 2, at: 16, into: &bytes)
    write(1, 2, at: 18, into: &bytes)
    write(number, 8, at: 24, into: &bytes)
    bytes[40] = 1
    write(3, 8, at: 56, into: &bytes)
    bytes.replaceSubrange(64..<67, with: Array("ds3".utf8))
    return Data(bytes)
}
private func failure(_ request: Data) -> Data {
    var bytes = Array(request.prefix(64)) + [UInt8](repeating: 0, count: 19)
    write(3, 2, at: 16, into: &bytes)
    write(19, 8, at: 56, into: &bytes)
    write(1, 2, at: 64, into: &bytes)
    write(16, 2, at: 66, into: &bytes)
    write(3, 2, at: 68, into: &bytes)
    write(3, 8, at: 72, into: &bytes)
    bytes.replaceSubrange(80..<83, with: [120, 0, 255])
    return Data(bytes)
}
private func receipt(_ number: UInt64) -> Dust3DWorkerServiceReceipt {
    var value = Dust3DWorkerServiceReceipt()
    value.operation = 1
    value.request_identity = number
    value.process_identifier = 101
    return value
}
private func collect(_ queue: WorkerServiceQueue, request: Data) throws {
    try queue.withReplyCollector { owner in
        let data = failure(request)
        let result = data.withUnsafeBytes {
            dust3d_worker_reply_append(owner, $0.bindMemory(to: UInt8.self).baseAddress, $0.count)
        }
        try require(result.status == DUST3D_WORKER_REPLY_READY, "Pure reply bytes appended")
        try require(dust3d_worker_reply_finish(owner).status == DUST3D_WORKER_REPLY_FRAMED, "Pure input EOF observed")
    }
}
private func started(_ queue: WorkerServiceQueue, number: UInt64) throws -> WorkerServiceRequest {
    guard case .started(let value) = try queue.begin(generation: 9), value.identity.request == number else {
        throw QueueSmokeError.invalid("Expected captured request")
    }
    return value
}
private func currentSourceAndOwnership() throws -> Data {
    let queue = try WorkerServiceQueue(
        limits: .init(maximum_input_bytes: 4096, maximum_output_bytes: 4096, maximum_failure_bytes: 4096))
    var source = request(1)
    try queue.submit(source)
    source[64] = 255
    let first = try started(queue, number: 1)
    try require(first.frame == request(1), "Outbound frame owns the submitted bytes")
    try queue.submit(request(2))
    try collect(queue, request: first.frame)
    do {
        _ = try queue.withDecodedReply(
            observed: .init(connection_generation: 9, process_identifier: 101), receipt: receipt(1),
            maximumDecodedBytes: 4096
        ) { _ in 0 }
        throw QueueSmokeError.invalid("Stale result admitted")
    } catch let cause as WorkerServiceQueueError {
        try require(
            cause.status == DUST3D_WORKER_DECODE_ADMISSION_REJECTED.rawValue
                && cause.source == DUST3D_WORKER_DETAIL_ADMISSION.rawValue && !cause.cause.isEmpty,
            "Original stale admission receipt copied")
    }
    let second = try started(queue, number: 2)
    try collect(queue, request: second.frame)
    return try queue.withDecodedReply(
        observed: .init(connection_generation: 9, process_identifier: 101), receipt: receipt(2),
        maximumDecodedBytes: 4096
    ) { owner in
        var count = 0
        guard let bytes = dust3d_worker_decoded_failure_cause(owner, &count) else {
            throw QueueSmokeError.invalid("Missing original failure bytes")
        }
        return Data(bytes: bytes, count: count)
    }
}
private func retirementAndRecovery() throws {
    for shouldClose in [false, true] {
        let queue = try WorkerServiceQueue(
            limits: .init(maximum_input_bytes: 4096, maximum_output_bytes: 4096, maximum_failure_bytes: 4096))
        guard case .idle = try queue.begin(generation: 9) else {
            throw QueueSmokeError.invalid("Empty queue must idle")
        }
        try queue.submit(request(1))
        let active = try started(queue, number: 1)
        guard case .busy = try queue.begin(generation: 10) else {
            throw QueueSmokeError.invalid("Active generation cannot change")
        }
        if shouldClose { queue.close() } else { queue.cancel(active.identity) }
        try collect(queue, request: active.frame)
        do {
            _ = try queue.withDecodedReply(
                observed: .init(connection_generation: 9, process_identifier: 101), receipt: receipt(1),
                maximumDecodedBytes: 4096
            ) { _ in 0 }
            throw QueueSmokeError.invalid("Retired publication admitted")
        } catch let cause as WorkerServiceQueueError {
            try require(
                cause.operation == .admit && cause.source == DUST3D_WORKER_DETAIL_ADMISSION.rawValue
                    && cause.status == DUST3D_WORKER_DECODE_ADMISSION_REJECTED.rawValue,
                "Retirement preserves typed receipt")
        }
        if shouldClose {
            do {
                try queue.submit(request(2))
                throw QueueSmokeError.invalid("Closed submission accepted")
            } catch let cause as WorkerServiceQueueError {
                try require(
                    cause.operation == .submit && cause.source == DUST3D_WORKER_DETAIL_QUEUE.rawValue,
                    "Closed submission keeps queue cause")
            }
        } else {
            try queue.submit(request(2))
            let next = try started(queue, number: 2)
            var foreign = next.identity
            foreign.request = 99
            do {
                try queue.abandon(foreign)
                throw QueueSmokeError.invalid("Foreign failure consumed active work")
            } catch let cause as WorkerServiceQueueError {
                try require(cause.operation == .abandon && !cause.cause.isEmpty, "Original abandonment error copied")
            }
            try queue.abandon(next.identity)
            guard case .idle = try queue.begin(generation: 10) else {
                throw QueueSmokeError.invalid("Matching cleanup releases capacity")
            }
            do {
                _ = try queue.withReplyCollector { _ in 0 }
                throw QueueSmokeError.invalid("Absent borrowed collector accepted")
            } catch let cause as WorkerServiceQueueError {
                try require(cause.operation == .collector, "Absent borrowed view has typed boundary cause")
            }
        }
    }
}
private func originalSwiftErrorSurvives() throws {
    let queue = try WorkerServiceQueue(
        limits: .init(maximum_input_bytes: 4096, maximum_output_bytes: 4096, maximum_failure_bytes: 4096))
    try queue.submit(request(1))
    let active = try started(queue, number: 1)
    try collect(queue, request: active.frame)
    do {
        try queue.withDecodedReply(
            observed: .init(connection_generation: 9, process_identifier: 101), receipt: receipt(1),
            maximumDecodedBytes: 4096
        ) { _ in
            throw QueueSmokeError.invalid("original copied-value consumer error")
        }
        throw QueueSmokeError.invalid("Missing consumer error")
    } catch QueueSmokeError.invalid(let reason) {
        try require(
            reason == "original copied-value consumer error", "Decoded cleanup must preserve original Swift cause")
    }
    try queue.submit(request(2))
    _ = try started(queue, number: 2)
}
private func invalidConfigurationPreservesWaiting() throws {
    do {
        _ = try WorkerServiceQueue(
            limits: .init(maximum_input_bytes: 0, maximum_output_bytes: 4096, maximum_failure_bytes: 4096))
        throw QueueSmokeError.invalid("Unsupported quota accepted")
    } catch let cause as WorkerServiceQueueError {
        try require(
            cause.operation == .create && cause.status == DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED.rawValue
                && cause.source == DUST3D_WORKER_DETAIL_FRAME.rawValue && !cause.cause.isEmpty,
            "Original configuration cause retained")
    }
    let queue = try WorkerServiceQueue(
        limits: .init(maximum_input_bytes: 4096, maximum_output_bytes: 4096, maximum_failure_bytes: 4096))
    try queue.submit(request(1))
    do {
        _ = try queue.begin(generation: 0)
        throw QueueSmokeError.invalid("Zero service generation accepted")
    } catch let cause as WorkerServiceQueueError {
        try require(
            cause.operation == .begin && cause.source == DUST3D_WORKER_DETAIL_QUEUE.rawValue && !cause.cause.isEmpty,
            "Invalid generation cause retained")
    }
    let unchanged = try started(queue, number: 1)
    try require(unchanged.frame == request(1), "Rejected configuration cannot consume waiting source")
}
@main
private struct WorkerServiceQueueSmoke {
    static func main() throws {
        try invalidConfigurationPreservesWaiting()
        try retirementAndRecovery()
        try originalSwiftErrorSurvives()
        let copied = try currentSourceAndOwnership()
        try require(copied == Data([120, 0, 255]), "Independent semantic bytes survive Swift/C owners")
        print("Pure Swift service coordinator source/ownership contracts passed")
    }
}

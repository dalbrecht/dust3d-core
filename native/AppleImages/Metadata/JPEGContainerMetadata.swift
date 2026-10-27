import Foundation

/// JPEG envelope failures, separate from platform entropy/pixel decoding failures.
public enum JPEGContainerError: Error, Equatable, Sendable {
    case invalidLimits
    case encodedLimitExceeded
    case invalidJPEG
    case decodedLimitExceeded
    case unsupportedJPEG
}

/// Bounded single-image JPEG metadata. Structural inspection does not validate entropy data.
public struct JPEGContainerMetadata: Equatable, Sendable {
    public let width: Int
    public let height: Int
    public let componentCount: Int
    public let isProgressive: Bool

    /// Inspect a complete eight-bit baseline, extended sequential or progressive JPEG envelope.
    /// - Parameters:
    ///   - data: Encoded single-image bytes, without trailing data.
    ///   - maximumEncodedBytes: Positive encoded ceiling, at most 256 MiB.
    ///   - maximumDecodedBytes: Positive tightly packed RGBA8 ceiling, at most 256 MiB.
    /// - Throws: JPEGContainerError for malformed structure, unsupported modes or exceeded limits.
    /// - Note: Dimensions are limited to 8192 per side. Codec validity still requires pixel decoding.
    /// - Example: try JPEGContainerMetadata(data: bytes).width
    public init(
        data: Data, maximumEncodedBytes: Int = 32 * 1024 * 1024,
        maximumDecodedBytes: Int = 256 * 1024 * 1024
    ) throws {
        let ceiling = 256 * 1024 * 1024
        guard (1...ceiling).contains(maximumEncodedBytes), (1...ceiling).contains(maximumDecodedBytes) else {
            throw JPEGContainerError.invalidLimits
        }
        guard data.count <= maximumEncodedBytes else { throw JPEGContainerError.encodedLimitExceeded }
        self = try data.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
            guard bytes.count >= 4, bytes[0] == 255, bytes[1] == 216 else {
                throw JPEGContainerError.invalidJPEG
            }
            var offset = 2
            var frame: JPEGContainerMetadata?
            var hasScan = false
            while offset < bytes.count {
                guard bytes[offset] == 255 else { throw JPEGContainerError.invalidJPEG }
                while offset < bytes.count, bytes[offset] == 255 { offset += 1 }
                guard offset < bytes.count else { throw JPEGContainerError.invalidJPEG }
                let marker = bytes[offset]
                offset += 1
                if marker == 217 {
                    guard let frame, hasScan, offset == bytes.count else { throw JPEGContainerError.invalidJPEG }
                    return frame
                }
                guard marker != 0, marker != 216, marker != 1, !(208...215).contains(marker) else {
                    throw JPEGContainerError.invalidJPEG
                }
                guard offset <= bytes.count - 2 else { throw JPEGContainerError.invalidJPEG }
                let length = Int(bytes[offset]) * 256 + Int(bytes[offset + 1])
                guard length >= 2, length <= bytes.count - offset else { throw JPEGContainerError.invalidJPEG }
                let end = offset + length
                if [UInt8(192), 193, 194].contains(marker) {
                    guard frame == nil, length >= 8 else { throw JPEGContainerError.invalidJPEG }
                    guard bytes[offset + 2] == 8 else { throw JPEGContainerError.unsupportedJPEG }
                    let height = Int(bytes[offset + 3]) * 256 + Int(bytes[offset + 4])
                    let width = Int(bytes[offset + 5]) * 256 + Int(bytes[offset + 6])
                    let components = Int(bytes[offset + 7])
                    guard [1, 3, 4].contains(components), length == 8 + 3 * components else {
                        throw JPEGContainerError.invalidJPEG
                    }
                    guard (1...8192).contains(width), (1...8192).contains(height),
                        width * height * 4 <= maximumDecodedBytes
                    else {
                        throw JPEGContainerError.decodedLimitExceeded
                    }
                    frame = JPEGContainerMetadata(
                        width: width, height: height,
                        componentCount: components, isProgressive: marker == 194)
                } else if marker == 218 {
                    guard let frame, length >= 6 else { throw JPEGContainerError.invalidJPEG }
                    let components = Int(bytes[offset + 2])
                    guard components > 0, components <= frame.componentCount, length == 6 + 2 * components else {
                        throw JPEGContainerError.invalidJPEG
                    }
                    hasScan = true
                } else if (192...207).contains(marker), marker != 196 {
                    throw JPEGContainerError.unsupportedJPEG
                } else if marker == 221 {
                    guard length == 4 else { throw JPEGContainerError.invalidJPEG }
                } else if [UInt8(220), 222, 223].contains(marker) {
                    throw JPEGContainerError.unsupportedJPEG
                }
                offset = end
                if marker == 218 {
                    // Skip entropy without allocating or interpreting codec samples.
                    while offset < bytes.count {
                        if bytes[offset] != 255 {
                            offset += 1
                            continue
                        }
                        let markerStart = offset
                        while offset < bytes.count, bytes[offset] == 255 { offset += 1 }
                        guard offset < bytes.count else { throw JPEGContainerError.invalidJPEG }
                        if bytes[offset] == 0 || (208...215).contains(bytes[offset]) {
                            offset += 1
                        } else {
                            offset = markerStart
                            break
                        }
                    }
                }
            }
            throw JPEGContainerError.invalidJPEG
        }
    }

    private init(width: Int, height: Int, componentCount: Int, isProgressive: Bool) {
        self.width = width
        self.height = height
        self.componentCount = componentCount
        self.isProgressive = isProgressive
    }
}

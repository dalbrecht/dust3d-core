import Foundation

/// Structural PNG failures are independent of platform pixel decoding.
public enum PNGContainerError: LocalizedError, Equatable, Sendable {
    case invalidLimits
    case encodedLimitExceeded
    case invalidPNG
    case decodedLimitExceeded

    /// Specific container/quota diagnostic; pixel decoding failures remain separate.
    public var errorDescription: String? {
        switch self {
        case .invalidLimits: return "The PNG size limits must be positive and at most 256 MiB."
        case .encodedLimitExceeded: return "The encoded PNG exceeds the input size limit."
        case .invalidPNG: return "The PNG container is incomplete or has unsupported structure."
        case .decodedLimitExceeded: return "The PNG dimensions or decoded storage exceed the configured limit."
        }
    }
}

/// Bounded PNG envelope metadata; this does not decompress samples or verify chunk CRCs.
public struct PNGContainerMetadata: Equatable, Sendable {
    public let width: Int
    public let height: Int
    public let bitDepth: UInt8
    public let colorType: UInt8
    public let hasTransparencyKey: Bool

    /// Inspect one complete PNG envelope before platform decoding or archive insertion.
    /// - Parameters:
    ///   - data: Original encoded bytes, retained unchanged by callers.
    ///   - maximumEncodedBytes: Positive encoded ceiling, at most 256 MiB.
    ///   - maximumDecodedBytes: Positive tightly packed RGBA8 ceiling, at most 256 MiB.
    /// - Throws: `PNGContainerError` for limits, dimensions, malformed chunks or unsupported IHDR fields.
    /// - Note: Dimensions are at most 8192 per side. Codec validity remains a platform decoding responsibility.
    /// - Example: `try PNGContainerMetadata(data: png).width` reads the bounded source width.
    public init(data: Data, maximumEncodedBytes: Int = 32 * 1024 * 1024, maximumDecodedBytes: Int = 256 * 1024 * 1024)
        throws
    {
        let ceiling = 256 * 1024 * 1024
        guard (1...ceiling).contains(maximumEncodedBytes), (1...ceiling).contains(maximumDecodedBytes) else {
            throw PNGContainerError.invalidLimits
        }
        guard data.count <= maximumEncodedBytes else { throw PNGContainerError.encodedLimitExceeded }
        let result = try data.withUnsafeBytes { (bytes: UnsafeRawBufferPointer) in
            guard bytes.count >= 33, Array(bytes.prefix(8)) == [137, 80, 78, 71, 13, 10, 26, 10] else {
                throw PNGContainerError.invalidPNG
            }
            func integer(_ offset: Int) -> Int { bytes[offset..<offset + 4].reduce(0) { $0 * 256 + Int($1) } }
            guard integer(8) == 13, Array(bytes[12..<16]) == Array("IHDR".utf8) else {
                throw PNGContainerError.invalidPNG
            }
            let width = integer(16)
            let height = integer(20)
            guard (1...8192).contains(width), (1...8192).contains(height), width * height * 4 <= maximumDecodedBytes
            else {
                throw PNGContainerError.decodedLimitExceeded
            }
            let depths: [UInt8]
            switch bytes[25] {
            case 0: depths = [1, 2, 4, 8, 16]
            case 2, 4, 6: depths = [8, 16]
            case 3: depths = [1, 2, 4, 8]
            default: throw PNGContainerError.invalidPNG
            }
            guard depths.contains(bytes[24]), bytes[26] == 0, bytes[27] == 0, bytes[28] <= 1 else {
                throw PNGContainerError.invalidPNG
            }
            var offset = 8
            var hasImageData = false
            var hasTransparencyKey = false
            while offset <= bytes.count - 12 {
                let count = integer(offset)
                guard count <= bytes.count - offset - 12 else { throw PNGContainerError.invalidPNG }
                let kind = Array(bytes[offset + 4..<offset + 8])
                if offset != 8 && kind == Array("IHDR".utf8) { throw PNGContainerError.invalidPNG }
                if kind == Array("IDAT".utf8), count > 0 { hasImageData = true }
                if kind == Array("tRNS".utf8) { hasTransparencyKey = true }
                if kind == Array("IEND".utf8) {
                    guard count == 0, offset + 12 == bytes.count, hasImageData else {
                        throw PNGContainerError.invalidPNG
                    }
                    return (width, height, bytes[24], bytes[25], hasTransparencyKey)
                }
                offset += count + 12
            }
            throw PNGContainerError.invalidPNG
        }
        width = result.0
        height = result.1
        bitDepth = result.2
        colorType = result.3
        hasTransparencyKey = result.4
    }
}

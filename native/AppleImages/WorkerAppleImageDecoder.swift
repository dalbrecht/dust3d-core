import CoreGraphics
import Dust3DWorkerRuntime
import Foundation
import ImageIO

// SPDX-License-Identifier: MIT
#if canImport(Dust3DImageMetadata)
    import Dust3DImageMetadata
#endif

private enum WorkerAppleImageError: Int, Error {
    case invalidInput = 1
    case unsupportedEncoding, platformDecode, invalidStorage, encodedLimitExceeded
}
/// Apple-only synchronous image callbacks. No archive generation, parent fallback,
/// color conversion, orientation transform or premultiplication occurs here.
public enum WorkerAppleImageDecoder {
    /// Create independent imported/procedural callbacks with no mutable context.
    /// Call synchronously off the UI thread; borrowed input must remain valid for
    /// each callback. Any returned C image transfers to the caller for destruction.
    /// Quotas cover encoded/final RGBA bytes, not ImageIO workspace or process RSS.
    /// - Returns: Configuration usable with `dust3d_worker_serve_with_decoders`.
    /// - Example: `var decoders = WorkerAppleImageDecoder.configuration()`.
    public static func configuration() -> Dust3DWorkerDecoders {
        Dust3DWorkerDecoders(
            struct_size: MemoryLayout<Dust3DWorkerDecoders>.size,
            imported: Dust3DWorkerImageDecoder(
                struct_size: MemoryLayout<Dust3DWorkerImageDecoder>.size, context: nil, decode: imported),
            procedural: Dust3DWorkerImageDecoder(
                struct_size: MemoryLayout<Dust3DWorkerImageDecoder>.size, context: nil, decode: procedural))
    }
}
private func imported(
    _ context: UnsafeMutableRawPointer?, _ input: UnsafePointer<Dust3DWorkerImageInput>?,
    _ result: UnsafeMutablePointer<Dust3DWorkerImageResult>?
) -> OpaquePointer? { decode(input, result, procedural: false) }
private func procedural(
    _ context: UnsafeMutableRawPointer?, _ input: UnsafePointer<Dust3DWorkerImageInput>?,
    _ result: UnsafeMutablePointer<Dust3DWorkerImageResult>?
) -> OpaquePointer? { decode(input, result, procedural: true) }

private func decode(
    _ input: UnsafePointer<Dust3DWorkerImageInput>?, _ result: UnsafeMutablePointer<Dust3DWorkerImageResult>?,
    procedural: Bool
) -> OpaquePointer? {
    guard let result else { return nil }
    result.pointee = Dust3DWorkerImageResult()
    do {
        guard let input else { throw WorkerAppleImageError.invalidInput }
        let value = input.pointee
        let ceiling: UInt64 = 256 * 1024 * 1024
        guard value.struct_size == MemoryLayout<Dust3DWorkerImageInput>.size,
            (1...ceiling).contains(value.maximum_encoded_bytes), (1...ceiling).contains(value.maximum_decoded_bytes),
            value.encoded_count > 0, let encoded = value.encoded,
            (1...38).contains(value.model_count), let model = value.model,
            let identity = String(bytes: UnsafeBufferPointer(start: model, count: value.model_count), encoding: .utf8),
            !identity.utf8.contains(0)
        else { throw WorkerAppleImageError.invalidInput }
        guard value.encoding == 1 || (!procedural && value.encoding == 2) else {
            throw WorkerAppleImageError.unsupportedEncoding
        }
        guard value.encoded_count <= value.maximum_encoded_bytes else {
            throw WorkerAppleImageError.encodedLimitExceeded
        }
        let data = Data(bytes: encoded, count: value.encoded_count)
        let width: Int
        let height: Int
        let zerosTransparentGray: Bool
        if value.encoding == 1 {
            let metadata = try PNGContainerMetadata(
                data: data, maximumEncodedBytes: Int(value.maximum_encoded_bytes),
                maximumDecodedBytes: Int(value.maximum_decoded_bytes))
            width = metadata.width
            height = metadata.height
            zerosTransparentGray =
                procedural && metadata.hasTransparencyKey && metadata.colorType == 0
                && [2, 4, 8].contains(metadata.bitDepth)
        } else {
            let metadata = try JPEGContainerMetadata(
                data: data, maximumEncodedBytes: Int(value.maximum_encoded_bytes),
                maximumDecodedBytes: Int(value.maximum_decoded_bytes))
            width = metadata.width
            height = metadata.height
            zerosTransparentGray = false
        }
        let options = [kCGImageSourceShouldCache: false, kCGImageSourceShouldAllowFloat: false] as CFDictionary
        guard let source = CGImageSourceCreateWithData(data as CFData, options),
            let image = CGImageSourceCreateImageAtIndex(source, 0, options),
            CGImageSourceGetStatusAtIndex(source, 0) == .statusComplete,
            image.width == width, image.height == height,
            let provider = image.dataProvider?.data, let storage = CFDataGetBytePtr(provider)
        else { throw WorkerAppleImageError.platformDecode }
        let layout = try ImageSampleLayout(image)
        let row = width.multipliedReportingOverflow(by: layout.bytesPerPixel)
        let span = image.bytesPerRow.multipliedReportingOverflow(by: height)
        guard !row.overflow, !span.overflow, image.bytesPerRow >= row.partialValue,
            CFDataGetLength(provider) >= span.partialValue
        else { throw WorkerAppleImageError.invalidStorage }
        // Both envelope inspectors already checked bounded width*height*4.
        var pixels = Data(count: width * height * 4)
        try pixels.withUnsafeMutableBytes { (output: UnsafeMutableRawBufferPointer) in
            for row in 0..<height {
                for column in 0..<width {
                    var pixel = try layout.rgba(
                        storage.advanced(by: row * image.bytesPerRow + column * layout.bytesPerPixel))
                    if zerosTransparentGray && pixel.3 == 0 { pixel = (0, 0, 0, 0) }
                    let offset = (row * width + column) * 4
                    output[offset] = pixel.0
                    output[offset + 1] = pixel.1
                    output[offset + 2] = pixel.2
                    output[offset + 3] = pixel.3
                }
            }
        }
        return pixels.withUnsafeBytes {
            dust3d_worker_image_create(
                UInt32(width), UInt32(height), $0.bindMemory(to: UInt8.self).baseAddress, $0.count,
                value.maximum_decoded_bytes, result)
        }
    } catch {
        result.pointee = failure(error)
        return nil
    }
}
private func failure(_ error: any Error) -> Dust3DWorkerImageResult {
    let status: Dust3DWorkerImageStatus
    let code: Int32
    switch error {
    case let value as WorkerAppleImageError:
        if value == .encodedLimitExceeded {
            status = DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED
        } else {
            status =
                value == .invalidInput || value == .unsupportedEncoding
                ? DUST3D_WORKER_IMAGE_INVALID_INPUT : DUST3D_WORKER_IMAGE_DECODE_FAILED
        }
        code = Int32(value.rawValue)
    case let value as PNGContainerError:
        status =
            value == .encodedLimitExceeded || value == .decodedLimitExceeded
            ? DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED : DUST3D_WORKER_IMAGE_DECODE_FAILED
        switch value {
        case .invalidLimits: code = 101
        case .encodedLimitExceeded: code = 102
        case .invalidPNG: code = 103
        case .decodedLimitExceeded: code = 104
        }
    case let value as JPEGContainerError:
        status =
            value == .encodedLimitExceeded || value == .decodedLimitExceeded
            ? DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED : DUST3D_WORKER_IMAGE_DECODE_FAILED
        switch value {
        case .invalidLimits: code = 201
        case .encodedLimitExceeded: code = 202
        case .invalidJPEG: code = 203
        case .decodedLimitExceeded: code = 204
        case .unsupportedJPEG: code = 205
        }
    case let value as ImageSampleError:
        status = DUST3D_WORKER_IMAGE_DECODE_FAILED
        switch value {
        case .unsupportedLayout: code = 301
        case .invalidPixels: code = 302
        }
    default:
        status = DUST3D_WORKER_IMAGE_INTERNAL_FAILED
        code = 0
    }
    let cause = Array(String(reflecting: error).utf8)
    return cause.withUnsafeBufferPointer { dust3d_worker_image_error_create(status, code, $0.baseAddress, $0.count) }
}

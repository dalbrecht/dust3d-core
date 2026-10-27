import CoreGraphics

enum ImageSampleError: Error { case unsupportedLayout, invalidPixels }

struct ImageSampleLayout {
    let bytesPerPixel: Int
    let bytesPerSample: Int
    let isLittleEndian: Bool
    let model: CGColorSpaceModel
    let hasAlpha: Bool
    let palette: [UInt8]

    init(_ image: CGImage) throws {
        guard let space = image.colorSpace,
            image.bitsPerComponent == 8 || image.bitsPerComponent == 16,
            image.bitsPerPixel % image.bitsPerComponent == 0,
            [.none, .last, .noneSkipLast].contains(image.alphaInfo)
        else { throw ImageSampleError.unsupportedLayout }
        model = space.model
        bytesPerSample = image.bitsPerComponent / 8
        bytesPerPixel = image.bitsPerPixel / 8
        hasAlpha = image.alphaInfo == .last
        isLittleEndian = image.bitmapInfo.intersection(.byteOrderMask) == .byteOrder16Little
        palette = space.colorTable ?? []
        let channels = image.bitsPerPixel / image.bitsPerComponent
        switch model {
        case .rgb:
            guard channels == (image.alphaInfo == .none ? 3 : 4) else {
                throw ImageSampleError.unsupportedLayout
            }
        case .monochrome:
            guard channels == (hasAlpha ? 2 : 1) else { throw ImageSampleError.unsupportedLayout }
        case .indexed:
            guard bytesPerPixel == 1, space.baseColorSpace?.model == .rgb, !palette.isEmpty,
                palette.count % 3 == 0
            else { throw ImageSampleError.unsupportedLayout }
        default: throw ImageSampleError.unsupportedLayout
        }
    }

    func rgba(_ source: UnsafePointer<UInt8>) throws -> (UInt8, UInt8, UInt8, UInt8) {
        func sample(_ channel: Int) -> UInt8 {
            let offset = channel * bytesPerSample
            guard bytesPerSample == 2 else { return source[offset] }
            let first = Int(source[offset])
            let second = Int(source[offset + 1])
            let value = isLittleEndian ? first + second * 256 : first * 256 + second
            // Match QRgba64's nearest 16-to-8 conversion, including half-step edges.
            let rounded = value + 128
            return UInt8((rounded - (rounded >> 8)) >> 8)
        }
        if model == .indexed {
            let offset = Int(source[0]) * 3
            guard offset + 2 < palette.count else { throw ImageSampleError.invalidPixels }
            return (palette[offset], palette[offset + 1], palette[offset + 2], 255)
        }
        if model == .monochrome {
            let gray = sample(0)
            return (gray, gray, gray, hasAlpha ? sample(1) : 255)
        }
        return (sample(0), sample(1), sample(2), hasAlpha ? sample(3) : 255)
    }
}

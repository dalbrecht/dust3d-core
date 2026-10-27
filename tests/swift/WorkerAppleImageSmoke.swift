import Darwin
import Dust3DWorkerRuntime
import Foundation

// SPDX-License-Identifier: MIT
#if canImport(Dust3DWorkerAppleImages)
    import Dust3DWorkerAppleImages
#endif

private enum ImageSmokeError: Error { case invalid(String) }
private func require(_ value: Bool, _ message: String) throws {
    guard value else { throw ImageSmokeError.invalid(message) }
}
private func decode(_ data: Data, encoding: UInt32, procedural: Bool, maximumDecoded: UInt64 = 1024) throws -> [UInt8] {
    let configuration = WorkerAppleImageDecoder.configuration()
    let decoder = procedural ? configuration.procedural : configuration.imported
    guard let callback = decoder.decode else { throw ImageSmokeError.invalid("Missing callback") }
    let identity = Array("00000000-0000-0000-0000-000000000001".utf8)
    var result = Dust3DWorkerImageResult()
    let image = data.withUnsafeBytes { bytes in
        identity.withUnsafeBufferPointer { model in
            var input = Dust3DWorkerImageInput(
                struct_size: MemoryLayout<Dust3DWorkerImageInput>.size,
                encoded: bytes.bindMemory(to: UInt8.self).baseAddress, encoded_count: bytes.count,
                model: model.baseAddress, model_count: model.count, encoding: encoding,
                maximum_encoded_bytes: 1024 * 1024, maximum_decoded_bytes: maximumDecoded)
            return callback(decoder.context, &input, &result)
        }
    }
    guard let image else { throw ImageSmokeError.invalid("Callback failed: \(result.status.rawValue)/\(result.code)") }
    defer { dust3d_worker_image_destroy(image) }
    try require(
        result.status == DUST3D_WORKER_IMAGE_OK && result.code == 0 && result.cause_length == 0, "Success receipt")
    let size = Int(dust3d_worker_image_byte_count(image))
    try require(size > 0 && size <= 1024 && dust3d_worker_image_width(image) == 2, "Bounded dimensions")
    var pixels = [UInt8](repeating: 0, count: size)
    try require(
        pixels.withUnsafeMutableBufferPointer { dust3d_worker_image_copy_pixels(image, 0, $0.baseAddress, $0.count) }
            == 1, "Owned sample copy")
    return pixels
}
// Valid encoded data with a scalar output policy: the envelope rejects before ImageIO.
private func checkQuotaDiagnostic(_ data: Data, encoded: Bool = false) throws {
    let configuration = WorkerAppleImageDecoder.configuration()
    guard let callback = configuration.imported.decode else { throw ImageSmokeError.invalid("Missing callback") }
    let identity = Array("00000000-0000-0000-0000-000000000001".utf8)
    var result = Dust3DWorkerImageResult()
    let image = data.withUnsafeBytes { bytes in
        identity.withUnsafeBufferPointer { model in
            var input = Dust3DWorkerImageInput(
                struct_size: MemoryLayout<Dust3DWorkerImageInput>.size,
                encoded: bytes.bindMemory(to: UInt8.self).baseAddress, encoded_count: bytes.count,
                model: model.baseAddress, model_count: model.count, encoding: 1,
                maximum_encoded_bytes: encoded ? UInt64(data.count - 1) : 1024 * 1024,
                maximum_decoded_bytes: encoded ? 1024 : 15)
            return callback(configuration.imported.context, &input, &result)
        }
    }
    defer { dust3d_worker_image_destroy(image) }
    let cause = withUnsafeBytes(of: result.cause) { Data($0.prefix(Int(result.cause_length))) }
    try require(
        image == nil && result.status == DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED && result.code == (encoded ? 5 : 104),
        "Predecode quota classification")
    try require(
        String(data: cause, encoding: .utf8)?.contains(
            encoded ? "encodedLimitExceeded" : "PNGContainerError.decodedLimitExceeded") == true,
        "Original typed envelope cause")
}
@main private enum WorkerAppleImageSmoke {
    static func main() {
        do {
            try require(CommandLine.arguments.count == 2, "Fixture directory required")
            let root = URL(fileURLWithPath: CommandLine.arguments[1])
            let png = try Data(contentsOf: root.appendingPathComponent("ImportedMaterial.png"))
            try require(
                try decode(png, encoding: 1, procedural: false) == [
                    255, 0, 0, 128, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 64,
                ], "Straight alpha/source row order")
            try checkQuotaDiagnostic(png)
            try checkQuotaDiagnostic(png, encoded: true)
            let jpeg = try Data(contentsOf: root.appendingPathComponent("ImportedMaterial.jpg"))
            let jpegPixels = try decode(jpeg, encoding: 2, procedural: false)
            try require(
                jpegPixels.count == 16 && stride(from: 3, to: 16, by: 4).allSatisfy { jpegPixels[$0] == 255 },
                "JPEG opaque shape")
            // Existing independently encoded MIT Apple positive fixture: keyed gray8.
            guard
                let gray = Data(
                    base64Encoded:
                        "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAAAAADRSSBWAAAAAnRSTlMAERwj7coAAAALSURBVHicYxA8AQAA7QDaiakNYwAAAABJRU5ErkJggg=="
                )
            else { throw ImageSmokeError.invalid("Fixture literal") }
            try require(
                try decode(gray, encoding: 1, procedural: false) == [17, 17, 17, 0, 200, 200, 200, 255],
                "Imported hidden RGB")
            try require(
                try decode(gray, encoding: 1, procedural: true) == [0, 0, 0, 0, 200, 200, 200, 255],
                "Procedural Qt-compatible keyed gray")
            let positiveSamples: [(String, [UInt8])] = [
                (
                    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABEAIAAAAr0DSeAAAAFUlEQVR4nGNg+N/QUP///38GhgYGACtoBfw3hzYxAAAAAElFTkSuQmCC",
                    [1, 128, 127, 255, 255, 0, 128, 255]
                ),
                (
                    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABAQMAAADO7O3JAAAABlBMVEX/AAAAAP9sof2OAAAACklEQVR4nGNwAAAAQgBBKTf07wAAAABJRU5ErkJggg==",
                    [255, 0, 0, 255, 0, 0, 255, 255]
                ),
                (
                    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAABEAQAAAAOu2tCAAAAEUlEQVR4nGNg+M/I2MBQ/x8ACwoDAHiba9wAAAAASUVORK5CYII=",
                    [1, 1, 1, 1, 128, 128, 128, 127]
                ),
            ]
            for (encoded, expected) in positiveSamples {
                guard let data = Data(base64Encoded: encoded) else {
                    throw ImageSmokeError.invalid("Positive sample literal")
                }
                try require(
                    try decode(data, encoding: 1, procedural: false) == expected, "16-bit rounding/palette/source alpha"
                )
            }
            print("Apple worker PNG/JPEG callback ownership and purpose-specific samples passed")
        } catch {
            FileHandle.standardError.write(Data("Apple worker image fixture failed: \(error)\n".utf8))
            exit(EXIT_FAILURE)
        }
    }
}

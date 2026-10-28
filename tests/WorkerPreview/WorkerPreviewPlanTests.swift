// SPDX-License-Identifier: MIT
import Dust3DWorkerPreview
import Dust3DWorkerXPC
import Foundation
import XCTest

final class WorkerPreviewPlanTests: XCTestCase {
    private func options(
        archiveBytes: UInt64 = 3, indexed: UInt64 = UInt64.max, source: UInt64 = 0,
        cache: UInt64 = 0, input: UInt64 = 27, output: UInt64 = UInt64.max,
        inputChunk: UInt32 = 1, outputChunk: UInt32 = 65536
    ) -> WorkerXPCPreviewOptions {
        WorkerXPCPreviewOptions(
            maximumCachePayloadBytes: cache, archiveBytes: archiveBytes,
            maximumIndexedBytes: indexed, maximumSourceBytes: source,
            maximumInputTransferBytes: input, inputChunkBytes: inputChunk,
            maximumOutputTransferBytes: output, outputChunkBytes: outputChunk)
    }
    func testShouldPreserveOwnedArchiveAndFullNativeRawPolicies() throws {
        var bytes = Data([1, 2, 3])
        let captured = try WorkerPreviewSource(archive: bytes, options: options())
        bytes[0] = 9
        XCTAssertEqual(captured.archive, Data([1, 2, 3]))
        XCTAssertEqual(captured.options.maximumIndexedBytes, UInt64.max)
        XCTAssertEqual(captured.options.maximumSourceBytes, 0)
        XCTAssertEqual(captured.options.maximumCachePayloadBytes, 0)
        XCTAssertEqual(captured.options.maximumOutputTransferBytes, UInt64.max)
    }
    func testShouldRejectMetadataBeforeAnyTransportOrNativeParsing() {
        let invalid = [
            options(archiveBytes: 0), options(archiveBytes: 4), options(source: 268_435_457),
            options(cache: 268_435_457), options(input: 26), options(output: 0),
            options(inputChunk: 0), options(outputChunk: 65537),
        ]
        for controls in invalid {
            XCTAssertThrowsError(try WorkerPreviewSource(archive: Data([1, 2, 3]), options: controls))
        }
        XCTAssertThrowsError(try WorkerPreviewSource(archive: Data(), options: options(archiveBytes: 0)))
    }
}

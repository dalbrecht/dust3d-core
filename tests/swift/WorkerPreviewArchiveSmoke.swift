// SPDX-License-Identifier: MIT
import Darwin
import Dust3DWorkerRuntime
import Foundation

private enum ArchiveSmokeError: Error { case invalid(String) }
private struct CopiedArchive: Sendable, Equatable {
    let bytes: Data
    let sourceBudget: UInt64
    let indexedBudget: UInt64
}
private actor ArchiveReceiver {
    private var value: CopiedArchive?
    func store(_ value: CopiedArchive) { self.value = value }
    func snapshot() -> CopiedArchive? { value }
}
private func copyArchive(_ archive: Data) throws -> CopiedArchive {
    guard let file = tmpfile() else { throw ArchiveSmokeError.invalid("Anonymous channel unavailable") }
    defer { fclose(file) }
    var scope = Dust3DWorkerPreviewArchiveScope()
    withUnsafeMutableBytes(of: &scope.identity.document) { $0[0] = 1 }
    scope.identity.request = 2
    scope.identity.revision = 7
    scope.connection_generation = 3
    let context = Dust3DWorkerPreviewArchiveContext(
        archive_bytes: UInt64(archive.count), maximum_source_bytes: 0, maximum_indexed_bytes: 268_435_457)
    let policy = Dust3DWorkerPreviewChunkPolicy(
        maximum_transfer_bytes: UInt64(archive.count) + 24, chunk_payload_bytes: 7)
    let sent = archive.withUnsafeBytes {
        dust3d_worker_preview_archive_write(
            fileno(file), scope, context, $0.bindMemory(to: UInt8.self).baseAddress, $0.count, policy)
    }
    guard sent.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK, fseek(file, 0, SEEK_SET) == 0 else {
        throw ArchiveSmokeError.invalid("Segmented archive write failed")
    }
    var result = Dust3DWorkerPreviewArchiveResult()
    guard let owner = dust3d_worker_preview_archive_read(fileno(file), scope, context, policy, &result),
        result.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK
    else { throw ArchiveSmokeError.invalid("Complete archive read failed") }
    defer { dust3d_worker_preview_archive_destroy(owner) }
    var count = 0
    guard let bytes = dust3d_worker_preview_archive_bytes(owner, &count), count == archive.count else {
        throw ArchiveSmokeError.invalid("Borrowed archive count differs")
    }
    let controls = dust3d_worker_preview_archive_context(owner)
    return CopiedArchive(
        bytes: Data(bytes: bytes, count: count), sourceBudget: controls.maximum_source_bytes,
        indexedBudget: controls.maximum_indexed_bytes)
}
@main private struct WorkerPreviewArchiveSmoke {
    static func main() async throws {
        guard CommandLine.arguments.count == 2 else { throw ArchiveSmokeError.invalid("Fixture required") }
        let original = try Data(contentsOf: URL(fileURLWithPath: CommandLine.arguments[1]))
        guard !original.isEmpty, original.count < 1_048_576 else {
            throw ArchiveSmokeError.invalid("Only ordinary small licensed fixture is permitted")
        }
        let copied = try copyArchive(original)
        guard copied.bytes == original, copied.sourceBudget == 0, copied.indexedBudget == 268_435_457 else {
            throw ArchiveSmokeError.invalid("Copied archive or native controls differ after owner destruction")
        }
        let receiver = ArchiveReceiver()
        await receiver.store(copied)
        guard await receiver.snapshot() == copied else {
            throw ArchiveSmokeError.invalid("Owned immutable bytes differ after actor transfer")
        }
        print("Segmented archive C/Swift ownership passed")
    }
}

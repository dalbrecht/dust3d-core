// SPDX-License-Identifier: MIT
#include <Dust3DWorkerPreviewArchive.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerPreviewArchiveIO.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
using File = std::unique_ptr<FILE, decltype(&fclose)>;
using Owner = std::unique_ptr<Dust3DWorkerPreviewArchive, decltype(&dust3d_worker_preview_archive_destroy)>;
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Licensed small archive fixture path required");
        std::ifstream input(argv[1], std::ios::binary);
        require(bool(input), "Licensed Capsule fixture opens");
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        require(!archive.empty() && archive.size() < 1048576, "Only ordinary small positive fixture is exercised");
        Dust3DWorkerPreviewArchiveScope scope {};
        scope.identity.document[0] = 1;
        scope.identity.request = 2;
        scope.identity.revision = 7;
        scope.connection_generation = 3;
        Dust3DWorkerPreviewArchiveContext context { archive.size(), maximumPayloadBytes, maximumPayloadBytes + 1 };
        Dust3DWorkerPreviewChunkPolicy policy { archive.size() + sourceRequestPrefixBytes, 7 };
        File channel(tmpfile(), fclose);
        require(bool(channel), "Fresh anonymous ordinary channel opens");
        const auto sent = dust3d_worker_preview_archive_write(fileno(channel.get()), scope, context, archive.data(), archive.size(), policy);
        require(sent.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK, "C sender writes the complete segmented archive");
        rewind(channel.get());
        Dust3DWorkerPreviewArchiveResult receipt {};
        Owner owner(dust3d_worker_preview_archive_read(fileno(channel.get()), scope, context, policy, &receipt), dust3d_worker_preview_archive_destroy);
        require(owner && receipt.status == DUST3D_WORKER_PREVIEW_ARCHIVE_OK && feof(channel.get()) == 0, "C receiver completes using descriptor EOF independently of stdio buffering");
        channel.reset();
        size_t count = 0;
        const auto* bytes = dust3d_worker_preview_archive_bytes(owner.get(), &count);
        require(!dust3d_worker_preview_archive_bytes(owner.get(), nullptr), "Missing size never returns a borrowed span");
        const auto metadata = dust3d_worker_preview_archive_context(owner.get());
        require(bytes && count == archive.size() && std::equal(archive.begin(), archive.end(), bytes)
                && metadata.archive_bytes == archive.size() && metadata.maximum_source_bytes == maximumPayloadBytes && metadata.maximum_indexed_bytes == maximumPayloadBytes + 1,
            "C immutable archive and original policies outlive their borrowed channel");
        const std::vector<uint8_t> copy(bytes, bytes + count);
        owner.reset();
        require(copy == archive, "Owned caller copy survives C owner destruction");
        count = 99;
        require(!dust3d_worker_preview_archive_bytes(nullptr, &count) && !count && !dust3d_worker_preview_archive_bytes(nullptr, nullptr)
                && !dust3d_worker_preview_archive_context(nullptr).archive_bytes,
            "Null C getters clear output safely");
        dust3d_worker_preview_archive_destroy(nullptr);
        File untouched(tmpfile(), fclose);
        require(bool(untouched), "Fresh preflight-only channel opens");
        require(!dust3d_worker_preview_archive_read(fileno(untouched.get()), scope, context, policy, nullptr) && ftell(untouched.get()) == 0, "Missing result prevents all input work");
        const auto nullSpan = dust3d_worker_preview_archive_write(fileno(untouched.get()), scope, context, nullptr, archive.size(), policy);
        require(nullSpan.status == DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED && nullSpan.detail_code == static_cast<int32_t>(PreviewChunkErrorCode::invalidInput) && ftell(untouched.get()) == 0, "Missing archive rejects before output");
        const auto wrongCount = dust3d_worker_preview_archive_write(fileno(untouched.get()), scope, context, archive.data(), archive.size() - 1, policy);
        require(wrongCount.status == DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED && ftell(untouched.get()) == 0, "Captured archive count must match sender storage before output");
        const auto invalidDescriptor = dust3d_worker_preview_archive_write(-1, scope, context, archive.data(), archive.size(), policy);
        require(invalidDescriptor.status == DUST3D_WORKER_PREVIEW_ARCHIVE_IO_FAILED && invalidDescriptor.system_error && invalidDescriptor.io_stage == DUST3D_WORKER_IO_OUTPUT, "Negative descriptor rejects in pure preflight with original stage/code");
        Owner missing(dust3d_worker_preview_archive_read(-1, scope, context, policy, &receipt), dust3d_worker_preview_archive_destroy);
        require(!missing && receipt.status == DUST3D_WORKER_PREVIEW_ARCHIVE_IO_FAILED && receipt.io_stage == DUST3D_WORKER_IO_INPUT && receipt.message[0], "Input descriptor preflight retains bounded original receipt");
        auto invalidContext = context;
        invalidContext.maximum_source_bytes = maximumPayloadBytes + 1;
        const auto invalidPolicy = dust3d_worker_preview_archive_write(fileno(untouched.get()), scope, invalidContext, archive.data(), archive.size(), policy);
        require(invalidPolicy.status == DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED && ftell(untouched.get()) == 0, "Native policy validation happens before writes");
        auto invalidScope = scope;
        invalidScope.connection_generation = 0;
        const auto invalidIdentity = dust3d_worker_preview_archive_write(fileno(untouched.get()), invalidScope, context, archive.data(), archive.size(), policy);
        require(invalidIdentity.status == DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED && ftell(untouched.get()) == 0, "Identity validation happens before writes");
        Owner empty(dust3d_worker_preview_archive_read(fileno(untouched.get()), scope, context, policy, &receipt), dust3d_worker_preview_archive_destroy);
        require(!empty && receipt.status == DUST3D_WORKER_PREVIEW_ARCHIVE_CHUNK_REJECTED && receipt.detail_code == static_cast<int32_t>(PreviewChunkErrorCode::invalidSequence), "Actual early EOF cannot publish a C owner");
        PreviewChunkIdentity expected;
        expected.request.document[0] = 1;
        expected.request.request = 2;
        expected.request.revision = 7;
        expected.connectionGeneration = 3;
        const PreviewArchiveContext captured { archive.size(), maximumPayloadBytes, maximumPayloadBytes + 1 };
        const PreviewChunkPolicy chunking { policy.maximum_transfer_bytes, 7 };
        File direct(tmpfile(), fclose);
        require(bool(direct), "Direct C++ channel opens");
        auto wrongDirection = expected;
        wrongDirection.direction = PreviewChunkDirection::session;
        try {
            writePreviewArchive(fileno(direct.get()), wrongDirection, captured, archive.data(), archive.size(), chunking);
            throw std::runtime_error("Session direction accepted by archive channel sender");
        } catch (const PreviewChunkError& cause) {
            require(cause.code() == PreviewChunkErrorCode::invalidIdentity && ftell(direct.get()) == 0, "Wrong direction rejects in preflight without writes");
        }
        writePreviewArchive(fileno(direct.get()), expected, captured, archive.data(), archive.size(), chunking);
        rewind(direct.get());
        auto received = readPreviewArchive(fileno(direct.get()), expected, captured, chunking);
        require(received->isComplete() && std::equal(archive.begin(), archive.end(), received->view().source.archive), "Direct C++ channel ownership agrees with C boundary");
        File unsupported(tmpfile(), fclose);
        require(bool(unsupported), "Tiny protocol-only diagnostic channel opens");
        const std::vector<uint8_t> opaque { 1 };
        auto prefix = encodeSourceRequestPrefix({ 1, 0 });
        prefix[3] = 2;
        PreviewChunkWriter protocol(expected, prefix.size() + opaque.size(), chunking, [&](const uint8_t* data, size_t size) { require(fwrite(data, 1, size, unsupported.get()) == size, "Protocol fixture writes"); });
        protocol.start();
        protocol.append(prefix.data(), prefix.size());
        protocol.append(opaque.data(), opaque.size());
        protocol.finish();
        rewind(unsupported.get());
        Owner rejected(dust3d_worker_preview_archive_read(fileno(unsupported.get()), scope, { 1, 0, 0 }, policy, &receipt), dust3d_worker_preview_archive_destroy);
        require(!rejected && receipt.status == DUST3D_WORKER_PREVIEW_ARCHIVE_SOURCE_REJECTED && receipt.detail_code == static_cast<int32_t>(SourceRequestErrorCode::unsupportedVersion), "Original prefix cause crosses C; opaque body never reaches native parsing");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

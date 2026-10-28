// SPDX-License-Identifier: MIT
#include "WorkerPreviewSessionIO.h"
#include "WorkerJobIO.h"
#include "WorkerPreviewArchiveIO.h"
#include "WorkerSessionOutputCopy.h"
#include <cerrno>
#include <unistd.h>
namespace dust3d {
namespace worker {
    void writePreviewSessionChannel(int output, PreviewChunkIdentity identity, PreviewArchiveContext context, const SessionOutput& value, PreviewChunkPolicy policy)
    {
        if (output < 0)
            throw WorkerJobIOError(WorkerJobStage::output, EINVAL);
        writePreviewSession(identity, context, value, policy, [output](const uint8_t* bytes, size_t size) {
            const auto written = writeWorkerBytes(output, bytes, size);
            if (!written.complete)
                throw WorkerJobIOError(WorkerJobStage::output, written.error);
        });
    }
    SessionOutput readPreviewSessionChannel(int input, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy)
    {
        if (input < 0)
            throw WorkerJobIOError(WorkerJobStage::input, EINVAL);
        PreviewSessionCollector owner(identity, context, policy);
        std::array<uint8_t, maximumPreviewChunkPayloadBytes> workspace;
        while (true) {
            const auto count = ::read(input, workspace.data(), workspace.size());
            if (count < 0) {
                if (errno == EINTR)
                    continue;
                throw WorkerJobIOError(WorkerJobStage::input, errno);
            }
            if (!count) {
                owner.finishAtEOF();
                return owner.take();
            }
            owner.append(workspace.data(), static_cast<size_t>(count));
        }
    }
    void servePreviewSessionJob(int input, int output, ConnectionGenerationSession& session, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy inputPolicy, PreviewChunkPolicy outputPolicy)
    {
        if (input < 0 || output < 0 || input == output)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Preview serving requires separate valid archive/reply channels.");
        context = detail::checkedPreviewArchiveContext(context);
        if (identity.direction != PreviewChunkDirection::archive)
            throw PreviewChunkError(PreviewChunkErrorCode::invalidIdentity, "Preview serving requires captured archive-direction scope.");
        (void)encodePreviewChunkHeader({ PreviewChunkKind::begin, identity, 0, sourceRequestPrefixBytes + context.archiveBytes, 0 }, inputPolicy);
        // Every session contains these prefixes even with no statistics available;
        // this checks necessary policy validity, not invented observed geometry/stats.
        (void)measurePreviewChunking(sessionOutputPrefixBytes + generationStatisticsPrefixBytes, outputPolicy);
        auto archive = readPreviewArchive(input, identity, context, inputPolicy);
        auto generated = session.generatePreview(archive->view(), identity.request, identity.connectionGeneration);
        archive.reset();
        auto value = copySessionOutput(std::move(generated), {});
        identity.direction = PreviewChunkDirection::session;
        writePreviewSessionChannel(output, identity, context, value, outputPolicy);
    }
}
}

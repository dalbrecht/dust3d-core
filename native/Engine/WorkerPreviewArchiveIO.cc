// SPDX-License-Identifier: MIT
#include "WorkerPreviewArchiveIO.h"
#include "WorkerJobIO.h"
#include <cerrno>
#include <unistd.h>
namespace dust3d {
namespace worker {
    void writePreviewArchive(int output, PreviewChunkIdentity identity, PreviewArchiveContext context, const uint8_t* archive, size_t size, PreviewChunkPolicy policy)
    {
        if (output < 0)
            throw WorkerJobIOError(WorkerJobStage::output, EINVAL);
        context = detail::checkedPreviewArchiveContext(context);
        if (identity.direction != PreviewChunkDirection::archive)
            throw PreviewChunkError(PreviewChunkErrorCode::invalidIdentity, "Preview archive sender requires the archive direction.");
        if (!archive || size != context.archiveBytes)
            throw PreviewChunkError(PreviewChunkErrorCode::invalidInput, "Preview sender storage differs from the captured archive span.");
        const auto prefix = encodeSourceRequestPrefix({ context.archiveBytes, context.maximumSourceBytes });
        PreviewChunkWriter writer(identity, prefix.size() + context.archiveBytes, policy, [output](const uint8_t* bytes, size_t count) {
            const auto result = writeWorkerBytes(output, bytes, count);
            if (!result.complete)
                throw WorkerJobIOError(WorkerJobStage::output, result.error);
        });
        writer.start();
        writer.append(prefix.data(), prefix.size());
        writer.append(archive, size);
        writer.finish();
    }
    std::unique_ptr<PreviewArchiveCollector> readPreviewArchive(int input, PreviewChunkIdentity identity, PreviewArchiveContext context, PreviewChunkPolicy policy)
    {
        if (input < 0)
            throw WorkerJobIOError(WorkerJobStage::input, EINVAL);
        auto owner = std::make_unique<PreviewArchiveCollector>(identity, context, policy);
        std::array<uint8_t, maximumPreviewChunkPayloadBytes> workspace;
        while (true) {
            const auto count = ::read(input, workspace.data(), workspace.size());
            if (count < 0) {
                if (errno == EINTR)
                    continue;
                throw WorkerJobIOError(WorkerJobStage::input, errno);
            }
            if (!count) {
                owner->finishAtEOF();
                return owner;
            }
            owner->append(workspace.data(), static_cast<size_t>(count));
        }
    }
}
}

// SPDX-License-Identifier: MIT
#include "WorkerJobIO.h"
#include "WorkerImportedOperation.h"
#include "WorkerIndexedSourceOperation.h"
#include "WorkerLegacyOperation.h"
#include "WorkerOperation.h"
#include "WorkerSourceBakeOperation.h"
#include "WorkerSourceOperation.h"
#include "WorkerUvOperation.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <unistd.h>
namespace dust3d {
namespace worker {
    namespace {
        WorkerIoResult readExactly(int descriptor, uint8_t* bytes, size_t size)
        {
            size_t offset = 0;
            while (offset < size) {
                const auto count = ::read(descriptor, bytes + offset, std::min(size - offset, size_t { 65536 }));
                if (count == 0)
                    return { false, 0 };
                if (count < 0) {
                    if (errno == EINTR)
                        continue;
                    return { false, errno };
                }
                offset += static_cast<size_t>(count);
            }
            return { true, 0 };
        }
        void requireRead(WorkerIoResult result, const char* truncated)
        {
            if (!result.complete) {
                if (result.error)
                    throw WorkerJobIOError(WorkerJobStage::input, result.error);
                throw ProtocolError(ProtocolErrorCode::truncated, truncated);
            }
        }
    }
    WorkerJobIOError::WorkerJobIOError(WorkerJobStage stage, int systemError) noexcept
        : m_stage(stage)
        , m_systemError(systemError)
    {
    }
    const char* WorkerJobIOError::what() const noexcept
    {
        return m_stage == WorkerJobStage::input ? "Worker request I/O failed." : "Worker reply I/O failed.";
    }
    const char* workerOperationName(std::optional<Operation> operation) noexcept
    {
        if (operation == Operation::generateMesh)
            return "generateMesh";
        if (operation == Operation::generateUvMesh)
            return "generateUvMesh";
        if (operation == Operation::generateSourcedMesh)
            return "generateSourcedMesh";
        if (operation == Operation::generateIndexedSourcedMesh)
            return "generateIndexedSourcedMesh";
        if (operation == Operation::generateIndexedMesh)
            return "generateIndexedMesh";
        if (operation == Operation::generateImportedRender)
            return "generateImportedRender";
        if (operation == Operation::generateSourceBaked)
            return "generateSourceBaked";
        if (operation == Operation::generateLegacyBaked)
            return "generateLegacyBaked";
        return "unknown";
    }
    WorkerIoResult writeWorkerBytes(int descriptor, const uint8_t* bytes, size_t size) noexcept
    {
        if (!size)
            return { true, 0 };
        if (descriptor < 0 || !bytes)
            return { false, EINVAL };
        size_t offset = 0;
        while (offset < size) {
            const auto count = ::write(descriptor, bytes + offset, std::min(size - offset, size_t { 65536 }));
            if (count <= 0) {
                if (count < 0 && errno == EINTR)
                    continue;
                return { false, count < 0 ? errno : EIO };
            }
            offset += static_cast<size_t>(count);
        }
        return { true, 0 };
    }
    void serveWorkerJob(int input, int output, WorkerJobContext& context, ImportedRenderImageDecoder* decoder, const NativeImportedProducerLimits& importedLimits, ProceduralImageDecoder* proceduralDecoder, const LegacyProducerOptions& legacyLimits, const NativeSourceProducerOptions& sourceBakeLimits)
    {
        context = {};
        if (input < 0 || output < 0 || input == output)
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Worker job requires separate valid request/reply channels.");
        std::array<uint8_t, headerBytes> bytes {};
        requireRead(readExactly(input, bytes.data(), bytes.size()), "Worker request header is incomplete.");
        const auto header = decodeHeader(bytes.data(), bytes.size(), {});
        if (header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Worker accepts request frames only.");
        context.requestIdentity = header.identity.request;
        context.operation = header.operation;
        Frame request { header, std::vector<uint8_t>(static_cast<size_t>(header.payloadBytes)) };
        requireRead(readExactly(input, request.payload.data(), request.payload.size()), "Worker request body is incomplete.");
        uint8_t extra;
        const auto end = readExactly(input, &extra, 1);
        if (end.complete)
            throw ProtocolError(ProtocolErrorCode::trailingData, "Worker accepts exactly one request followed by EOF.");
        if (end.error)
            throw WorkerJobIOError(WorkerJobStage::input, end.error);
        Frame response;
        switch (header.operation) {
        case Operation::generateSourceBaked:
            response = generateSourceBakedReply(request, proceduralDecoder, decoder, sourceBakeLimits);
            break;
        case Operation::generateLegacyBaked:
            response = generateLegacyBakedReply(request, proceduralDecoder, legacyLimits);
            break;
        case Operation::generateImportedRender:
            response = generateImportedRenderReply(request, decoder, importedLimits);
            break;
        case Operation::generateMesh:
            response = generateMeshReply(request, {});
            break;
        case Operation::generateUvMesh:
            response = generateUvMeshReply(request, {});
            break;
        case Operation::generateSourcedMesh:
            response = generateSourcedMeshReply(request, {});
            break;
        case Operation::generateIndexedSourcedMesh:
            response = generateIndexedSourcesReply(request, {});
            break;
        case Operation::generateIndexedMesh:
            response = generateIndexedMeshReply(request, {});
            break;
        }
        std::vector<uint8_t>().swap(request.payload);
        const auto reply = encodeFrame(response, {});
        const auto written = writeWorkerBytes(output, reply.data(), reply.size());
        if (!written.complete)
            throw WorkerJobIOError(WorkerJobStage::output, written.error);
    }
}
}

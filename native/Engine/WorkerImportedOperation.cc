// SPDX-License-Identifier: MIT
#include "WorkerImportedOperation.h"
#include "WorkerImportedRequest.h"
#include <algorithm>
#include <cstring>
#include <new>
namespace dust3d {
namespace worker {
    namespace {
        FailurePayload failure(FailureStatus status, const char* cause, FailureDomain domain, int32_t code)
        {
            const auto limits = FailurePayloadLimits {};
            const auto maximum = static_cast<size_t>(std::min(limits.maximumCauseBytes, limits.maximumWireBytes - detailedFailurePrefixBytes));
            size_t count = 0;
            while (count < maximum && cause[count])
                ++count;
            return { status, { cause, cause + count }, FailureDetail { domain, code, 0, static_cast<uint8_t>(cause[count] != 0) } };
        }
        Frame reply(const Header& request, FailurePayload cause)
        {
            auto header = request;
            header.kind = FrameKind::failure;
            auto bytes = encodeFailurePayload(cause, {});
            header.payloadBytes = bytes.size();
            return { header, std::move(bytes) };
        }
    }
    Frame generateImportedRenderReply(const Frame& request, ImportedRenderImageDecoder* decoder, const NativeImportedProducerLimits& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.operation != Operation::generateImportedRender)
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Imported rendering requires its dedicated opcode.");
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Imported rendering requires a request frame.");
        if (request.header.payloadBytes != request.payload.size() || request.payload.empty())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Imported request requires complete nonempty archive bytes.");
        const auto decoded = decodeImportedRequest(request.payload.data(), request.payload.size(), { Limits {}.maximumInputBytes, Limits {}.maximumInputBytes });
        const auto policy = applyImportedRequestOptions(decoded.options, limits);
        if (!decoder)
            return reply(request.header, { FailureStatus::unsupportedAsset, { 'N', 'o', ' ', 'w', 'o', 'r', 'k', 'e', 'r', ' ', 'i', 'm', 'a', 'g', 'e', ' ', 'd', 'e', 'c', 'o', 'd', 'e', 'r', '.' } });
        try {
            const auto generated = generateImportedRender(decoded.archive, *decoder, policy);
            auto header = request.header;
            header.kind = FrameKind::response;
            auto bytes = generated ? encodeImportedRenderPayload(*generated, policy.preparation.render) : std::vector<uint8_t> {};
            header.payloadBytes = bytes.size();
            return { header, std::move(bytes) };
        } catch (const ImportedImageDecoderFailure& cause) {
            return reply(request.header, cause.failure());
        } catch (const ImportedProducerNativeError& cause) {
            return reply(request.header, cause.failure());
        } catch (const ImportedProducerError& cause) {
            return reply(request.header, failure(cause.code() == ImportedProducerErrorCode::decodeLimitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::producer, static_cast<int32_t>(cause.code())));
        } catch (const ImportedSurfaceError& cause) {
            return reply(request.header, failure(cause.code() == ImportedSurfaceErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::surface, static_cast<int32_t>(cause.code())));
        } catch (const ImportedMaterialsError& cause) {
            return reply(request.header, failure(cause.code() == ImportedMaterialsErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::materials, static_cast<int32_t>(cause.code())));
        } catch (const ImportedRenderError& cause) {
            return reply(request.header, failure(cause.code() == ImportedRenderErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::render, static_cast<int32_t>(cause.code())));
        } catch (const MeshPayloadError& cause) {
            return reply(request.header, failure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::surface, static_cast<int32_t>(cause.code())));
        } catch (const std::bad_alloc& cause) {
            return reply(request.header, failure(FailureStatus::limitExceeded, cause.what(), FailureDomain::standard, 0));
        } catch (const std::exception& cause) {
            return reply(request.header, failure(FailureStatus::generationFailed, cause.what(), FailureDomain::standard, 0));
        } catch (...) {
            return reply(request.header, failure(FailureStatus::generationFailed, "Unknown imported worker failure.", FailureDomain::standard, 0));
        }
    }
}
}

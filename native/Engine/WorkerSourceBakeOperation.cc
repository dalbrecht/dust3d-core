// SPDX-License-Identifier: MIT
#include "WorkerSourceBakeOperation.h"
#include "WorkerImportedProducer.h"
#include "WorkerSourceBakeRequest.h"
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
        FailurePayload imageFailure(const SourceImageError& error)
        {
            FailurePayload result;
            try {
                std::rethrow_exception(error.cause());
            } catch (const ImportedImageDecoderFailure& cause) {
                result = cause.failure();
            } catch (const LegacyProducerError& cause) {
                const auto status = cause.code() == LegacyProducerErrorCode::decodedLimitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed;
                result = failure(status, cause.what(), FailureDomain::legacyProducer, static_cast<int32_t>(cause.code()));
            } catch (const ImportedRenderError& cause) {
                result = failure(FailureStatus::generationFailed, cause.what(), FailureDomain::render, static_cast<int32_t>(cause.code()));
            } catch (const std::bad_alloc& cause) {
                result = failure(FailureStatus::limitExceeded, cause.what(), FailureDomain::standard, 0);
            } catch (const std::exception& cause) {
                result = failure(FailureStatus::generationFailed, cause.what(), FailureDomain::standard, 0);
            } catch (...) {
                result = failure(FailureStatus::generationFailed, "Unknown source image decoder failure.", FailureDomain::standard, 0);
            }
            if (!result.detail)
                result.detail = FailureDetail { FailureDomain::imageDecoder, 0, 0, 0 };
            result.context = FailureResourceContext { error.role() == SourceImageRole::procedural ? FailureResourceRole::proceduralImage : FailureResourceRole::importedImage, error.identity() };
            const FailurePayloadLimits limits;
            const auto maximum = std::min(limits.maximumCauseBytes, limits.maximumWireBytes - contextualFailurePrefixBytes - result.context->identity.size());
            if (result.cause.size() > maximum) {
                result.cause.resize(static_cast<size_t>(maximum));
                result.detail->causeTruncated = 1;
            }
            return result;
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
    Frame generateSourceBakedReply(const Frame& request, ProceduralImageDecoder* proceduralDecoder, ImportedRenderImageDecoder* importedDecoder, const NativeSourceProducerOptions& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.operation != Operation::generateSourceBaked)
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Source baking requires its dedicated opcode.");
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Source baking requires a request frame.");
        if (request.header.payloadBytes != request.payload.size() || request.payload.empty())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Source request requires a complete nonempty body.");
        const auto decoded = decodeSourceBakeRequest(request.payload.data(), request.payload.size(), { Limits {}.maximumInputBytes, Limits {}.maximumInputBytes });
        const auto policy = applySourceBakeRequestOptions(decoded, limits);
        try {
            const auto generated = generateSourceBakedRender(decoded.legacy.archive, proceduralDecoder, importedDecoder, policy);
            auto header = request.header;
            header.kind = FrameKind::response;
            auto bytes = encodeSourceBakedPayload(generated, policy.output);
            header.payloadBytes = bytes.size();
            return { header, std::move(bytes) };
        } catch (const SourceImageError& cause) {
            return reply(request.header, imageFailure(cause));
        } catch (const SourceBakedError& cause) {
            return reply(request.header, failure(cause.code() == SourceBakedErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::sourceBaked, static_cast<int32_t>(cause.code())));
        } catch (const ImportedMaterialsError& cause) {
            return reply(request.header, failure(cause.code() == ImportedMaterialsErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::materials, static_cast<int32_t>(cause.code())));
        } catch (const ImportedImageDecoderFailure& cause) {
            return reply(request.header, cause.failure());
        } catch (const LegacyProducerNativeError& cause) {
            return reply(request.header, cause.failure());
        } catch (const LegacyProducerError& cause) {
            const auto status = (cause.code() == LegacyProducerErrorCode::decodedLimitExceeded || cause.code() == LegacyProducerErrorCode::inputLimitExceeded) ? FailureStatus::limitExceeded
                : cause.code() == LegacyProducerErrorCode::unavailableDecoder                                                                                  ? FailureStatus::unsupportedAsset
                                                                                                                                                               : FailureStatus::generationFailed;
            return reply(request.header, failure(status, cause.what(), FailureDomain::legacyProducer, static_cast<int32_t>(cause.code())));
        } catch (const ProceduralImageError& cause) {
            return reply(request.header, failure(cause.code() == ProceduralImageErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::unsupportedAsset, cause.what(), FailureDomain::proceduralImages, static_cast<int32_t>(cause.code())));
        } catch (const LegacyBakedError& cause) {
            return reply(request.header, failure(cause.code() == LegacyBakedErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::legacyBaked, static_cast<int32_t>(cause.code())));
        } catch (const BakedTexturesError& cause) {
            return reply(request.header, failure(cause.code() == BakedTexturesErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::bakedTextures, static_cast<int32_t>(cause.code())));
        } catch (const UvPayloadError& cause) {
            return reply(request.header, failure(cause.code() == UvPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::uv, static_cast<int32_t>(cause.code())));
        } catch (const ImportedRenderError& cause) {
            return reply(request.header, failure(cause.code() == ImportedRenderErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::render, static_cast<int32_t>(cause.code())));
        } catch (const MeshPayloadError& cause) {
            return reply(request.header, failure(cause.code() == MeshPayloadErrorCode::limitExceeded ? FailureStatus::limitExceeded : FailureStatus::generationFailed, cause.what(), FailureDomain::mesh, static_cast<int32_t>(cause.code())));
        } catch (const std::bad_alloc& cause) {
            return reply(request.header, failure(FailureStatus::limitExceeded, cause.what(), FailureDomain::standard, 0));
        } catch (const std::exception& cause) {
            return reply(request.header, failure(FailureStatus::generationFailed, cause.what(), FailureDomain::standard, 0));
        } catch (...) {
            return reply(request.header, failure(FailureStatus::generationFailed, "Unknown source-bake worker failure.", FailureDomain::standard, 0));
        }
    }
}
}

// SPDX-License-Identifier: MIT
#include "WorkerImageDecoder.h"
#include "WorkerImportedRenderValidation.h"
#include "WorkerUtf8.h"
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>
using namespace dust3d::worker;
struct Dust3DWorkerImage {
    ImportedRenderImage value;
};
namespace {
Dust3DWorkerImageResult diagnostic(Dust3DWorkerImageStatus status, const char* cause) noexcept
{
    return dust3d_worker_image_error_create(status, 0, reinterpret_cast<const uint8_t*>(cause), std::strlen(cause));
}
bool isFailure(Dust3DWorkerImageStatus status) noexcept
{
    return status >= DUST3D_WORKER_IMAGE_INVALID_INPUT && status <= DUST3D_WORKER_IMAGE_INTERNAL_FAILED;
}
}
extern "C" Dust3DWorkerImageResult dust3d_worker_image_error_create(Dust3DWorkerImageStatus status, int32_t code, const uint8_t* cause, size_t count) noexcept
{
    Dust3DWorkerImageResult result {};
    if (!isFailure(status) || (!cause && count)) {
        result.status = DUST3D_WORKER_IMAGE_INVALID_INPUT;
        return result;
    }
    result.status = status;
    result.code = code;
    result.cause_length = static_cast<uint32_t>(std::min(count, sizeof(result.cause)));
    result.cause_truncated = count > sizeof(result.cause);
    if (result.cause_length)
        std::memcpy(result.cause, cause, result.cause_length);
    return result;
}
extern "C" Dust3DWorkerImage* dust3d_worker_image_create(uint32_t width, uint32_t height, const uint8_t* pixels, size_t count, uint64_t maximum, Dust3DWorkerImageResult* result) noexcept
{
    if (!result)
        return nullptr;
    *result = {};
    try {
        if (!maximum || maximum > maximumPayloadBytes || !pixels) {
            *result = diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, "Worker image policy or readable pixel span is missing.");
            return nullptr;
        }
        detail::validateImportedRenderImage(width, height, count, true);
        if (count > maximum) {
            *result = diagnostic(DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED, "Worker image exceeds its output quota.");
            return nullptr;
        }
        auto image = std::make_unique<Dust3DWorkerImage>();
        image->value = { width, height, { pixels, pixels + count } };
        return image.release();
    } catch (const ImportedRenderError& cause) {
        *result = diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, cause.what());
    } catch (const std::bad_alloc& cause) {
        *result = diagnostic(DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED, cause.what());
    } catch (const std::exception& cause) {
        *result = diagnostic(DUST3D_WORKER_IMAGE_INTERNAL_FAILED, cause.what());
    } catch (...) {
        *result = diagnostic(DUST3D_WORKER_IMAGE_INTERNAL_FAILED, "Unknown worker image failure.");
    }
    return nullptr;
}
extern "C" void dust3d_worker_image_destroy(Dust3DWorkerImage* image) noexcept { delete image; }
extern "C" uint32_t dust3d_worker_image_width(const Dust3DWorkerImage* image) noexcept { return image ? image->value.width : 0; }
extern "C" uint32_t dust3d_worker_image_height(const Dust3DWorkerImage* image) noexcept { return image ? image->value.height : 0; }
extern "C" uint64_t dust3d_worker_image_byte_count(const Dust3DWorkerImage* image) noexcept { return image ? image->value.pixels.size() : 0; }
extern "C" uint8_t dust3d_worker_image_copy_pixels(const Dust3DWorkerImage* image, uint64_t offset, uint8_t* destination, size_t count) noexcept
{
    if (!image || offset > image->value.pixels.size() || count > image->value.pixels.size() - offset || (!destination && count))
        return 0;
    if (count)
        std::memcpy(destination, image->value.pixels.data() + static_cast<size_t>(offset), count);
    return 1;
}
namespace dust3d {
namespace worker {
    WorkerImageDecoderError::WorkerImageDecoderError(const Dust3DWorkerImageResult& result)
        : ImportedImageDecoderFailure(std::string(reinterpret_cast<const char*>(result.cause), result.cause_length))
        , m_result(result)
    {
    }
    FailurePayload WorkerImageDecoderError::failure() const
    {
        return { m_result.status == DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED ? FailureStatus::limitExceeded : FailureStatus::unsupportedAsset,
            { m_result.cause, m_result.cause + m_result.cause_length },
            FailureDetail { FailureDomain::imageDecoder, m_result.code, static_cast<uint32_t>(m_result.status), m_result.cause_truncated } };
    }
    CallbackImportedImageDecoder::CallbackImportedImageDecoder(Dust3DWorkerImageDecoder configuration)
        : m_configuration(configuration)
    {
        if (configuration.struct_size != sizeof(configuration) || !configuration.decode)
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, "Worker image decoder configuration is invalid."));
    }
    ImportedRenderImage CallbackImportedImageDecoder::decode(const ImportedEncodedImage& encoded, std::string_view model, uint64_t maximumEncoded, uint64_t maximumDecoded)
    {
        return decodeBorrowed(encoded.bytes.data(), encoded.bytes.size(), encoded.encoding, model, maximumEncoded, maximumDecoded);
    }
    ImportedRenderImage CallbackImportedImageDecoder::decodeBorrowed(const uint8_t* bytes, size_t count, ImportedImageEncoding encoding,
        std::string_view model, uint64_t maximumEncoded, uint64_t maximumDecoded)
    {
        if (!maximumEncoded || maximumEncoded > maximumPayloadBytes || !maximumDecoded || maximumDecoded > maximumPayloadBytes
            || (!bytes || !count) || model.empty() || model.size() > 38 || !detail::isIdentityUtf8(model)
            || (encoding != ImportedImageEncoding::png && encoding != ImportedImageEncoding::jpeg))
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, "Worker image callback input or policies are invalid."));
        if (count > maximumEncoded)
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED, "Worker encoded image exceeds its input quota."));
        Dust3DWorkerImageInput input { sizeof(Dust3DWorkerImageInput), bytes, count, reinterpret_cast<const uint8_t*>(model.data()), model.size(), static_cast<uint32_t>(encoding), maximumEncoded, maximumDecoded };
        Dust3DWorkerImageResult result {};
        std::unique_ptr<Dust3DWorkerImage, decltype(&dust3d_worker_image_destroy)> image(m_configuration.decode(m_configuration.context, &input, &result), dust3d_worker_image_destroy);
        if (result.cause_length > sizeof(result.cause) || result.cause_truncated > 1 || (result.status != DUST3D_WORKER_IMAGE_OK && !isFailure(result.status)))
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, "Worker decoder returned an invalid diagnostic layout."));
        if (result.status != DUST3D_WORKER_IMAGE_OK)
            throw WorkerImageDecoderError(result);
        if (!image || result.code || result.cause_length || result.cause_truncated)
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_INVALID_INPUT, "Worker decoder success receipt is inconsistent."));
        if (image->value.pixels.size() > maximumDecoded)
            throw WorkerImageDecoderError(diagnostic(DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED, "Worker decoder returned pixels beyond its supplied quota."));
        detail::validateImportedRenderImage(image->value.width, image->value.height, image->value.pixels.size(), true);
        return std::move(image->value);
    }
    CallbackProceduralImageDecoder::CallbackProceduralImageDecoder(Dust3DWorkerImageDecoder configuration)
        : m_decoder(configuration)
    {
    }
    ProceduralDecodedImage CallbackProceduralImageDecoder::decode(const ProceduralImageInput& input, uint64_t maximumEncoded, uint64_t maximumDecoded)
    {
        return m_decoder.decodeBorrowed(input.encoded.data(), input.encoded.size(), ImportedImageEncoding::png,
            input.identity, maximumEncoded, maximumDecoded);
    }

}
}

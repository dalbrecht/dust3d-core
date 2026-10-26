// SPDX-License-Identifier: MIT
#include <Dust3DWorkerImageDecoder.h>
#include <cstring>
#include <iostream>
#include <native/WorkerRuntime/WorkerImageDecoder.h>
#include <stdexcept>
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
using namespace dust3d::worker;
struct CallbackState {
    int mode = 0;
    size_t calls = 0;
    uint32_t encoding = 1;
};
Dust3DWorkerImage* callback(void* context, const Dust3DWorkerImageInput* input, Dust3DWorkerImageResult* result)
{
    auto& state = *static_cast<CallbackState*>(context);
    ++state.calls;
    require(input->struct_size == sizeof(*input) && input->encoded_count == 3 && input->encoded[0] == 7
            && input->model_count == 5 && std::memcmp(input->model, "Model", 5) == 0
            && input->encoding == state.encoding && input->maximum_encoded_bytes == 3 && input->maximum_decoded_bytes == 4,
        "Callback receives actual scoped bytes, exact identity and independent quotas");
    uint8_t pixels[] = { 11, 23, 37, 0, 1, 2, 3, 4 };
    auto* image = state.mode == 1 ? nullptr : dust3d_worker_image_create(state.mode == 10 ? 2 : 1, 1, pixels, state.mode == 10 ? 8 : 4, 8, result);
    if (state.mode == 2 || state.mode == 3) {
        const uint8_t cause[] = { 'a', 0, 255 };
        *result = dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_DECODE_FAILED, 42, cause, sizeof(cause));
        if (state.mode == 3) {
            dust3d_worker_image_destroy(image);
            image = nullptr;
        }
    }
    if (state.mode == 4)
        result->cause_length = DUST3D_WORKER_IMAGE_CAUSE_BYTES + 1;
    if (state.mode == 5)
        result->cause_truncated = 2;
    if (state.mode == 6)
        result->status = static_cast<Dust3DWorkerImageStatus>(5);
    if (state.mode == 7)
        result->code = 1;
    if (state.mode == 8)
        result->cause_length = 1;
    if (state.mode == 9)
        result->cause_truncated = 1;
    return image;
}
void adapter()
{
    CallbackState state;
    Dust3DWorkerImageDecoder configuration { sizeof(Dust3DWorkerImageDecoder), &state, callback };
    CallbackImportedImageDecoder decoder(configuration);
    ImportedEncodedImage encoded { ImportedImageEncoding::png, { 7, 8, 9 } };
    auto image = decoder.decode(encoded, "Model", 3, 4);
    encoded.bytes[0] = 0;
    require(image.pixels == std::vector<uint8_t>({ 11, 23, 37, 0 }), "Moved callback pixels outlive independent handle and input storage");
    encoded.bytes[0] = 7;
    encoded.encoding = ImportedImageEncoding::jpeg;
    state.encoding = 2;
    require(decoder.decode(encoded, "Model", 3, 4).pixels.size() == 4, "JPEG declaration crosses callback unchanged without platform decoding");
    encoded.encoding = ImportedImageEncoding::png;
    state.encoding = 1;
    for (int mode = 1; mode <= 10; ++mode) {
        state.mode = mode;
        bool rejected = false;
        try {
            decoder.decode(encoded, "Model", 3, 4);
        } catch (const WorkerImageDecoderError& cause) {
            const auto& result = cause.result();
            const auto failure = cause.failure();
            const auto wire = encodeFailurePayload(failure, {});
            const auto receipt = decodeFailurePayload(wire.data(), wire.size(), {});
            require(receipt.detail && receipt.detail->domain == FailureDomain::imageDecoder && receipt.detail->code == result.code
                    && receipt.detail->status == static_cast<uint32_t>(result.status) && receipt.detail->causeTruncated == result.cause_truncated
                    && receipt.cause == std::vector<uint8_t>(result.cause, result.cause + result.cause_length),
                "Callback status, code, truncation and opaque diagnostics survive explicit failure transport");
            if (mode == 2 || mode == 3)
                rejected = result.status == DUST3D_WORKER_IMAGE_DECODE_FAILED && result.code == 42 && result.cause_length == 3
                    && result.cause[0] == 'a' && result.cause[1] == 0 && result.cause[2] == 255;
            else
                rejected = result.status == (mode == 10 ? DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED : DUST3D_WORKER_IMAGE_INVALID_INPUT);
        }
        require(rejected, "Original decoder failures survive and inconsistent receipts reject");
    }
    for (int field = 0; field < 2; ++field) {
        auto invalid = configuration;
        if (field == 0)
            invalid.struct_size = 0;
        if (field == 1)
            invalid.decode = nullptr;
        bool rejected = false;
        try {
            CallbackImportedImageDecoder unused(invalid);
        } catch (const WorkerImageDecoderError& cause) {
            rejected = cause.result().status == DUST3D_WORKER_IMAGE_INVALID_INPUT;
        }
        require(rejected, "Bad callback configuration rejects before invocation");
    }
    const auto calls = state.calls;
    for (int field = 0; field < 9; ++field) {
        auto value = encoded;
        auto inputLimit = uint64_t(3), outputLimit = uint64_t(4);
        std::string model = "Model";
        if (field == 0)
            inputLimit = 0;
        if (field == 1)
            inputLimit = maximumPayloadBytes + 1;
        if (field == 2)
            outputLimit = 0;
        if (field == 3)
            outputLimit = maximumPayloadBytes + 1;
        if (field == 4)
            value.bytes.clear();
        if (field == 5)
            model.clear();
        if (field == 6)
            model = std::string(39, 'a');
        if (field == 7)
            model = std::string("a\0b", 3);
        if (field == 8)
            value.encoding = static_cast<ImportedImageEncoding>(3);
        bool rejected = false;
        try {
            decoder.decode(value, model, inputLimit, outputLimit);
        } catch (const WorkerImageDecoderError& cause) {
            rejected = cause.result().status == DUST3D_WORKER_IMAGE_INVALID_INPUT;
        }
        require(rejected && calls == state.calls, "Bad scalar input rejects before callback");
    }
    bool limited = false;
    try {
        decoder.decode(encoded, "Model", 2, 4);
    } catch (const WorkerImageDecoderError& cause) {
        limited = cause.result().status == DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED;
    }
    require(limited && calls == state.calls, "Encoded input limit rejects before callback");
}
void diagnostics()
{
    std::vector<uint8_t> bytes(513, 255);
    auto result = dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_DECODE_FAILED, -77, bytes.data(), bytes.size());
    require(result.code == -77 && result.cause_length == 512 && result.cause_truncated && result.cause[511] == 255,
        "Bounded opaque cause truncation is explicit");
    result = dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_DECODE_FAILED, 8, nullptr, 0);
    require(result.status == DUST3D_WORKER_IMAGE_DECODE_FAILED && !result.cause_length, "Empty original cause remains empty");
    for (auto status : { DUST3D_WORKER_IMAGE_OK, static_cast<Dust3DWorkerImageStatus>(5) })
        require(dust3d_worker_image_error_create(status, 0, nullptr, 0).status == DUST3D_WORKER_IMAGE_INVALID_INPUT, "Failure factory rejects invalid status");
    require(dust3d_worker_image_error_create(DUST3D_WORKER_IMAGE_DECODE_FAILED, 0, nullptr, 1).status == DUST3D_WORKER_IMAGE_INVALID_INPUT,
        "Missing diagnostic span rejects without access");
}

}
int main()
{
    try {
        adapter();
        diagnostics();
        uint8_t pixels[] = { 17, 23, 41, 0 };
        Dust3DWorkerImageResult result {};
        auto* image = dust3d_worker_image_create(1, 1, pixels, sizeof(pixels), 4, &result);
        require(image && result.status == DUST3D_WORKER_IMAGE_OK, "Tiny straight RGBA image creates an independent owner");
        pixels[0] = 0;
        require(dust3d_worker_image_width(image) == 1 && dust3d_worker_image_height(image) == 1
                && dust3d_worker_image_byte_count(image) == 4,
            "Image dimensions and tight count remain explicit");
        uint8_t copy[4] {};
        require(dust3d_worker_image_copy_pixels(image, 0, copy, 4) && copy[0] == 17 && copy[3] == 0,
            "Copied original transparent RGB outlives input storage");
        require(!dust3d_worker_image_copy_pixels(image, 4, copy, 1), "Out-of-range copy rejects before writing");
        require(!dust3d_worker_image_copy_pixels(nullptr, 0, copy, 1)
                && !dust3d_worker_image_copy_pixels(image, 5, copy, 0)
                && !dust3d_worker_image_copy_pixels(image, 0, nullptr, 1)
                && dust3d_worker_image_copy_pixels(image, 4, nullptr, 0)
                && dust3d_worker_image_copy_pixels(image, 0, copy, 0),
            "Missing owners, ranges and null destinations reject before writes; empty copies accept");
        require(!dust3d_worker_image_width(nullptr) && !dust3d_worker_image_height(nullptr) && !dust3d_worker_image_byte_count(nullptr), "Null owner getters return zero");
        require(!dust3d_worker_image_create(1, 1, pixels, 4, 4, nullptr), "Missing required result rejects before allocation");
        for (int field = 0; field < 3; ++field) {
            const auto quota = field == 0 ? uint64_t(0) : field == 1 ? maximumPayloadBytes + 1
                                                                     : uint64_t(4);
            auto* rejected = dust3d_worker_image_create(1, 1, field == 2 ? nullptr : pixels, 4, quota, &result);
            require(!rejected && result.status == DUST3D_WORKER_IMAGE_INVALID_INPUT, "Bad scalar policies/storage reject before pixel access");
        }
        dust3d_worker_image_destroy(image);
        image = dust3d_worker_image_create(1, 1, pixels, sizeof(pixels), 3, &result);
        require(!image && result.status == DUST3D_WORKER_IMAGE_LIMIT_EXCEEDED, "Output quota rejects before retained pixel copy");
        image = dust3d_worker_image_create(2, 1, pixels, sizeof(pixels), 8, &result);
        require(!image && result.status == DUST3D_WORKER_IMAGE_INVALID_INPUT, "Wrong tight image shape rejects");
        dust3d_worker_image_destroy(nullptr);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

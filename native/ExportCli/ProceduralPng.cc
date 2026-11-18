// SPDX-License-Identifier: MIT
#include "ProceduralPng.h"
#include <cstring>
#include <png.h>

namespace dust3d::export_cli {
namespace {
    void recordPngError(png_structp png, png_const_charp message)
    {
        auto* diagnostic = static_cast<char*>(png_get_error_ptr(png));
        std::strncpy(diagnostic, message, 255);
        diagnostic[255] = '\0';
        png_longjmp(png, 1);
    }

    void discardPngWarning(png_structp, png_const_charp)
    {
    }

    struct PngReader {
        char diagnostic[256] {};
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, diagnostic, recordPngError, discardPngWarning);
        png_infop info = png ? png_create_info_struct(png) : nullptr;

        ~PngReader()
        {
            if (png)
                png_destroy_read_struct(&png, &info, nullptr);
        }
    };

    struct PngSource {
        const std::vector<uint8_t>& bytes;
        size_t offset = 0;
    };

    void readBytes(png_structp png, png_bytep destination, png_size_t count)
    {
        auto& source = *static_cast<PngSource*>(png_get_io_ptr(png));
        if (count > source.bytes.size() - source.offset)
            png_error(png, "Truncated PNG");
        std::memcpy(destination, source.bytes.data() + source.offset, count);
        source.offset += count;
    }

    bool readHeader(PngReader& reader, PngSource& source)
    {
        if (setjmp(png_jmpbuf(reader.png)))
            return false;
        png_set_read_fn(reader.png, &source, readBytes);
        png_set_crc_action(reader.png, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
        png_read_info(reader.png, reader.info);
        return true;
    }

    bool readPixels(PngReader& reader, std::vector<png_bytep>& rows, size_t rowBytes)
    {
        if (setjmp(png_jmpbuf(reader.png)))
            return false;
        const auto color = png_get_color_type(reader.png, reader.info);
        const bool keyed = png_get_valid(reader.png, reader.info, PNG_INFO_tRNS);
        png_set_expand(reader.png);
        png_set_scale_16(reader.png);
        png_set_gray_to_rgb(reader.png);
        if (!(color & PNG_COLOR_MASK_ALPHA) && !keyed)
            png_set_add_alpha(reader.png, 255, PNG_FILLER_AFTER);
        png_set_interlace_handling(reader.png);
        png_read_update_info(reader.png, reader.info);
        if (png_get_rowbytes(reader.png, reader.info) != rowBytes)
            return false;
        png_read_image(reader.png, rows.data());
        png_read_end(reader.png, nullptr);
        return true;
    }
}

worker::ProceduralDecodedImage ProceduralPngDecoder::decode(const worker::ProceduralImageInput& input,
    uint64_t maximumEncodedBytes, uint64_t maximumDecodedBytes)
{
    const auto fail = [&](const char* reason) {
        return std::runtime_error(std::string(reason) + " procedural PNG: " + input.assetName);
    };
    if (input.encoded.size() > maximumEncodedBytes)
        throw fail("Encoded byte limit exceeded for");
    PngReader reader;
    PngSource source { input.encoded };
    const auto decodeError = [&] {
        return std::runtime_error(std::string(fail("Could not decode").what())
            + (reader.diagnostic[0] ? ": " + std::string(reader.diagnostic) : ""));
    };
    if (!reader.info || !readHeader(reader, source))
        throw decodeError();
    const auto width = png_get_image_width(reader.png, reader.info);
    const auto height = png_get_image_height(reader.png, reader.info);
    if (width > 8192 || height > 8192 || uint64_t(width) * height * 4 > maximumDecodedBytes)
        throw fail("Decoded size limit exceeded for");

    const auto color = png_get_color_type(reader.png, reader.info);
    const auto depth = png_get_bit_depth(reader.png, reader.info);
    const bool clearKeyedGray = color == PNG_COLOR_TYPE_GRAY && depth >= 2 && depth <= 8
        && png_get_valid(reader.png, reader.info, PNG_INFO_tRNS);
    const size_t rowBytes = size_t(width) * 4;
    worker::ProceduralDecodedImage image { width, height, std::vector<uint8_t>(rowBytes * height) };
    std::vector<png_bytep> rows;
    rows.reserve(height);
    for (size_t row = 0; row < height; ++row)
        rows.push_back(image.pixels.data() + row * rowBytes);
    if (!readPixels(reader, rows, rowBytes))
        throw decodeError();
    if (clearKeyedGray) {
        for (size_t pixel = 0; pixel < image.pixels.size(); pixel += 4) {
            if (!image.pixels[pixel + 3])
                std::memset(image.pixels.data() + pixel, 0, 3);
        }
    }
    return image;
}
}

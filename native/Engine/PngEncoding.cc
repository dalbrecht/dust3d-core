// SPDX-License-Identifier: MIT
#include "PngEncoding.h"
#include "EngineInternal.h"
#include <algorithm>
#include <array>
#include <zlib.h>
namespace dust3d {
namespace native {
    PngEncodingPlan planPngEncoding(size_t width, size_t height, size_t pixelBytes, size_t maximumBytes)
    {
        if (!width || !height)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "PNG requires nonempty dimensions.");
        if (width > 8192 || height > 8192)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "PNG dimensions exceed8192.");
        const auto rowBytes = width * 4;
        const auto requiredBytes = rowBytes * height;
        if (pixelBytes != requiredBytes)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "PNG requires exact tightly packed RGBA8 bytes.");
        const auto maximum = maximumBytes ? maximumBytes : 64 * 1024 * 1024;
        if (maximum < 57 || maximum > 64 * 1024 * 1024)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "PNG encoded quota must be57bytes...64MiB.");
        return { rowBytes, requiredBytes, maximum };
    }
    namespace {
        [[noreturn]] void compressionFailure(int status)
        {
            throw DocumentError(status == Z_MEM_ERROR ? DUST3D_STATUS_LIMIT_EXCEEDED : DUST3D_STATUS_GENERATION_FAILED,
                "PNG zlib compression failed with status " + std::to_string(status) + '.');
        }
        class DeflateStream {
        public:
            DeflateStream()
            {
                const auto status = deflateInit(&stream, Z_DEFAULT_COMPRESSION);
                if (status != Z_OK)
                    compressionFailure(status);
            }
            ~DeflateStream() { deflateEnd(&stream); }
            DeflateStream(const DeflateStream&) = delete;
            DeflateStream& operator=(const DeflateStream&) = delete;
            z_stream stream {};
        };
        std::array<uint8_t, 4> bigEndian(uint32_t value)
        {
            return { static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) };
        }
        class PngOutput {
        public:
            explicit PngOutput(size_t maximum)
                : m_maximum(maximum)
            {
            }
            void append(const uint8_t* data, size_t count)
            {
                if (count > m_maximum - bytes.size())
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "PNG encoded output exceeds quota.");
                const auto required = bytes.size() + count;
                // Geometric growth avoids copying the entire image for each small PNG chunk field.
                if (required > bytes.capacity())
                    bytes.reserve(std::max(required, std::min(m_maximum, bytes.capacity() ? bytes.capacity() * 2 : size_t { 8 })));
                if (bytes.capacity() > m_maximum)
                    throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "PNG encoded capacity exceeds quota.");
                bytes.insert(bytes.end(), data, data + count);
            }
            void chunk(const std::array<uint8_t, 4>& type, const uint8_t* data, size_t count)
            {
                const auto length = bigEndian(static_cast<uint32_t>(count));
                append(length.data(), length.size());
                append(type.data(), type.size());
                append(data, count);
                auto checksum = crc32(0, type.data(), static_cast<uInt>(type.size()));
                // Empty chunks still checksum the type; do not invoke zlib's null-input reset.
                if (count)
                    checksum = crc32(checksum, data, static_cast<uInt>(count));
                const auto crc = bigEndian(static_cast<uint32_t>(checksum));
                append(crc.data(), crc.size());
            }
            std::vector<uint8_t> bytes;

        private:
            size_t m_maximum;
        };
    }
    std::vector<uint8_t> encodePng(const TextureImage& image, size_t maximumBytes)
    {
        const auto plan = planPngEncoding(image.width, image.height, image.pixels.size(), maximumBytes);
        PngOutput output(plan.maximumEncodedBytes);
        const std::array<uint8_t, 8> signature { 137, 80, 78, 71, 13, 10, 26, 10 };
        output.append(signature.data(), signature.size());
        std::array<uint8_t, 13> header {};
        const auto width = bigEndian(static_cast<uint32_t>(image.width));
        const auto height = bigEndian(static_cast<uint32_t>(image.height));
        std::copy(width.begin(), width.end(), header.begin());
        std::copy(height.begin(), height.end(), header.begin() + 4);
        header[8] = 8;
        header[9] = 6;
        output.chunk({ 'I', 'H', 'D', 'R' }, header.data(), header.size());
        DeflateStream compressor;
        std::array<uint8_t, 65536> buffer {};
        auto compress = [&](const uint8_t* data, size_t count, bool finish) {
            // zlib's historical next_in type is mutable; deflate only reads these bytes.
            compressor.stream.next_in = const_cast<Bytef*>(data);
            compressor.stream.avail_in = static_cast<uInt>(count);
            do {
                compressor.stream.next_out = buffer.data();
                compressor.stream.avail_out = static_cast<uInt>(buffer.size());
                const auto status = deflate(&compressor.stream, finish ? Z_FINISH : Z_NO_FLUSH);
                if (status != Z_OK && status != Z_STREAM_END)
                    compressionFailure(status);
                const auto produced = buffer.size() - compressor.stream.avail_out;
                if (produced)
                    output.chunk({ 'I', 'D', 'A', 'T' }, buffer.data(), produced);
                if (status == Z_STREAM_END)
                    return;
            } while (finish || compressor.stream.avail_in);
        };
        const uint8_t filter = 0;
        for (size_t row = 0; row < image.height; ++row) {
            compress(&filter, 1, false);
            compress(image.pixels.data() + row * plan.rowBytes, plan.rowBytes, false);
        }
        compress(&filter, 0, true);
        output.chunk({ 'I', 'E', 'N', 'D' }, &filter, 0);
        return std::move(output.bytes);
    }
}
}

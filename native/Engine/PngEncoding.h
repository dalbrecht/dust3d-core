// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_PNG_ENCODING_H_
#define DUST3D_NATIVE_PNG_ENCODING_H_
#include <dust3d/uv/texture_baker.h>
namespace dust3d {
namespace native {
    /// Checked RGBA layout and encoded-output quota; no allocations or byte access.
    struct PngEncodingPlan {
        size_t rowBytes;
        size_t pixelBytes;
        size_t maximumEncodedBytes;
    };
    /// Admit exact tightly packed RGBA8 spans with dimensions1...8192.
    /// @param width Pixel columns.
    /// @param height Top-to-bottom pixel rows.
    /// @param pixelBytes Exact source byte count, width*height*4.
    /// @param maximumBytes Encoded quota, zero64MiB; supported57bytes...64MiB.
    /// @return Checked row/source sizes and resolved output quota.
    /// @throws DocumentError INVALID_DOCUMENT for empty/incorrect spans; LIMIT_EXCEEDED for bounds/quota.
    PngEncodingPlan planPngEncoding(size_t width, size_t height, size_t pixelBytes, size_t maximumBytes = 0);
    /// Encode owned RGBA8 PNG bytes with system zlib; never decode caller assets or touch files.
    /// @param image Immutable tightly packed straight-alpha pixels in top-to-bottom order.
    /// @param maximumBytes Encoded quota as above, excluding source and zlib workspace.
    /// @return Independent PNG preserving all source bytes, without gamma conversion or metadata.
    /// @throws DocumentError for invalid input, exceeded quota or a zlib failure (original status retained).
    /// Allocation failures propagate. RAII releases stream state on every exit.
    /// No byte-identical compression guarantee across zlib versions. Example: encodePng(baked.color).
    std::vector<uint8_t> encodePng(const TextureImage& image, size_t maximumBytes = 0);
}
}
#endif

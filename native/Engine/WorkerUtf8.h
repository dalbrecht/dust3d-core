// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UTF8_H_
#define DUST3D_NATIVE_WORKER_UTF8_H_
#include <cstdint>
#include <string_view>
namespace dust3d {
namespace worker {
    namespace detail {
        /// Validate a borrowed UTF8 identity span without NUL; empty is a separate caller policy.
        /// No normalization, allocation or exceptions. Example: isIdentityUtf8(key).
        inline bool isIdentityUtf8(std::string_view text)
        {
            size_t index = 0;
            while (index < text.size()) {
                const auto first = static_cast<uint8_t>(text[index++]);
                if (!first)
                    return false;
                if (first < 128)
                    continue;
                uint32_t code = 0, minimum = 0;
                size_t extra = 0;
                if (first >= 0xc2 && first <= 0xdf) {
                    code = first & 0x1f;
                    minimum = 0x80;
                    extra = 1;
                } else if (first >= 0xe0 && first <= 0xef) {
                    code = first & 0x0f;
                    minimum = 0x800;
                    extra = 2;
                } else if (first >= 0xf0 && first <= 0xf4) {
                    code = first & 7;
                    minimum = 0x10000;
                    extra = 3;
                } else
                    return false;
                if (extra > text.size() - index)
                    return false;
                for (size_t next = 0; next < extra; ++next) {
                    const auto continuation = static_cast<uint8_t>(text[index++]);
                    if ((continuation & 0xc0) != 0x80)
                        return false;
                    code = (code << 6) | (continuation & 0x3f);
                }
                if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
                    return false;
            }
            return true;
        }
    }
}
}
#endif

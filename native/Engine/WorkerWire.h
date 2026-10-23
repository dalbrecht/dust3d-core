// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_WIRE_H_
#define DUST3D_NATIVE_WORKER_WIRE_H_
#include <cstddef>
#include <cstdint>

namespace dust3d {
namespace worker {
    namespace detail {
        // Callers validate complete spans first; field widths are fixed by the schema (1..8).
        inline void writeUnsigned(uint8_t* output, uint64_t value, size_t count)
        {
            for (size_t index = 0; index < count; ++index) {
                output[count - index - 1] = static_cast<uint8_t>(value & 255);
                value >>= 8;
            }
        }
        inline uint64_t readUnsigned(const uint8_t* input, size_t count)
        {
            uint64_t result = 0;
            for (size_t index = 0; index < count; ++index)
                result = (result << 8) | input[index];
            return result;
        }
    }
}
}
#endif

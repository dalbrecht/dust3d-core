// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_PROPERTY_BLOCK_H_
#define DUST3D_NATIVE_FBX_PROPERTY_BLOCK_H_
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace dust3d {
namespace native {
    /// Move-only FBX7400 property list, preserving typed binary string separators.
    /// Borrowed bytes expire on mutation/move/destruction. Moved-from values may
    /// only be destroyed or reassigned. No source engine handles, files or globals.
    /// Serialize calls on one block; referenced inputs stay unchanged during a call.
    class FbxPropertyBlock {
    public:
        static constexpr size_t maximumPropertyCount = 4096;
        /// Root plus byte capacity quota: zero selects64MiB, maximum256MiB.
        /// Throws DocumentError LIMIT_EXCEEDED for an invalid quota. Temporary
        /// growth/self-alias copies, caller inputs and allocator overhead excluded.
        explicit FbxPropertyBlock(size_t maximumPayloadBytes = 0);
        FbxPropertyBlock(const FbxPropertyBlock&) = delete;
        FbxPropertyBlock& operator=(const FbxPropertyBlock&) = delete;
        FbxPropertyBlock(FbxPropertyBlock&&) noexcept = default;
        FbxPropertyBlock& operator=(FbxPropertyBlock&&) noexcept = default;
        /// Return borrowed encoded bytes; invalidated by mutation/move/destruction. Never throws.
        const std::vector<uint8_t>& bytes() const noexcept { return m_bytes; }
        /// Return encoded property count, at most maximumPropertyCount. Never throws.
        uint32_t propertyCount() const noexcept { return m_propertyCount; }
        /// Return root plus retained byte capacity, excluding temporaries/allocator overhead. Never throws.
        size_t payloadBytes() const noexcept { return sizeof(*this) + m_bytes.capacity(); }
        /// Append one exact little-endian property atomically. Quota/count errors
        /// throw LIMIT_EXCEEDED; nonfinite floats throw INVALID_DOCUMENT; ordinary
        /// allocation failures propagate. Every failure preserves prior bytes and
        /// count. Arrays use uncompressed encoding0 with checked element/byte sizes.
        /// Strings/raw permit embedded NUL and arbitrary bytes. Examples:
        /// block.appendInt32(1); block.appendString(std::string("Mesh\0\1Geometry",14)).
        void appendBoolean(bool value);
        void appendInt16(int16_t value);
        void appendInt32(int32_t value);
        void appendInt64(int64_t value);
        void appendFloat(float value);
        void appendDouble(double value);
        void appendString(const std::string& value);
        void appendRaw(const std::vector<uint8_t>& value);
        void appendInt32Array(const std::vector<int32_t>& values);
        void appendInt64Array(const std::vector<int64_t>& values);
        void appendFloatArray(const std::vector<float>& values);
        void appendDoubleArray(const std::vector<double>& values);
        void appendBooleanArray(const std::vector<bool>& values);

    private:
        size_t admitSize(size_t headerBytes, size_t count = 0, size_t width = 1) const;
        template <typename Writer>
        void appendEncoded(size_t byteCount, Writer writer);
        template <typename Value>
        void appendScalar(uint8_t tag, Value value);
        template <typename Value>
        void appendArray(uint8_t tag, const std::vector<Value>& values);
        size_t m_maximumPayloadBytes;
        std::vector<uint8_t> m_bytes;
        uint32_t m_propertyCount = 0;
    };
}
}
#endif

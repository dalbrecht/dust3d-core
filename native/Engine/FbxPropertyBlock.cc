// SPDX-License-Identifier: MIT
#include "FbxPropertyBlock.h"
#include "EngineInternal.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>
namespace dust3d {
namespace native {
    namespace {
        static_assert(sizeof(float) == 4 && sizeof(double) == 8 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559);
        template <typename Value>
        constexpr size_t wireWidth() { return std::is_same_v<Value, bool> ? 1 : sizeof(Value); }
        template <typename Unsigned>
        void writeUnsigned(uint8_t* destination, Unsigned value) noexcept
        {
            for (size_t byte = 0; byte < sizeof(Unsigned); ++byte)
                destination[byte] = static_cast<uint8_t>(value >> (byte * 8));
        }
        template <typename Value>
        void writeScalar(uint8_t* destination, Value value) noexcept
        {
            if constexpr (std::is_same_v<Value, bool>) {
                destination[0] = value ? 1 : 0;
            } else if constexpr (std::is_floating_point_v<Value>) {
                using Bits = std::conditional_t<sizeof(Value) == 4, uint32_t, uint64_t>;
                Bits bits;
                std::memcpy(&bits, &value, sizeof(value));
                writeUnsigned(destination, bits);
            } else {
                writeUnsigned(destination, static_cast<std::make_unsigned_t<Value>>(value));
            }
        }
    }
    FbxPropertyBlock::FbxPropertyBlock(size_t maximumPayloadBytes)
        : m_maximumPayloadBytes(maximumPayloadBytes == 0 ? 64 * 1024 * 1024 : maximumPayloadBytes)
    {
        if (m_maximumPayloadBytes > 256 * 1024 * 1024 || m_maximumPayloadBytes <= sizeof(*this))
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX property quota must fit its owner and be at most 256 MiB.");
    }
    size_t FbxPropertyBlock::admitSize(size_t headerBytes, size_t count, size_t width) const
    {
        if (m_propertyCount >= maximumPropertyCount)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX node property count exceeds 4096.");
        const size_t remaining = m_maximumPayloadBytes - sizeof(*this) - m_bytes.size();
        if (headerBytes > remaining || count > (remaining - headerBytes) / width)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX property bytes exceed the retained quota.");
        return headerBytes + count * width;
    }
    template <typename Writer>
    void FbxPropertyBlock::appendEncoded(size_t byteCount, Writer writer)
    {
        const size_t previousSize = m_bytes.size();
        const size_t nextSize = previousSize + byteCount; // Every caller first admits this sum against the quota.
        if (nextSize > m_bytes.capacity()) {
            const size_t available = m_maximumPayloadBytes - sizeof(*this);
            const size_t capacity = std::max(nextSize, std::min(available, std::max(size_t(64), m_bytes.capacity() * 2)));
            std::vector<uint8_t> next;
            next.reserve(capacity);
            if (next.capacity() > available)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX property buffer capacity exceeds the retained quota.");
            next.resize(nextSize);
            std::copy(m_bytes.begin(), m_bytes.end(), next.begin());
            writer(next.data() + previousSize);
            m_bytes.swap(next);
        } else {
            m_bytes.resize(nextSize);
            writer(m_bytes.data() + previousSize);
        }
        ++m_propertyCount;
    }
    template <typename Value>
    void FbxPropertyBlock::appendScalar(uint8_t tag, Value value)
    {
        const auto size = admitSize(1 + wireWidth<Value>());
        if constexpr (std::is_floating_point_v<Value>)
            if (!std::isfinite(value))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX numeric property is nonfinite.");
        appendEncoded(size, [=](uint8_t* destination) noexcept {
            destination[0] = tag;
            writeScalar(destination + 1, value);
        });
    }
    template <typename Value>
    void FbxPropertyBlock::appendArray(uint8_t tag, const std::vector<Value>& values)
    {
        constexpr auto width = wireWidth<Value>();
        const auto size = admitSize(13, values.size(), width);
        if constexpr (std::is_floating_point_v<Value>)
            for (Value value : values)
                if (!std::isfinite(value))
                    throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX numeric array contains a nonfinite value.");
        appendEncoded(size, [&](uint8_t* destination) noexcept {
            destination[0] = tag;
            writeUnsigned(destination + 1, static_cast<uint32_t>(values.size()));
            writeUnsigned(destination + 5, uint32_t(0));
            writeUnsigned(destination + 9, static_cast<uint32_t>(values.size() * width));
            for (size_t index = 0; index < values.size(); ++index)
                writeScalar(destination + 13 + index * width, static_cast<Value>(values[index]));
        });
    }
    void FbxPropertyBlock::appendBoolean(bool value) { appendScalar('C', value); }
    void FbxPropertyBlock::appendInt16(int16_t value) { appendScalar('Y', value); }
    void FbxPropertyBlock::appendInt32(int32_t value) { appendScalar('I', value); }
    void FbxPropertyBlock::appendInt64(int64_t value) { appendScalar('L', value); }
    void FbxPropertyBlock::appendFloat(float value) { appendScalar('F', value); }
    void FbxPropertyBlock::appendDouble(double value) { appendScalar('D', value); }
    void FbxPropertyBlock::appendString(const std::string& value)
    {
        const auto size = admitSize(5, value.size());
        appendEncoded(size, [&](uint8_t* destination) noexcept {
            destination[0] = 'S';
            writeUnsigned(destination + 1, static_cast<uint32_t>(value.size()));
            if (!value.empty())
                std::memcpy(destination + 5, value.data(), value.size());
        });
    }
    void FbxPropertyBlock::appendRaw(const std::vector<uint8_t>& value)
    {
        const auto size = admitSize(5, value.size());
        if (&value == &m_bytes) {
            // Keep self-alias input stable across resize/reallocation before appending its snapshot.
            const auto copy = value;
            appendRaw(copy);
            return;
        }
        appendEncoded(size, [&](uint8_t* destination) noexcept {
            destination[0] = 'R';
            writeUnsigned(destination + 1, static_cast<uint32_t>(value.size()));
            if (!value.empty())
                std::memcpy(destination + 5, value.data(), value.size());
        });
    }
    void FbxPropertyBlock::appendInt32Array(const std::vector<int32_t>& values) { appendArray('i', values); }
    void FbxPropertyBlock::appendInt64Array(const std::vector<int64_t>& values) { appendArray('l', values); }
    void FbxPropertyBlock::appendFloatArray(const std::vector<float>& values) { appendArray('f', values); }
    void FbxPropertyBlock::appendDoubleArray(const std::vector<double>& values) { appendArray('d', values); }
    void FbxPropertyBlock::appendBooleanArray(const std::vector<bool>& values) { appendArray('b', values); }
}
}

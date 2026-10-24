// SPDX-License-Identifier: MIT
/*
Footer adapted from application/third_party/fbx/src/fbxdocument.cpp.
MIT License

Copyright (c) 2017 Jakub Skořepa

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "FbxDocument.h"
#include "EngineInternal.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <dust3d/base/retained_payload_budget.h>
#include <limits>
namespace dust3d {
namespace native {
    namespace {
        constexpr size_t maximumBytes = 256 * 1024 * 1024;
        static_assert(maximumBytes <= std::numeric_limits<uint32_t>::max());
        constexpr std::array<uint8_t, 23> magic { 'K', 'a', 'y', 'd', 'a', 'r', 'a', ' ', 'F', 'B', 'X', ' ', 'B', 'i', 'n', 'a', 'r', 'y', ' ', ' ', 0, 0x1a, 0 };
        constexpr std::array<uint8_t, 176> footer {
            0xfa,
            0xbc,
            0xab,
            0x09,
            0xd0,
            0xc8,
            0xd4,
            0x66,
            0xb1,
            0x76,
            0xfb,
            0x83,
            0x1c,
            0xf7,
            0x26,
            0x7e,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0xe8,
            0x1c,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0xf8,
            0x5a,
            0x8c,
            0x6a,
            0xde,
            0xf5,
            0xd9,
            0x7e,
            0xec,
            0xe9,
            0x0c,
            0xe3,
            0x75,
            0x8f,
            0x29,
            0x0b,
        };
        class Writer {
        public:
            explicit Writer(std::vector<uint8_t>& bytes)
                : m_bytes(bytes)
            {
            }
            size_t position() const noexcept { return m_position; }
            void write(const uint8_t* source, size_t count)
            {
                requireRemaining(count);
                if (count)
                    std::memcpy(m_bytes.data() + m_position, source, count);
                m_position += count;
            }
            void integer(uint32_t value)
            {
                std::array<uint8_t, 4> bytes {};
                for (size_t index = 0; index < bytes.size(); ++index)
                    bytes[index] = static_cast<uint8_t>(value >> (index * 8));
                write(bytes.data(), bytes.size());
            }
            void zeroes(size_t count)
            {
                requireRemaining(count);
                std::fill_n(m_bytes.begin() + m_position, count, uint8_t(0));
                m_position += count;
            }

        private:
            void requireRemaining(size_t count) const
            {
                if (count > m_bytes.size() - m_position)
                    throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FBX metadata exceeds its admitted byte buffer.");
            }
            std::vector<uint8_t>& m_bytes;
            size_t m_position = 0;
        };
        void writeNode(Writer& writer, const FbxNode& node)
        {
            const auto end = writer.position() + node.encodedSize();
            // Immutable construction/preflight bound offsets/list bytes below256MiB and names below256.
            writer.integer(static_cast<uint32_t>(end));
            writer.integer(node.properties().propertyCount());
            writer.integer(static_cast<uint32_t>(node.properties().bytes().size()));
            const auto nameSize = static_cast<uint8_t>(node.name().size());
            writer.write(&nameSize, 1);
            writer.write(node.name().data(), node.name().size());
            writer.write(node.properties().bytes().data(), node.properties().bytes().size());
            for (const auto& child : node.children())
                writeNode(writer, child);
            if (!node.children().empty())
                writer.zeroes(13);
            if (writer.position() != end)
                throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FBX node encoding changed its admitted record size.");
        }
    }
    OwnedFbxDocument encodeFbxDocument(const std::vector<FbxNode>& roots, FbxDocumentLimits limits)
    {
        const auto limit = limits.maximumPayloadBytes == 0 ? 64 * 1024 * 1024 : limits.maximumPayloadBytes;
        if (limit <= sizeof(OwnedFbxDocument) || limit > maximumBytes || limits.maximumNodes == 0 || limits.maximumNodes > 65536)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX document limits exceed supported payload or node bounds.");
        if (roots.empty())
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX document requires a nonempty node forest.");
        const auto available = limit - sizeof(OwnedFbxDocument);
        RetainedPayloadBudget wire(available);
        wire.tryAdd(magic.size());
        wire.tryAdd(4);
        wire.tryAdd(13);
        wire.tryAdd(footer.size());
        size_t count = 0;
        for (const auto& root : roots) {
            if (root.nodeCount() > limits.maximumNodes - count)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX document descendant count exceeds its node limit.");
            count += root.nodeCount();
            wire.tryAdd(root.encodedSize());
        }
        if (wire.isExceeded())
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX document bytes exceed the retained quota.");
        std::vector<uint8_t> bytes(wire.usedBytes());
        if (bytes.capacity() > available)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX document buffer capacity exceeds its retained quota.");
        Writer writer(bytes);
        writer.write(magic.data(), magic.size());
        writer.integer(7400);
        for (const auto& root : roots)
            writeNode(writer, root);
        writer.zeroes(13);
        writer.write(footer.data(), footer.size());
        if (writer.position() != bytes.size())
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "FBX document encoding changed its admitted byte count.");
        return OwnedFbxDocument(std::move(bytes));
    }
}
}

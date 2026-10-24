// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_FBX_DOCUMENT_H_
#define DUST3D_NATIVE_FBX_DOCUMENT_H_
#include "FbxNode.h"
namespace dust3d {
namespace native {
    struct FbxDocumentLimits {
        size_t maximumPayloadBytes = 0;
        size_t maximumNodes = 65536;
    };
    /// Independent immutable FBX7400 container bytes. Borrowed bytes expire on
    /// move/reassignment/destruction; moved-from values are only destructible or
    /// reassignable. No source nodes, native handles or file state are retained.
    class OwnedFbxDocument {
    public:
        OwnedFbxDocument(const OwnedFbxDocument&) = delete;
        OwnedFbxDocument& operator=(const OwnedFbxDocument&) = delete;
        OwnedFbxDocument(OwnedFbxDocument&&) noexcept = default;
        OwnedFbxDocument& operator=(OwnedFbxDocument&&) noexcept = default;
        /// Return borrowed complete container bytes. Never throws.
        const std::vector<uint8_t>& bytes() const noexcept { return m_bytes; }
        /// Return root plus retained byte capacity; temporaries, source nodes and
        /// allocator costs excluded. Never throws; no RSS limit is implied.
        size_t payloadBytes() const noexcept { return sizeof(*this) + m_bytes.capacity(); }

    private:
        friend OwnedFbxDocument encodeFbxDocument(const std::vector<FbxNode>&, FbxDocumentLimits);
        explicit OwnedFbxDocument(std::vector<uint8_t> bytes) noexcept
            : m_bytes(std::move(bytes))
        {
        }
        std::vector<uint8_t> m_bytes;
    };
    /// Preflight a nonempty immutable forest, then encode checked absolute offsets,
    /// header, node/list terminators and the reference footer into an owned buffer.
    /// Nodes must remain alive/unchanged during the call and must not be moved-from.
    /// Payload zero selects64MiB/max256MiB (root+actual capacity); nodes1...65,536.
    /// Throws DocumentError INVALID_DOCUMENT for empty input, LIMIT_EXCEEDED for
    /// invalid/exceeded quotas, GENERATION_FAILED for inconsistent metadata; ordinary
    /// allocation failures propagate. Input remains unchanged on every outcome.
    /// No engine globals, C ABI, files or geometry/scene validity claim.
    /// Example: auto output = encodeFbxDocument(roots, { 16 * 1024 * 1024, 4096 });
    OwnedFbxDocument encodeFbxDocument(const std::vector<FbxNode>& roots, FbxDocumentLimits limits = {});
}
}
#endif

// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_HINT_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_SOURCE_HINT_VALIDATION_H_
#include "WorkerSourceHints.h"
namespace dust3d {
namespace worker {
    namespace detail {
        /// Internal shared typed rejection; messages contain no source text.
        [[noreturn]] void rejectSourceHintValidation(SourceHintPayloadErrorCode code, const char* cause);
        /// Validate nonempty non-NUL UTF8 under an independently checked text bound.
        /// Throws original SourceHintPayloadError; no allocation or normalization.
        void validateSourceHintIdentity(const uint8_t* bytes, size_t size, uint64_t maximumBytes);
        /// Validate finite nonnegative original weight and bounded/sentinel identity.
        /// Unrecognized hint kinds remain opaque. Throws SourceHintPayloadError.
        void validateSourceHintInfluence(const SourceHintInfluence& value, uint64_t identities);
        /// Read one already bounds-checked24-byte record. Caller checks reserved fields
        /// and validates the result before publication. No pointers are retained.
        SourceHintInfluence readSourceHintInfluence(const uint8_t* bytes);
        SourceHintCorner readSourceHintCorner(const uint8_t* bytes);
        /// Validate only a corner's range before its influence records arrive.
        /// Throws SourceHintPayloadError(invalidReference); no lookup/allocation.
        void validateSourceHintCornerRange(const SourceHintCorner& value, uint64_t influences);
        /// Validate corner ranges and kind correspondence through checked borrowed lookup.
        /// Throws original SourceHintPayloadError; no native work or output retention.
        template <typename KindAt>
        void validateSourceHintCorner(const SourceHintCorner& value, uint64_t influences, KindAt kindAt)
        {
            validateSourceHintCornerRange(value, influences);
            switch (value.kind) {
            case SourceHintKind::nodeHint:
            case SourceHintKind::ambiguous:
                if (value.influenceCount != 1 || kindAt(value.firstInfluence) != value.kind)
                    rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidCorner, "Worker source single-hint corner is inconsistent.");
                break;
            case SourceHintKind::unknown:
                if (value.influenceCount && (value.influenceCount != 1 || kindAt(value.firstInfluence) != SourceHintKind::unknown))
                    rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidCorner, "Worker source unknown corner is inconsistent.");
                break;
            case SourceHintKind::weightedHint:
                if (!value.influenceCount)
                    rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidCorner, "Worker weighted source corner has no influences.");
                break;
            default:
                break;
            }
        }
    }
}
}
#endif

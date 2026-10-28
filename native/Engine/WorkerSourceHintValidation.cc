// SPDX-License-Identifier: MIT
#include "WorkerSourceHintValidation.h"
#include "WorkerUtf8.h"
#include "WorkerWire.h"
#include <cmath>
#include <cstring>
namespace dust3d {
namespace worker {
    namespace detail {
        [[noreturn]] void rejectSourceHintValidation(SourceHintPayloadErrorCode code, const char* cause)
        {
            throw SourceHintPayloadError(code, cause);
        }
        void validateSourceHintIdentity(const uint8_t* bytes, size_t size, uint64_t maximumBytes)
        {
            if (!size || size > maximumBytes)
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidIdentity, "Worker source identity length is invalid.");
            if (!detail::isIdentityUtf8({ reinterpret_cast<const char*>(bytes), size }))
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidIdentity, "Worker source identity is not non-NUL UTF-8.");
        }
        void validateSourceHintCornerRange(const SourceHintCorner& value, uint64_t influences)
        {
            if (value.firstInfluence > influences || value.influenceCount > influences - value.firstInfluence)
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidReference, "Worker source corner range is invalid.");
        }
        void validateSourceHintInfluence(const SourceHintInfluence& value, uint64_t identities)
        {
            if (!std::isfinite(value.weight) || value.weight < 0)
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidWeight, "Worker source weight is not finite and nonnegative.");
            if (value.kind == SourceHintKind::weightedHint)
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidReference, "A weighted source kind belongs to corners, not influences.");
            if ((value.kind == SourceHintKind::nodeHint && value.identityIndex >= identities)
                || ((value.kind == SourceHintKind::unknown || value.kind == SourceHintKind::ambiguous) && value.identityIndex != noSourceIdentity)
                || (value.identityIndex != noSourceIdentity && value.identityIndex >= identities))
                rejectSourceHintValidation(SourceHintPayloadErrorCode::invalidReference, "Worker source influence identity is invalid.");
        }
        SourceHintInfluence readSourceHintInfluence(const uint8_t* bytes)
        {
            const auto bits = static_cast<uint32_t>(detail::readUnsigned(bytes + 16, 4));
            float weight = 0;
            std::memcpy(&weight, &bits, sizeof(weight));
            return { static_cast<SourceHintKind>(detail::readUnsigned(bytes, 4)), detail::readUnsigned(bytes + 8, 8), weight };
        }
        SourceHintCorner readSourceHintCorner(const uint8_t* bytes)
        {
            return { static_cast<SourceHintKind>(detail::readUnsigned(bytes, 4)), detail::readUnsigned(bytes + 8, 8), detail::readUnsigned(bytes + 16, 8) };
        }
    }
}
}

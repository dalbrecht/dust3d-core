// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_COPY_BUDGET_H_
#define DUST3D_NATIVE_WORKER_UV_COPY_BUDGET_H_
#include "WorkerUv.h"
namespace dust3d {
namespace worker {
    namespace detail {
        /// Native component keys and chart image identities share one pre-copy text quota.
        /// Scalar-only admission keeps the boundary testable without constructing native owners.
        class UvCopyTextBudget {
        public:
            /// @param maximumBytes Already validated aggregate text allowance; no allocation occurs.
            explicit UvCopyTextBudget(uint64_t maximumBytes)
                : m_maximumBytes(maximumBytes)
            {
            }
            /// @param byteCount Original retained text length, charged once for each copied string.
            /// @throws UvPayloadError(limitExceeded) before changing prior admission.
            void charge(uint64_t byteCount)
            {
                if (byteCount > m_maximumBytes || m_chargedBytes > m_maximumBytes - byteCount)
                    throw UvPayloadError(UvPayloadErrorCode::limitExceeded, "Native worker UV text exceeds its aggregate copy quota.");
                m_chargedBytes += byteCount;
            }
            /// Return admitted bytes, including both component and chart text; no borrowed storage.
            uint64_t chargedBytes() const noexcept { return m_chargedBytes; }

        private:
            const uint64_t m_maximumBytes;
            uint64_t m_chargedBytes = 0;
        };
    }
}
}
#endif

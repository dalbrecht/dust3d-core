// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SESSION_OUTPUT_VALIDATION_H_
#define DUST3D_NATIVE_WORKER_SESSION_OUTPUT_VALIDATION_H_
#include "WorkerSessionOutput.h"
namespace dust3d {
namespace worker {
    namespace detail {
        /// Determine the complete owned outcome kind; reject a valueless variant.
        /// No encoding, native work or policy changes. Throws SessionOutputError.
        uint32_t sessionOutputKind(const SessionOutput& value);
        /// Shared source-fallback/statistics/empty invariants, including provisional
        /// decode state. Caller validates kind/layout first. Throws SessionOutputError.
        void validateSessionOutputOutcome(const SessionOutput& value, uint32_t kind);
    }
}
}
#endif

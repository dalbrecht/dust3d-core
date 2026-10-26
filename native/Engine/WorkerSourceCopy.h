// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_COPY_H_
#define DUST3D_NATIVE_WORKER_SOURCE_COPY_H_
#include "WorkerSourceHints.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    namespace detail {
        struct NativeSourceHintInspection {
            uint64_t identityCount, cornerCount, influenceCount, wireBytes, logicalBytes;
        };
        /// Preflight a stable borrowed native owner under independent source-copy quotas.
        /// @param owner Live immutable source owner, retained by caller throughout the call.
        /// @param limits Supported count/text/wire/logical policies, excluding native workspace/RSS.
        /// @return Validated storage counts and copy charges; no native pointers escape.
        /// @throws SourceHintPayloadError; no returned source arrays/text are constructed.
        /// Example: auto shape = inspectNativeSourceHints(owner.get(), {});
        NativeSourceHintInspection inspectNativeSourceHints(const Dust3DSourcedMesh* owner, const SourceHintPayloadLimits& limits);
        /// Independently preflight and copy all source values from one stable borrowed owner.
        /// @param owner Live immutable source owner, retained until return.
        /// @param limits Supported count/text/wire/logical policies.
        /// @return Fully validated independent hints; no caller-supplied count plan is trusted.
        /// @throws SourceHintPayloadError or allocation errors; no partial hints are returned.
        /// Example: auto hints = copyNativeSourceHints(owner.get(), {});
        SourceHintPayload copyNativeSourceHints(const Dust3DSourcedMesh* owner, const SourceHintPayloadLimits& limits);
    }
}
}
#endif

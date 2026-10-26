// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_GENERATION_STATISTICS_COPY_H_
#define DUST3D_NATIVE_WORKER_GENERATION_STATISTICS_COPY_H_
#include "WorkerGenerationStatistics.h"
#include <Dust3DBridge.h>
namespace dust3d {
namespace worker {
    /// Copy an actual stable native statistics snapshot into independent worker values.
    /// Caller serializes the session's generation/query pair; this function cannot
    /// establish call provenance or turn unavailable metrics into zeros. No native work.
    /// Validates scalar/cause quotas and bounded diagnostic termination before return;
    /// temporary validation bytes and root/capacity/workspace/RSS are excluded.
    /// Throws GenerationStatisticsError/allocation errors. Example: auto stats = copyNativeGenerationStatistics(native, {});
    GenerationStatistics copyNativeGenerationStatistics(const Dust3DGenerationStats& source, const GenerationStatisticsLimits& limits);
}
}
#endif

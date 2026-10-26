// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_BAKED_TEXTURE_VIEWS_H_
#define DUST3D_NATIVE_WORKER_BAKED_TEXTURE_VIEWS_H_
#include "WorkerBakedTextures.h"
namespace dust3d {
namespace worker {
    namespace detail {
        struct BakedTextureLevelView {
            uint32_t width = 0, height = 0;
            uint64_t byteCount = 0;
            const uint8_t* pixels = nullptr;
        };
        /// Fixed borrowed slots; caller guarantees every nonempty readable span stays stable.
        struct BakedTextureViews {
            std::array<uint32_t, bakedTextureRoleCount> counts {};
            std::array<std::array<BakedTextureLevelView, maximumTextureLevels>, bakedTextureRoleCount> maps {};
            double maximumSamplingLod = 0;
        };
        struct BakedTextureViewsInspection {
            uint64_t wireBytes, logicalBytes;
        };
        /// Share all owned/wire texture rules for stable borrowed native views before pixel copies.
        /// @param views Fixed role/level slots and actual readable pixel spans, retained by caller.
        /// @param limits Positive supported copy/wire/pixel quotas, excluding root/capacity/RSS.
        /// @return Exact wire and logical charges; fixed-stack inspection allocates no decoded arrays.
        /// @throws BakedTexturesError. Example: auto shape = inspectBakedTextureViews(views, {});
        BakedTextureViewsInspection inspectBakedTextureViews(const BakedTextureViews& views, const BakedTexturesLimits& limits);
        /// Independently inspect then copy stable views; no caller-supplied inspection is trusted.
        /// @param views Stable complete borrowed spans, releasable after return.
        /// @param limits Independent supported quotas.
        /// @return Owned texture value preserving exact bytes/LOD; no native pointers escape.
        /// @throws BakedTexturesError or allocation errors. Example: auto copy = copyBakedTextureViews(views, {});
        BakedTexturesPayload copyBakedTextureViews(const BakedTextureViews& views, const BakedTexturesLimits& limits);
    }
}
}
#endif

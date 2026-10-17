// SPDX-License-Identifier: MIT
#ifndef DUST3D_UV_TEXTURE_BAKER_INTERNAL_H_
#define DUST3D_UV_TEXTURE_BAKER_INTERNAL_H_

#include <dust3d/uv/texture_baker.h>

namespace dust3d {
namespace detail {
    // Internal stage: caller validates geometry/UVs and charges output first.
    // Kept separate so cancellation after validation is tested without timing races.
    std::vector<TextureTangent> buildTextureTangents(const Object& object, const UvAtlas& atlas, const TextureBakeOptions& options);
}
}
#endif

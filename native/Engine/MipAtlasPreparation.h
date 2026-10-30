// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#define DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#include <dust3d/uv/uv_atlas_builder.h>
#include <string>
namespace dust3d::native {
template<typename PaddedBuild, typename BaseBuild>
auto buildMipAtlasWithFallback(bool& mip, std::string& diagnostic, PaddedBuild padded, BaseBuild base) -> decltype(padded())
{
    if (!mip)
        return base();
    try {
        return padded();
    } catch (const UvAtlasError& error) {
        if (error.code() != UvAtlasErrorCode::packingFailed)
            throw;
        diagnostic = std::string("Mip atlas padding could not be packed; mipmaps are disabled: ") + error.what();
        mip = false;
        try {
            return base();
        } catch (const UvAtlasError& baseError) {
            const auto both = diagnostic + "; base atlas failed: " + baseError.what();
            throw UvAtlasError(baseError.code(), both.c_str());
        }
    }
}
}
#endif

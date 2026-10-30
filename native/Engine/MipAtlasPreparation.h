// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#define DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#include <dust3d/uv/uv_atlas_builder.h>
#include <dust3d/uv/texture_baker.h>
#include <algorithm>
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
template<typename ResolveInputs>
UvAtlas buildNormalAwareMipAtlas(const Object& object, const std::map<Uuid, UvAtlasComponent>& components,
    UvAtlasOptions options, const TextureBakeOptions& planningOptions, double normalPadding,
    bool& mip, std::string& diagnostic, ResolveInputs resolveInputs)
{
    auto build = [&] { return UvAtlasBuilder::build(object, components, options); };
    auto base = [&] { options.paddingFraction = 0; options.usePackedChartExtents = false; return build(); };
    auto atlas = buildMipAtlasWithFallback(mip, diagnostic, build, base);
    if (mip && std::any_of(atlas.triangleUvKnown.begin(), atlas.triangleUvKnown.end(), [](uint8_t known) { return known != 0; })
        && normalPadding != options.paddingFraction
        && TextureBaker::willProduceNormal(object, atlas, resolveInputs(), planningOptions)) {
        options.paddingFraction = normalPadding;
        atlas = UvAtlas {};
        atlas = buildMipAtlasWithFallback(mip, diagnostic, build, base);
    }
    return atlas;
}

}
#endif

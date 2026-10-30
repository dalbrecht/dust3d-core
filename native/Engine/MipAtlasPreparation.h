// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#define DUST3D_NATIVE_MIP_ATLAS_PREPARATION_H_
#include <algorithm>
#include <dust3d/uv/texture_baker.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
namespace dust3d::native {
class AtlasRetryAllocationError : public std::bad_alloc {
public:
    explicit AtlasRetryAllocationError(std::string message) noexcept
        : m_message(std::move(message)) { }
    const char* what() const noexcept override { return m_message.c_str(); }
private:
    std::string m_message;
};
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
        // Prepare this while allocation is still available so a failed retry can
        // retain its original packing context without allocating in the handler.
        auto allocationDiagnostic = diagnostic + "; base atlas failed: allocation failure";
        try {
            return base();
        } catch (const UvAtlasError& baseError) {
            const auto both = diagnostic + "; base atlas failed: " + baseError.what();
            throw UvAtlasError(baseError.code(), both.c_str());
        } catch (const std::bad_alloc&) {
            throw AtlasRetryAllocationError(std::move(allocationDiagnostic));
        } catch (const std::exception& baseError) {
            std::throw_with_nested(std::runtime_error(diagnostic + "; base atlas failed: " + baseError.what()));
        } catch (...) {
            std::throw_with_nested(std::runtime_error(diagnostic + "; base atlas failed with a non-standard exception"));
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

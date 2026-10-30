// SPDX-License-Identifier: MIT
#include <dust3d/base/retained_payload_budget.h>
#include <iostream>
#include <native/Engine/EngineInternal.h>
#include <native/Engine/SourceBakedMesh.h>
#include <stdexcept>

namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void fallbackWarningPreservesCauseAndChargesOwnedGrowth()
{
    std::string warning = "Existing diagnostic.";
    dust3d::RetainedPayloadBudget budget(4096);
    budget.accountDynamic(warning);
    const auto before = budget.usedBytes();
    const auto previousCapacity = warning.capacity();
    dust3d::native::appendSourceMipFallbackWarning(warning, "Known procedural mip triangle has no single owning island; mipmaps are disabled.", budget);
    require(warning == "Existing diagnostic. Known procedural mip triangle has no single owning island; mipmaps are disabled.", "Both original causes remain visible");
    require(budget.usedBytes() == before + warning.capacity() - previousCapacity, "Warning capacity growth is charged exactly");
    const auto retained = warning;
    const auto charge = budget.usedBytes();
    dust3d::native::appendSourceMipFallbackWarning(warning, "", budget);
    require(warning == retained && budget.usedBytes() == charge, "An empty diagnostic does not alter the warning or charge");
}
void fallbackWarningRejectsInsufficientQuota()
{
    std::string warning;
    dust3d::RetainedPayloadBudget budget(warning.capacity() + 1);
    budget.accountDynamic(warning);
    try {
        dust3d::native::appendSourceMipFallbackWarning(warning, "Known procedural mip triangle has no single owning island; mipmaps are disabled.", budget);
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_LIMIT_EXCEEDED, "Quota failure preserves typed limit status");
        return;
    }
    throw std::runtime_error("Warning growth exceeded its quota without a failure");
}
void paddingHeuristicUsesSmallestBoundedMap()
{
    require(dust3d::native::sourceMipPaddingFraction(256, 64, 128, true) == .0625, "Smallest map sets four-texel padding");
    require(dust3d::native::sourceMipPaddingFraction(1, 8192, 8192, true) == .125, "Small maps cap the padding fraction");
    require(dust3d::native::sourceMipPaddingFraction(256, 256, 1, false) == .015625, "Absent normal map must not affect padding");
    require(dust3d::native::sourceMipPaddingFraction(256, 256, 1, true) == .125, "Enabled normal map determines padding");
    try {
        (void)dust3d::native::sourceMipPaddingFraction(0, 64, 128, true);
        throw std::runtime_error("Zero map dimension unexpectedly succeeded");
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid map dimension retains typed status");
    }
}
}
int main()
{
    try {
        fallbackWarningPreservesCauseAndChargesOwnedGrowth();
        fallbackWarningRejectsInsufficientQuota();
        paddingHeuristicUsesSmallestBoundedMap();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

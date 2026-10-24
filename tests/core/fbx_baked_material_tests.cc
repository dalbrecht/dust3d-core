// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxBakedMaterial.h"
#include "PngEncoding.h"
#include <iostream>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "Baked FBX material lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid baked FBX material accepted");
}
}
int main()
{
    try {
        std::array<TextureImage, 6> images;
        FbxBakedImageViews views {};
        for (size_t channel = 0; channel < images.size(); ++channel) {
            images[channel] = { 2, 2, std::vector<uint8_t>(16, static_cast<uint8_t>(20 + channel * 30)) };
            views[channel] = &images[channel];
        }
        const auto material = makeFbxBakedMaterial(views);
        require(material.baseColor == std::array<double, 4>({ 1, 1, 1, 1 }) && material.metallic == 1 && material.roughness == 1,
            "Baked maps received non-neutral color/scalar multipliers");
        size_t retained = 0;
        for (size_t channel = 0; channel < images.size(); ++channel) {
            require(material.pngMaps[channel] == encodePng(images[channel]), "Baked map swapped channel or changed pixels");
            retained += material.pngMaps[channel].capacity();
        }
        require(retained <= 64 * 1024 * 1024, "Encoded map capacities exceeded aggregate quota");
        const auto owned = material.pngMaps;
        images[0].pixels[0] = 0;
        require(material.pngMaps == owned, "Material retained caller pixel views");
        FbxBakedImageViews colorOnly {};
        colorOnly[0] = &images[0];
        const auto simple = makeFbxBakedMaterial(colorOnly);
        require(simple.metallic == 0 && simple.roughness == 1 && !simple.pngMaps[0].empty(), "Absent maps lost dielectric defaults");
        for (size_t channel = 1; channel < simple.pngMaps.size(); ++channel)
            require(simple.pngMaps[channel].empty(), "Missing optional baked channel invented");
        rejects([] { makeFbxBakedMaterial({}); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([&] { makeFbxBakedMaterial(views, 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { makeFbxBakedMaterial(views, 67108865); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { makeFbxBakedMaterial(views, 57); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { makeFbxBakedMaterial(views, 128); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        images[5].pixels.pop_back();
        rejects([&] { makeFbxBakedMaterial(views); }, DUST3D_STATUS_INVALID_DOCUMENT);
        std::cout << "Owned baked FBX material contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

/*
 *  Copyright (c) 2026 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <dust3d/animation/surface_material.h>
namespace dust3d {
const std::array<SurfaceMaterial, 8>& surfaceMaterials() noexcept
{
    static constexpr std::array<SurfaceMaterial, 8> materials { SurfaceMaterial::Stone, SurfaceMaterial::Wood,
        SurfaceMaterial::Sand, SurfaceMaterial::Metal, SurfaceMaterial::Grass, SurfaceMaterial::Water,
        SurfaceMaterial::Dirt, SurfaceMaterial::Snow };
    return materials;
}
std::string surfaceMaterialName(SurfaceMaterial material)
{
    switch (material) {
    case SurfaceMaterial::Stone:
        return "Stone";
    case SurfaceMaterial::Wood:
        return "Wood";
    case SurfaceMaterial::Sand:
        return "Sand";
    case SurfaceMaterial::Metal:
        return "Metal";
    case SurfaceMaterial::Grass:
        return "Grass";
    case SurfaceMaterial::Water:
        return "Water";
    case SurfaceMaterial::Dirt:
        return "Dirt";
    case SurfaceMaterial::Snow:
        return "Snow";
    }
    return "Stone";
}

SurfaceMaterial surfaceMaterialFromName(const std::string& name)
{
    if (name == "Wood")
        return SurfaceMaterial::Wood;
    if (name == "Sand")
        return SurfaceMaterial::Sand;
    if (name == "Metal")
        return SurfaceMaterial::Metal;
    if (name == "Grass")
        return SurfaceMaterial::Grass;
    if (name == "Water")
        return SurfaceMaterial::Water;
    if (name == "Dirt")
        return SurfaceMaterial::Dirt;
    if (name == "Snow")
        return SurfaceMaterial::Snow;
    return SurfaceMaterial::Stone;
}

}

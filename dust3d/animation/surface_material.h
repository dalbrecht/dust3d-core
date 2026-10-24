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

#ifndef DUST3D_ANIMATION_SURFACE_MATERIAL_H_
#define DUST3D_ANIMATION_SURFACE_MATERIAL_H_
#include <array>
#include <string>
namespace dust3d {
enum class SurfaceMaterial {
    Stone,
    Wood,
    Sand,
    Metal,
    Grass,
    Water,
    Dirt,
    Snow
};
/** Return fixed reference UI order; immutable process-static storage, no synthesis.
 * @return Stone, Wood, Sand, Metal, Grass, Water, Dirt, Snow.
 * No input or allocation. Example: `surfaceMaterials().front()`.
 */
const std::array<SurfaceMaterial, 8>& surfaceMaterials() noexcept;
/** Return the inherited exact stored name; unknown enum values fall back to Stone.
 * @param material Reference surface identity.
 * @return Owned name; normal string construction errors propagate to C++ callers.
 * Example: `surfaceMaterialName(SurfaceMaterial::Wood)`.
 */
std::string surfaceMaterialName(SurfaceMaterial material);
/** Preserve inherited exact-name parsing and Stone fallback; not a document validator.
 * @param name Stored name; unknown names must be retained separately by document editors.
 * @return Matching identity, or Stone for unknown/case-mismatched names.
 * Example: `surfaceMaterialFromName("Wood")`.
 */
SurfaceMaterial surfaceMaterialFromName(const std::string& name);
}
#endif

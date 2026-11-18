// SPDX-License-Identifier: MIT
#include <array>
#include <cmath>
#include <dust3d/uv/uv_map_packer.h>
#include <iostream>
#include <stdexcept>

namespace {
using Corners = std::array<dust3d::Vector2, 3>;
using TriangleKey = std::array<dust3d::PositionKey, 3>;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
TriangleKey positions()
{
    return { dust3d::PositionKey(dust3d::Vector3(0, 0, 0)), dust3d::PositionKey(dust3d::Vector3(1, 0, 0)), dust3d::PositionKey(dust3d::Vector3(0, 1, 0)) };
}
void verify(const dust3d::UvMapPacker::Layout& layout, const Corners& input, const Corners& output)
{
    for (size_t corner = 0; corner < 3; ++corner) {
        const auto& uv = input[corner];
        const double x = layout.left + (layout.flipped ? uv.y() : uv.x()) * layout.width;
        const double y = layout.top + (layout.flipped ? uv.x() : uv.y()) * layout.height;
        require(std::abs(output[corner].x() - x) < 1e-12 && std::abs(output[corner].y() - y) < 1e-12,
            "Ordinal corner did not receive its chart transform");
    }
}
void coincidentOrdinals()
{
    dust3d::UvMapPacker::Part part;
    part.width = 2;
    part.height = 3;
    const Corners first { dust3d::Vector2(.1, .2), dust3d::Vector2(.8, .2), dust3d::Vector2(.1, .9) };
    const Corners second { dust3d::Vector2(.9, .8), dust3d::Vector2(.2, .8), dust3d::Vector2(.9, .1) };
    // Distinct face ordinals can own different corners at the same positions.
    part.localUv[positions()] = first;
    part.indexedLocalUv[7] = first;
    part.indexedLocalUv[31] = second;
    const auto saved = part.indexedLocalUv;
    dust3d::UvMapPacker packer;
    packer.addPart(part);
    packer.pack();
    require(part.indexedLocalUv == saved, "Packing mutated caller-owned corners");
    require(packer.packedLayouts().size() == 1, "Packing lost a chart");
    const auto& layout = packer.packedLayouts()[0];
    require(layout.indexedGlobalUv.size() == 2 && layout.globalUv.size() == 1, "Coincident ordinals were merged or changed legacy records");
    verify(layout, first, layout.indexedGlobalUv.at(7));
    verify(layout, second, layout.indexedGlobalUv.at(31));
    require(layout.globalUv.at(positions()) == layout.indexedGlobalUv.at(7), "Legacy and ordinal transforms disagree");
    part.indexedLocalUv.clear();
    require(layout.indexedGlobalUv.size() == 2, "Packed records borrowed caller storage");
}
void chartTransforms()
{
    const Corners corners { dust3d::Vector2(.15, .3), dust3d::Vector2(.8, .45), dust3d::Vector2(.35, .9) };
    const std::array<std::pair<double, double>, 4> sizes { { { 4, 1 }, { 1, 4 }, { 2, 1 }, { 1, 2 } } };
    dust3d::UvMapPacker packer;
    for (size_t index = 0; index < sizes.size(); ++index) {
        dust3d::UvMapPacker::Part part;
        part.width = sizes[index].first;
        part.height = sizes[index].second;
        part.indexedLocalUv[index] = corners;
        part.localUv[positions()] = corners;
        packer.addPart(part);
        part.indexedLocalUv.clear();
        part.localUv.clear();
    }
    packer.pack();
    const auto& layouts = packer.packedLayouts();
    require(layouts.size() == sizes.size(), "Packing lost chart ownership");
    size_t flipped = 0;
    for (size_t index = 0; index < sizes.size(); ++index) {
        const auto& layout = layouts[index];
        flipped += layout.flipped;
        require(layout.indexedGlobalUv.size() == 1 && layout.indexedGlobalUv.count(index), "Chart reassigned a triangle ordinal");
        verify(layout, corners, layout.indexedGlobalUv.at(index));
        require(layout.globalUv.at(positions()) == layout.indexedGlobalUv.at(index), "Flipped ordinal and legacy corners differ");
    }
    require(flipped > 0 && flipped < sizes.size(), "Fixture did not exercise flipped and ordinary layouts");
}
void legacyOnly()
{
    dust3d::UvMapPacker::Part part;
    part.width = part.height = 1;
    const Corners corners { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    part.localUv[positions()] = corners;
    dust3d::UvMapPacker packer;
    packer.addPart(part);
    packer.pack();
    require(packer.packedLayouts().size() == 1, "Legacy chart lost");
    const auto& layout = packer.packedLayouts()[0];
    require(layout.indexedGlobalUv.empty() && layout.globalUv.size() == 1, "Legacy-only chart invented ordinal metadata");
    verify(layout, corners, layout.globalUv.at(positions()));
}
}
int main()
{
    try {
        coincidentOrdinals();
        chartTransforms();
        legacyOnly();
        std::cout << "Indexed UV packer contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

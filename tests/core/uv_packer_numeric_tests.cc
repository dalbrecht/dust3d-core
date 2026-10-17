#include <cmath>
#include <cstdint>
#include <dust3d/uv/chart_packer.h>
#include <dust3d/uv/max_rectangles.h>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

void wideAreasAndScores()
{
    for (const auto method : { dust3d::uv::kMaxRectanglesBestShortSideFit, dust3d::uv::kMaxRectanglesBestLongSideFit, dust3d::uv::kMaxRectanglesBestAreaFit, dust3d::uv::kMaxRectanglesBottomLeftRule, dust3d::uv::kMaxRectanglesContactPointRule }) {
        dust3d::uv::MaxRectanglesSize rectangles[] = { { 600000, 500000 }, { 400000, 500000 } };
        dust3d::uv::MaxRectanglesPosition positions[2] {};
        float occupancy = 0;
        require(dust3d::uv::maxRectangles(1000000, 1000000, 2, rectangles, method, true, positions, &occupancy) == 0, "Large valid rectangles must pack under every heuristic");
        require(std::isfinite(occupancy) && std::abs(occupancy - 0.5f) < 0.000001f, "Occupancy must use full-width area multiplication");
        for (size_t i = 0; i < 2; ++i) {
            const int64_t width = positions[i].rotated ? rectangles[i].height : rectangles[i].width;
            const int64_t height = positions[i].rotated ? rectangles[i].width : rectangles[i].height;
            require(positions[i].left >= 0 && positions[i].top >= 0 && positions[i].left + width <= 1000000 && positions[i].top + height <= 1000000, "Large rectangle coordinates must remain inside the bin");
        }
    }
    const int maximum = std::numeric_limits<int>::max();
    dust3d::uv::MaxRectanglesSize rectangle { maximum, maximum };
    dust3d::uv::MaxRectanglesPosition position {};
    float occupancy = 0;
    require(dust3d::uv::maxRectangles(maximum, maximum, 1, &rectangle, dust3d::uv::kMaxRectanglesContactPointRule, false, &position, &occupancy) == 0, "Contact scores must represent two maximum-width boundaries");
    require(occupancy == 1 && position.left == 0 && position.top == 0, "A full bin remains exactly occupied");
}

void invalidRectangleInputs()
{
    dust3d::uv::MaxRectanglesSize rectangle { 1, 1 };
    dust3d::uv::MaxRectanglesPosition position {};
    float occupancy = 0;
    const auto method = dust3d::uv::kMaxRectanglesBestAreaFit;
    require(dust3d::uv::maxRectangles(0, 1, 1, &rectangle, method, false, &position, &occupancy) == -1, "Zero-width bins must reject before division");
    require(dust3d::uv::maxRectangles(1, -1, 1, &rectangle, method, false, &position, &occupancy) == -1, "Negative bin height must reject");
    require(dust3d::uv::maxRectangles(1, 1, -1, &rectangle, method, false, &position, &occupancy) == -1, "Negative rectangle count must reject");
    require(dust3d::uv::maxRectangles(1, 1, 1, nullptr, method, false, &position, &occupancy) == -1, "Missing rectangle buffers must reject");
    require(dust3d::uv::maxRectangles(1, 1, 1, &rectangle, method, false, nullptr, &occupancy) == -1, "Missing output buffers must reject");
    rectangle.width = 0;
    require(dust3d::uv::maxRectangles(1, 1, 1, &rectangle, method, false, &position, &occupancy) == -1, "Degenerate rectangles must reject");
    rectangle = { 1, -1 };
    require(dust3d::uv::maxRectangles(1, 1, 1, &rectangle, method, false, &position, &occupancy) == -1, "Negative rectangle height must reject");
    require(dust3d::uv::maxRectangles(1, 1, 0, nullptr, method, false, nullptr, &occupancy) == 0 && occupancy == 0, "An empty positive bin remains well-defined");
}

void checkedChartConversions()
{
    dust3d::ChartPacker packer;
    packer.setCharts({ { 4096, 4096 } });
    const auto textureSize = packer.pack();
    require(std::isfinite(textureSize) && textureSize > 4096 && packer.getResult().size() == 1, "The ordinary automatic atlas workflow must remain finite and complete");
    require(!packer.tryPack(1) && packer.getResult().empty(), "A valid chart that cannot fit must fail without stale layouts");
    require(packer.tryPack(8192), "Ordinary texture-sized charts must remain supported");
    require(packer.getResult().size() == 1, "A successful chart stays addressable");
    for (float invalid : { 0.0f, -1.0f, 1e-30f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max(), std::ldexp(1.0f, std::numeric_limits<int>::digits) / 10000.0f }) {
        require(!packer.tryPack(invalid), "Unrepresentable texture sizes must reject before integer conversion");
        require(packer.getResult().empty(), "Rejected packing must not publish a previous layout");
    }
    packer.setCharts({ { 0.0001f, 1 } });
    require(!packer.tryPack(200000), "Padding precision must not erase a positive chart and publish zero width");
    for (float invalid : { 0.0f, -1.0f, 1e-30f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::max() }) {
        packer.setCharts({ { invalid, 1 } });
        require(!packer.tryPack(8192), "Invalid chart width must reject before rectangle packing");
        packer.setCharts({ { 1, invalid } });
        require(!packer.tryPack(8192), "Invalid chart height must reject before rectangle packing");
    }
}
}

int main()
{
    try {
        wideAreasAndScores();
        checkedChartConversions();
        invalidRectangleInputs();
        std::cout << "UV packer numeric contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

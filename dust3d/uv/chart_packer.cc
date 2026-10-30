/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <cmath>
#include <dust3d/uv/chart_packer.h>
#include <dust3d/uv/max_rectangles.h>
#include <limits>

namespace dust3d {

void ChartPacker::setCharts(const std::vector<std::pair<float, float>>& chartSizes)
{
    m_chartSizes = chartSizes;
}

bool ChartPacker::setPaddingFraction(double fraction)
{
    if (!std::isfinite(fraction) || fraction <= 0 || fraction > .125)
        return false;
    const float converted = static_cast<float>(fraction);
    if (!std::isfinite(converted) || converted <= 0)
        return false;
    m_paddingSize = converted;
    m_result.clear();
    m_tryNum = 0;
    m_textureSizeFactor = 1.0;
    return true;
}

const std::vector<std::tuple<float, float, float, float, bool>>& ChartPacker::getResult()
{
    return m_result;
}

double ChartPacker::calculateTotalArea()
{
    double totalArea = 0;
    for (const auto& chartSize : m_chartSizes) {
        totalArea += chartSize.first * chartSize.second;
    }
    return totalArea;
}

bool ChartPacker::tryPack(float textureSize)
{
    m_result.clear();
    const double exclusiveUpperBound = std::ldexp(1.0, std::numeric_limits<int>::digits);
    const float scaledTextureSize = textureSize * m_floatToIntFactor;
    if (!std::isfinite(scaledTextureSize) || scaledTextureSize < 1 || scaledTextureSize >= exclusiveUpperBound
        || m_chartSizes.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        return false;
    std::vector<uv::MaxRectanglesSize> rects;
    int width = static_cast<int>(scaledTextureSize);
    int height = width;
    float paddingSize = m_paddingSize * width;
    float paddingSize2 = paddingSize + paddingSize;
    for (const auto& chartSize : m_chartSizes) {
        const float scaledWidth = chartSize.first * m_floatToIntFactor;
        const float scaledHeight = chartSize.second * m_floatToIntFactor;
        const float paddedWidth = scaledWidth + paddingSize2;
        const float paddedHeight = scaledHeight + paddingSize2;
        if (!std::isfinite(scaledWidth) || !std::isfinite(scaledHeight) || scaledWidth < 1 || scaledHeight < 1
            || !std::isfinite(paddedWidth) || !std::isfinite(paddedHeight)
            || paddedWidth >= exclusiveUpperBound || paddedHeight >= exclusiveUpperBound)
            return false;
        uv::MaxRectanglesSize r;
        r.width = static_cast<int>(paddedWidth);
        r.height = static_cast<int>(paddedHeight);
        // Float padding can erase a small chart even when each integer conversion is in range.
        if (static_cast<float>(r.width) <= paddingSize2 || static_cast<float>(r.height) <= paddingSize2)
            return false;
        rects.push_back(r);
    }
    const uv::MaxRectanglesFreeRectChoiceHeuristic methods[] = {
        uv::kMaxRectanglesBestShortSideFit,
        uv::kMaxRectanglesBestLongSideFit,
        uv::kMaxRectanglesBestAreaFit,
        uv::kMaxRectanglesBottomLeftRule,
        uv::kMaxRectanglesContactPointRule
    };
    float occupancy = 0;
    float bestOccupancy = 0;
    std::vector<uv::MaxRectanglesPosition> bestResult;
    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i) {
        std::vector<uv::MaxRectanglesPosition> result(rects.size());
        if (0 != maxRectangles(width, height, static_cast<int>(rects.size()), rects.data(), methods[i], true, result.data(), &occupancy)) {
            continue;
        }
        if (occupancy > bestOccupancy) {
            bestResult = result;
            bestOccupancy = occupancy;
        }
    }
    if (bestResult.size() != rects.size())
        return false;
    m_result.resize(bestResult.size());
    for (decltype(bestResult.size()) i = 0; i < bestResult.size(); ++i) {
        const auto& result = bestResult[i];
        const auto& rect = rects[i];
        auto& dest = m_result[i];
        std::get<0>(dest) = (float)(result.left + paddingSize) / width;
        std::get<1>(dest) = (float)(result.top + paddingSize) / height;
        std::get<2>(dest) = (float)(rect.width - paddingSize2) / width;
        std::get<3>(dest) = (float)(rect.height - paddingSize2) / height;
        std::get<4>(dest) = result.rotated;
    }
    return true;
}

float ChartPacker::pack()
{
    float textureSize = 0;
    float initialGuessSize = std::sqrt(calculateTotalArea() * m_initialAreaGuessFactor);
    while (true) {
        textureSize = initialGuessSize * m_textureSizeFactor;
        ++m_tryNum;
        if (tryPack(textureSize))
            break;
        m_textureSizeFactor += m_textureSizeGrowFactor;
        if (m_tryNum >= m_maxTryNum) {
            break;
        }
    }
    return textureSize;
}

}

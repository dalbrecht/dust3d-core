// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/uv/chart_packer.h>
#include <dust3d/uv/source_uv_atlas.h>
#include <limits>
namespace dust3d {
namespace {
    [[noreturn]] void invalid(const char* message) { throw UvAtlasError(UvAtlasErrorCode::invalidInput, message); }
    void checkCancellation(const SourceUvAtlasOptions& options)
    {
        if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
            throw UvAtlasError(UvAtlasErrorCode::cancelled, "Source UV atlas construction cancelled");
    }
    bool unit(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
    void validateChart(const SourceUvChart& chart)
    {
        if (!unit(chart.left) || !unit(chart.top) || !unit(chart.width) || !unit(chart.height)
            || chart.width <= 0 || chart.height <= 0 || chart.left + chart.width > 1.000001 || chart.top + chart.height > 1.000001)
            invalid("Source UV chart bounds are invalid");
    }
    void charge(RetainedPayloadBudget& budget, size_t count, size_t width = 1)
    {
        if (!budget.tryAdd(count, width))
            throw UvAtlasError(UvAtlasErrorCode::limitExceeded, "Source UV atlas retained payload exceeds quota");
    }
    struct LocalTriangle {
        std::array<Vector2, 3> uv;
        double width = 0;
        double height = 0;
        double area = 0;
    };
    LocalTriangle localTriangle(const Object& object, size_t index, bool imported)
    {
        const auto& triangle = object.triangles[index];
        if (triangle.size() != 3 || std::any_of(triangle.begin(), triangle.end(), [&](size_t vertex) { return vertex >= object.vertices.size(); }))
            invalid("Source UV geometry has an invalid triangle index or size");
        const auto ab = object.vertices[triangle[1]] - object.vertices[triangle[0]];
        const auto ac = object.vertices[triangle[2]] - object.vertices[triangle[0]];
        const auto cross = Vector3::crossProduct(ab, ac);
        double twiceArea = std::hypot(cross.x(), cross.y(), cross.z());
        double length = std::hypot(ab.x(), ab.y(), ab.z());
        if (!std::isfinite(twiceArea) || !std::isfinite(length))
            invalid("Source UV geometry exceeds finite chart arithmetic");
        LocalTriangle result;
        result.area = twiceArea / 2;
        if (!imported)
            return result;
        if (length <= 0 || result.area <= 0)
            invalid("Imported UV charts require nondegenerate geometry");
        double projection = Vector3::dotProduct(ac, ab / length);
        result.width = std::max(length, projection) - std::min(0.0, projection);
        result.height = twiceArea / length;
        if (!std::isfinite(projection) || !std::isfinite(result.width) || !std::isfinite(result.height) || result.width <= 0 || result.height <= 0)
            invalid("Imported UV chart dimensions are not finite and positive");
        const double minimum = std::min(0.0, projection);
        result.uv = { Vector2(-minimum / result.width, 0), Vector2((length - minimum) / result.width, 0), Vector2((projection - minimum) / result.width, 1) };
        return result;
    }
}
Vector2 SourceUvChart::map(const Vector2& coordinate) const
{
    validateChart(*this);
    if (!unit(coordinate.x()) || !unit(coordinate.y()))
        invalid("Source UV local chart coordinates must be normalized and finite");
    double u = flipped ? coordinate.y() : coordinate.x();
    double v = flipped ? coordinate.x() : coordinate.y();
    return { std::clamp(left + u * width, 0.0, 1.0), std::clamp(top + v * height, 0.0, 1.0) };
}
SourceUvAtlas SourceUvAtlasBuilder::build(const Object& object, const UvAtlas& legacy, const SourceUvAtlasOptions& options)
{
    checkCancellation(options);
    if (!options.textureSize || options.textureSize > 8192 || !options.maximumChartCount || options.maximumChartCount > 100000 || options.maximumPayloadBytes > 256 * 1024 * 1024)
        invalid("Source UV atlas options exceed supported dimensions, chart count or quota");
    const size_t count = object.triangles.size();
    auto aligned = [&](size_t size) { return !size || size == count; };
    if (!aligned(object.triangleModelKeys.size()) || !aligned(object.triangleComponentKeys.size()) || !aligned(object.triangleCornerUvs.size())
        || !aligned(legacy.triangleUvs.size()) || !aligned(legacy.triangleUvKnown.size()) || legacy.triangleUvs.size() != legacy.triangleUvKnown.size())
        invalid("Source UV channels must align with original triangles");
    size_t maximum = options.maximumPayloadBytes ? options.maximumPayloadBytes : 64 * 1024 * 1024;
    RetainedPayloadBudget planned(maximum);
    charge(planned, 1, sizeof(SourceUvAtlas));
    charge(planned, count, sizeof(SourceUvTriangle));
    for (const auto* keys : { &object.triangleModelKeys, &object.triangleComponentKeys })
        for (const auto& key : *keys) {
            checkCancellation(options);
            charge(planned, key.size());
            charge(planned, 1);
        }
    size_t plannedCharts = 0;
    bool plannedLegacy = false;
    for (size_t index = 0; index < count; ++index) {
        checkCancellation(options);
        const bool imported = !object.triangleModelKeys.empty() && !object.triangleModelKeys[index].empty();
        if (imported)
            ++plannedCharts;
        else if (!legacy.triangleUvKnown.empty() && legacy.triangleUvKnown[index])
            plannedLegacy = true;
    }
    plannedCharts += plannedLegacy ? 1 : 0;
    if (plannedCharts > options.maximumChartCount)
        throw UvAtlasError(UvAtlasErrorCode::limitExceeded, "Source UV chart count exceeds its work limit");
    for (const auto& vertex : object.vertices) {
        checkCancellation(options);
        for (size_t axis = 0; axis < 3; ++axis)
            if (!std::isfinite(vertex[axis]))
                invalid("Source UV geometry contains nonfinite vertices");
    }
    SourceUvAtlas result;
    result.triangles.resize(count);
    RetainedPayloadBudget retained(maximum);
    charge(retained, 1, sizeof(SourceUvAtlas));
    charge(retained, result.triangles.capacity(), sizeof(SourceUvTriangle));
    std::vector<LocalTriangle> locals(count);
    std::vector<size_t> imports;
    bool hasLegacy = false;
    double legacyArea = 0;
    double totalArea = 0;
    for (size_t index = 0; index < count; ++index) {
        checkCancellation(options);
        auto& triangle = result.triangles[index];
        if (!object.triangleModelKeys.empty())
            triangle.modelKey = object.triangleModelKeys[index];
        if (!object.triangleComponentKeys.empty())
            triangle.componentKey = object.triangleComponentKeys[index];
        charge(retained, triangle.modelKey.capacity());
        charge(retained, triangle.componentKey.capacity());
        charge(retained, 2);
        const bool imported = !triangle.modelKey.empty();
        if (!object.triangleCornerUvs.empty() && object.triangleCornerUvs[index]) {
            if (!imported)
                invalid("Authored UVs require a resolved imported model owner");
            triangle.authoredUvs = object.triangleCornerUvs[index];
            for (const auto& uv : *triangle.authoredUvs)
                if (!std::isfinite(uv.x()) || !std::isfinite(uv.y()))
                    invalid("Authored source UVs must be finite");
        }
        locals[index] = localTriangle(object, index, imported);
        if (imported) {
            imports.push_back(index);
            triangle.known = true;
        } else if (!legacy.triangleUvKnown.empty()) {
            if (legacy.triangleUvKnown[index] > 1)
                invalid("Legacy UV known markers must be zero or one");
            triangle.known = legacy.triangleUvKnown[index] != 0;
            if (triangle.known) {
                for (const auto& uv : legacy.triangleUvs[index])
                    if (!unit(uv.x()) || !unit(uv.y()))
                        invalid("Legacy packed UVs must be finite and normalized");
                hasLegacy = true;
                legacyArea += locals[index].area;
            }
        }
        if (triangle.known)
            totalArea += locals[index].area;
    }
    if (!std::isfinite(totalArea) || !std::isfinite(legacyArea))
        invalid("Source UV total chart area is not finite");
    if (hasLegacy && (!std::isfinite(legacy.packedTextureSize) || legacy.packedTextureSize <= 0))
        invalid("Known legacy UVs require a positive finite atlas size");
    const size_t chartCount = imports.size() + (hasLegacy ? 1 : 0);
    if (imports.empty()) {
        if (hasLegacy) {
            result.legacyChart = SourceUvChart {};
            result.packedTextureSize = legacy.packedTextureSize;
            for (size_t index = 0; index < count; ++index)
                if (result.triangles[index].known)
                    result.triangles[index].packedUvs = legacy.triangleUvs[index];
        }
    } else {
        double scale = options.textureSize / std::sqrt(totalArea);
        // Imported areas are strictly positive and the checked total is finite;
        // textureSize is bounded, so this double scale is finite and positive.
        std::vector<std::pair<float, float>> dimensions;
        auto dimension = [&](double value) {
            const double scaled = std::max(1.0, value * scale);
            if (!std::isfinite(scaled) || scaled > std::numeric_limits<float>::max())
                invalid("Source UV chart dimension exceeds packing range");
            return static_cast<float>(scaled);
        };
        if (hasLegacy) {
            const float side = dimension(std::sqrt(legacyArea));
            dimensions.emplace_back(side, side);
        }
        for (const auto index : imports)
            dimensions.emplace_back(dimension(locals[index].width), dimension(locals[index].height));
        double rectangleArea = 0;
        for (const auto& size : dimensions)
            rectangleArea += static_cast<double>(size.first) * size.second;
        double guess = std::sqrt(rectangleArea * 1.1);
        if (!std::isfinite(guess) || guess <= 0 || guess > std::numeric_limits<float>::max())
            invalid("Source UV packing area exceeds finite range");
        ChartPacker packer;
        packer.setCharts(dimensions);
        bool packed = false;
        for (size_t attempt = 0; attempt < 100; ++attempt) {
            checkCancellation(options);
            const double candidate = guess * (1 + .05 * attempt);
            if (candidate > std::numeric_limits<float>::max())
                break;
            if (packer.tryPack(static_cast<float>(candidate))) {
                result.packedTextureSize = static_cast<float>(candidate);
                packed = true;
                break;
            }
        }
        if (!packed || packer.getResult().size() != chartCount)
            throw UvAtlasError(UvAtlasErrorCode::packingFailed, "The source UV packer could not retain every chart");
        auto region = [&](size_t ordinal) {
            const auto& value = packer.getResult()[ordinal];
            bool flipped = std::get<4>(value);
            SourceUvChart chart { std::get<0>(value), std::get<1>(value), flipped ? std::get<3>(value) : std::get<2>(value), flipped ? std::get<2>(value) : std::get<3>(value), flipped };
            validateChart(chart);
            return chart;
        };
        if (hasLegacy) {
            result.legacyChart = region(0);
            for (size_t index = 0; index < count; ++index) {
                checkCancellation(options);
                if (!result.triangles[index].modelKey.empty() || !result.triangles[index].known)
                    continue;
                for (size_t corner = 0; corner < 3; ++corner)
                    result.triangles[index].packedUvs[corner] = result.legacyChart->map(legacy.triangleUvs[index][corner]);
            }
        }
        for (size_t ordinal = 0; ordinal < imports.size(); ++ordinal) {
            checkCancellation(options);
            const size_t index = imports[ordinal];
            auto& triangle = result.triangles[index];
            triangle.importedChart = region(ordinal + (hasLegacy ? 1 : 0));
            for (size_t corner = 0; corner < 3; ++corner)
                triangle.packedUvs[corner] = triangle.importedChart->map(locals[index].uv[corner]);
        }
    }
    checkCancellation(options);
    result.payloadBytes = retained.usedBytes();
    return result;
}
}

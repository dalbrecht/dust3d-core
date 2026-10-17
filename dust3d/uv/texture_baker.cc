// SPDX-License-Identifier: MIT
// Bake algorithms derived from Dust3D, Copyright (c) 2016-2026 Jeremy HU.
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/uv/texture_baker.h>
#include <limits>
#include <memory>

namespace dust3d {
namespace {
    constexpr size_t maximumDimension = 8192;
    constexpr size_t maximumBudget = 256 * 1024 * 1024;
    void invalid(const char* message)
    {
        throw TextureBakeError(TextureBakeErrorCode::invalidInput, message);
    }
    void checkCancellation(const TextureBakeOptions& options)
    {
        if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
            throw TextureBakeError(TextureBakeErrorCode::cancelled, "Texture baking was cancelled");
    }
    bool unit(double value)
    {
        return std::isfinite(value) && value >= 0 && value <= 1;
    }
    void validateColor(const Color& color)
    {
        for (size_t channel = 0; channel < 4; ++channel) {
            if (!unit(color[channel]))
                invalid("Texture color must contain normalized finite channels");
        }
    }
    void validate(const Object& object, const UvAtlas& atlas, const TextureBakeInputs& inputs, const TextureBakeOptions& options)
    {
        checkCancellation(options);
        for (const auto dimension : { options.colorSize, options.normalSize, options.materialSize }) {
            if (dimension == 0 || dimension > maximumDimension)
                invalid("Texture dimensions must be in 1..8192");
        }
        if (!options.maximumSpatialGridEntries || options.maximumSpatialGridEntries > 16000000)
            invalid("Texture spatial grid work limit must be in 1..16000000");
        if (options.maximumPayloadBytes > maximumBudget)
            invalid("Texture output budget exceeds 256 MiB");
        for (const auto& vertex : object.vertices) {
            for (size_t axis = 0; axis < 3; ++axis) {
                const double scaled = vertex[axis] * 100000.0;
                const double bound = std::ldexp(1.0, std::numeric_limits<long>::digits);
                if (!std::isfinite(vertex[axis]) || scaled < -bound || scaled >= bound)
                    invalid("Texture geometry must contain finite vertices");
            }
        }
        if (atlas.triangleUvs.size() != object.triangles.size() || atlas.triangleUvKnown.size() != object.triangles.size() || atlas.triangleComponentIds.size() != object.triangles.size())
            invalid("Texture UV records must align with original triangles");
        for (size_t index = 0; index < object.triangles.size(); ++index) {
            const auto& triangle = object.triangles[index];
            if (triangle.size() != 3)
                invalid("Texture geometry must be triangulated");
            for (const auto vertex : triangle) {
                if (vertex >= object.vertices.size())
                    invalid("Texture triangle index exceeds vertex count");
            }
            if (atlas.triangleUvKnown[index] > 1)
                invalid("Texture known markers must be zero or one");
            for (const auto& uv : atlas.triangleUvs[index]) {
                if (!unit(uv.x()) || !unit(uv.y()))
                    invalid("Texture coordinates must be finite and normalized");
            }
        }
        for (const auto& layout : atlas.layouts) {
            validateColor(layout.color);
            if (!unit(layout.left) || !unit(layout.top) || !unit(layout.width) || !unit(layout.height) || layout.left + layout.width > 1.0000000001 || layout.top + layout.height > 1.0000000001)
                invalid("Texture chart bounds exceed the atlas");
        }
        std::set<size_t> gradientOrdinals;
        for (const auto& gradient : atlas.gradients) {
            if (gradient.layoutIndex >= atlas.layouts.size() || !gradientOrdinals.insert(gradient.layoutIndex).second || gradient.resolution < 2 || gradient.resolution > maximumDimension)
                invalid("Texture gradient has an invalid or duplicate layout ordinal or resolution");
            validateColor(gradient.largeColor);
            validateColor(gradient.smallColor);
        }
        for (const auto& layout : atlas.layouts) {
            for (const auto& entry : layout.globalUv) {
                for (const auto& uv : entry.second) {
                    if (!unit(uv.x()) || !unit(uv.y()))
                        invalid("Surface chart contains invalid UV coordinates");
                }
            }
        }
        for (const auto& entry : inputs.surfaces) {
            const auto& surface = entry.second;
            if (!unit(surface.wrinkleStrength))
                invalid("Wrinkle strength must be finite and normalized");
            if (surface.pattern) {
                const auto& pattern = *surface.pattern;
                const auto kind = static_cast<int>(pattern.type);
                if (kind < 0 || kind > static_cast<int>(SurfacePattern::Type::Mottled) || !std::isfinite(pattern.scale) || pattern.scale <= 0 || !unit(pattern.belly))
                    invalid("Surface pattern settings exceed supported finite ranges");
                validateColor(pattern.baseColor);
                validateColor(pattern.patternColor);
            }
            for (const auto& fold : surface.folds) {
                const auto kind = static_cast<int>(fold.kind);
                if (kind < 0 || kind > static_cast<int>(ClothFolds::Kind::Crease) || fold.points.size() < 2 || fold.normals.size() != fold.points.size() || !std::isfinite(fold.width) || fold.width <= 0 || !std::isfinite(fold.depth) || fold.depth < 0 || !unit(fold.crease) || !unit(fold.peak) || !unit(fold.endDepth) || !std::isfinite(fold.ridgeSide) || std::abs(fold.ridgeSide) != 1)
                    invalid("Cloth fold has invalid shape, dimensions or profile");
                for (size_t index = 0; index < fold.points.size(); ++index) {
                    for (size_t axis = 0; axis < 3; ++axis) {
                        if (!std::isfinite(fold.points[index][axis]) || !std::isfinite(fold.normals[index][axis]))
                            invalid("Cloth fold contains nonfinite points or normals");
                    }
                    const auto length = fold.normals[index].lengthSquared();
                    if (!std::isfinite(length) || std::abs(length - 1) > 1e-6)
                        invalid("Cloth fold normals must be unit vectors");
                }
            }
        }
        for (const auto& entry : inputs.images) {
            const auto& image = entry.second;
            if (!image.pixels || !image.width || !image.height || image.width > maximumDimension || image.height > maximumDimension)
                invalid("Source image requires pixels and dimensions in 1..8192");
            const auto rowBytes = image.width * 4;
            if (image.rowStride < rowBytes || (image.height - 1 && image.rowStride > (std::numeric_limits<size_t>::max() - rowBytes) / (image.height - 1)))
                invalid("Source image stride is invalid or overflows its byte span");
            if ((image.height - 1) * image.rowStride + rowBytes > image.byteCount)
                invalid("Source image byte span is truncated");
        }
        for (const auto& entry : inputs.materials) {
            const auto& material = entry.second;
            if (!unit(material.metallic) || !unit(material.roughness) || !std::isfinite(material.emissive) || material.emissive < 0 || material.emissive > 2)
                invalid("Material scalars exceed their finite supported ranges");
        }
    }
    uint8_t byte(double value)
    {
        if (!std::isfinite(value))
            invalid("Texture evaluation produced a nonfinite channel");
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255));
    }
    using Pixel = std::array<uint8_t, 4>;
    Pixel colorPixel(const Color& color)
    {
        return { byte(color.r()), byte(color.g()), byte(color.b()), byte(color.alpha()) };
    }
    void put(TextureImage& image, size_t index, const Pixel& pixel)
    {
        std::copy(pixel.begin(), pixel.end(), image.pixels.begin() + index * 4);
    }
    TextureImage image(size_t size)
    {
        return { size, size, std::vector<uint8_t>(size * size * 4) };
    }
    void dilate(TextureImage& image, std::vector<uint8_t>& covered, const TextureBakeOptions& options)
    {
        // Coverage is independent of color and alpha, including transparent source texels.
        std::vector<uint32_t> queue;
        for (size_t index = 0; index < covered.size(); ++index) {
            if (covered[index])
                queue.push_back(static_cast<uint32_t>(index));
        }
        for (size_t cursor = 0; cursor < queue.size(); ++cursor) {
            if (cursor % 4096 == 0)
                checkCancellation(options);
            const size_t index = queue[cursor];
            const auto x = index % image.width;
            const auto y = index / image.width;
            auto spread = [&](size_t next) {
                if (covered[next])
                    return;
                covered[next] = 1;
                for (size_t channel = 0; channel < 4; ++channel)
                    image.pixels[next * 4 + channel] = image.pixels[index * 4 + channel];
                queue.push_back(static_cast<uint32_t>(next));
            };
            if (x)
                spread(index - 1);
            if (x + 1 < image.width)
                spread(index + 1);
            if (y)
                spread(index - image.width);
            if (y + 1 < image.height)
                spread(index + image.width);
        }
    }
    template <typename Paint>
    std::vector<uint8_t> rectangles(TextureImage& output, const UvAtlas& atlas, double bleedFraction, const TextureBakeOptions& options, Paint paint, const std::set<size_t>& skipped = {})
    {
        std::vector<uint8_t> covered(output.width * output.height);
        const auto size = static_cast<int>(output.width);
        const auto bleed = static_cast<int>(bleedFraction * size);
        for (size_t ordinal = 0; ordinal < atlas.layouts.size(); ++ordinal) {
            checkCancellation(options);
            if (skipped.count(ordinal))
                continue;
            const auto& layout = atlas.layouts[ordinal];
            const auto left = static_cast<int>(layout.left * size);
            const auto top = static_cast<int>(layout.top * size);
            const auto width = static_cast<int>(layout.width * size);
            const auto height = static_cast<int>(layout.height * size);
            if (!width || !height)
                continue;
            for (int y = std::max(0, top - bleed); y < std::min(size, top + height + bleed); ++y) {
                checkCancellation(options);
                for (int x = std::max(0, left - bleed); x < std::min(size, left + width + bleed); ++x) {
                    const auto index = static_cast<size_t>(y) * output.width + x;
                    const double u = std::clamp((x - left + 0.5) / width, 0.0, 1.0);
                    const double v = std::clamp((y - top + 0.5) / height, 0.0, 1.0);
                    put(output, index, paint(ordinal, layout, u, v));
                    covered[index] = 1;
                }
            }
        }
        return covered;
    }
    struct SurfaceTriangle {
        std::array<Vector3, 3> positions;
        std::array<Vector2, 3> uvs;
        std::array<size_t, 3> vertices;
        Vector3 tangent;
        double handedness = 1;
        double worldPerUv = 0;
    };
    double foldGridScale(const TextureSurface& surface)
    {
        double maximumReach = 0;
        double segmentLength = 0;
        size_t segmentCount = 0;
        for (const auto& fold : surface.folds) {
            double length = 0;
            for (size_t index = 1; index < fold.points.size(); ++index)
                length += (fold.points[index] - fold.points[index - 1]).length();
            if (!std::isfinite(length))
                invalid("Cloth fold length overflows finite geometry");
            if (length <= 1e-9)
                continue;
            maximumReach = std::max(maximumReach, fold.width * 1.15 * 2.6);
            segmentLength += length;
            segmentCount += fold.points.size() - 1;
        }
        if (!std::isfinite(maximumReach) || !std::isfinite(segmentLength))
            invalid("Cloth fold spatial grid scale overflows");
        // Conservative headroom for summation rounding in the inherited grid builder.
        return std::max(1e-5, std::max(maximumReach, segmentLength / std::max(size_t(1), segmentCount))) * 0.5;
    }
    void checkFoldCell(const Vector3& position, double scale, double reach = 0)
    {
        const double limit = static_cast<double>(std::numeric_limits<int>::max()) - 4;
        for (size_t axis = 0; axis < 3; ++axis) {
            const auto bound = (std::abs(position[axis]) + reach) / scale;
            if (!std::isfinite(bound) || bound > limit)
                invalid("Cloth fold coordinate exceeds its safe signed spatial grid range");
        }
    }
    struct SurfaceChart {
        size_t layoutIndex = 0;
        double foldScale = 0;
        std::vector<Vector3> vertices;
        std::vector<Vector3> normals;
        std::vector<SurfaceTriangle> triangles;
        std::unique_ptr<SurfacePattern> pattern;
        std::unique_ptr<ClothFolds> folds;
    };
    std::vector<SurfaceChart> surfaces(const Object& object, const UvAtlas& atlas, const TextureBakeInputs& inputs, const TextureBakeOptions& options)
    {
        size_t gridEntries = 0;
        std::map<PositionKey, Vector3> positions;
        for (const auto& vertex : object.vertices)
            positions.emplace(PositionKey(vertex), vertex);
        std::vector<SurfaceChart> charts;
        for (size_t ordinal = 0; ordinal < atlas.layouts.size(); ++ordinal) {
            const auto& layout = atlas.layouts[ordinal];
            const auto found = inputs.surfaces.find(layout.sourceId);
            if (!layout.id.isNull() || found == inputs.surfaces.end())
                continue;
            const auto& surface = found->second;
            SurfaceChart chart;
            chart.layoutIndex = ordinal;
            std::map<PositionKey, size_t> welded;
            for (const auto& entry : layout.globalUv) {
                SurfaceTriangle triangle;
                triangle.uvs = entry.second;
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto position = positions.find(entry.first[corner]);
                    if (position == positions.end())
                        invalid("Surface chart references a missing geometry position");
                    triangle.positions[corner] = position->second;
                    const auto inserted = welded.emplace(entry.first[corner], chart.vertices.size());
                    if (inserted.second)
                        chart.vertices.push_back(position->second);
                    triangle.vertices[corner] = inserted.first->second;
                }
                const auto edge1 = triangle.positions[1] - triangle.positions[0];
                const auto edge2 = triangle.positions[2] - triangle.positions[0];
                const auto du1 = triangle.uvs[1].x() - triangle.uvs[0].x();
                const auto dv1 = triangle.uvs[1].y() - triangle.uvs[0].y();
                const auto du2 = triangle.uvs[2].x() - triangle.uvs[0].x();
                const auto dv2 = triangle.uvs[2].y() - triangle.uvs[0].y();
                const auto determinant = du1 * dv2 - du2 * dv1;
                const auto normal = Vector3::crossProduct(edge1, edge2);
                if (std::abs(determinant) < 1e-18 || normal.lengthSquared() < 1e-30)
                    continue;
                const auto alongU = (edge1 * dv2 - edge2 * dv1) / determinant;
                const auto alongV = (edge2 * du1 - edge1 * du2) / determinant;
                if (alongU.lengthSquared() < 1e-30)
                    continue;
                triangle.tangent = alongU.normalized();
                triangle.handedness = Vector3::dotProduct(Vector3::crossProduct(normal, triangle.tangent), -alongV) < 0 ? -1 : 1;
                double worldLength = 0;
                double uvLength = 0;
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto next = (corner + 1) % 3;
                    worldLength += (triangle.positions[corner] - triangle.positions[next]).length();
                    uvLength += std::hypot(triangle.uvs[corner].x() - triangle.uvs[next].x(), triangle.uvs[corner].y() - triangle.uvs[next].y());
                }
                triangle.worldPerUv = worldLength / uvLength;
                chart.triangles.push_back(triangle);
            }
            if (chart.triangles.empty())
                continue;
            chart.normals.resize(chart.vertices.size());
            for (const auto& triangle : chart.triangles) {
                const auto normal = Vector3::crossProduct(triangle.positions[1] - triangle.positions[0], triangle.positions[2] - triangle.positions[0]);
                for (const auto vertex : triangle.vertices)
                    chart.normals[vertex] += normal;
            }
            for (auto& normal : chart.normals)
                normal.normalize();
            if (surface.pattern && surface.pattern->type != SurfacePattern::Type::None)
                chart.pattern = std::make_unique<SurfacePattern>(*surface.pattern, chart.vertices);
            if (!surface.folds.empty() && surface.wrinkleStrength > 0) {
                chart.foldScale = foldGridScale(surface);
                for (const auto& fold : surface.folds) {
                    const auto reach = fold.width * 1.15 * 2.6;
                    for (const auto& point : fold.points)
                        checkFoldCell(point, chart.foldScale, reach);
                }
                for (const auto& fold : surface.folds) {
                    const auto reach = fold.width * 1.15 * 2.6;
                    for (size_t index = 1; index < fold.points.size(); ++index) {
                        checkCancellation(options);
                        size_t cells = 1;
                        for (size_t axis = 0; axis < 3; ++axis) {
                            const auto extent = std::abs(fold.points[index][axis] - fold.points[index - 1][axis]) + 2 * reach;
                            const auto span = std::ceil(extent / chart.foldScale) + 2;
                            if (!std::isfinite(span) || span > options.maximumSpatialGridEntries)
                                throw TextureBakeError(TextureBakeErrorCode::limitExceeded, "Cloth spatial grid axis exceeds its work quota");
                            const auto count = static_cast<size_t>(span);
                            if (count > (options.maximumSpatialGridEntries - gridEntries) / cells)
                                throw TextureBakeError(TextureBakeErrorCode::limitExceeded, "Cloth spatial grid exceeds its cumulative work quota");
                            cells *= count;
                        }
                        gridEntries += cells;
                    }
                }
                chart.folds = std::make_unique<ClothFolds>(surface.folds, surface.wrinkleStrength);
                if (chart.folds->empty())
                    chart.folds.reset();
            }
            if (chart.pattern || chart.folds)
                charts.push_back(std::move(chart));
        }
        return charts;
    }
    void paintSurface(TextureImage& output, std::vector<uint8_t>& covered, const SurfaceChart& chart, const Color& baseColor, bool normalMap, const TextureBakeOptions& options)
    {
        const auto size = static_cast<int>(output.width);
        const double margin = 2.0 * size / (normalMap ? 2048 : 4096);
        std::vector<float> distance(output.width);
        for (int y = 0; y < size; ++y) {
            checkCancellation(options);
            std::fill(distance.begin(), distance.end(), std::numeric_limits<float>::max());
            for (const auto& triangle : chart.triangles) {
                double x[3], py[3];
                for (size_t corner = 0; corner < 3; ++corner) {
                    x[corner] = triangle.uvs[corner].x() * size;
                    py[corner] = triangle.uvs[corner].y() * size;
                }
                if (y + 0.5 < std::min({ py[0], py[1], py[2] }) - margin || y + 0.5 > std::max({ py[0], py[1], py[2] }) + margin)
                    continue;
                const double area = (x[1] - x[0]) * (py[2] - py[0]) - (x[2] - x[0]) * (py[1] - py[0]);
                if (std::abs(area) < 1e-12)
                    continue;
                double heights[3];
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto j = (corner + 1) % 3, k = (corner + 2) % 3;
                    const auto edge = std::hypot(x[k] - x[j], py[k] - py[j]);
                    heights[corner] = edge > 1e-12 ? std::abs(area) / edge : 0;
                }
                const int left = std::max(0, static_cast<int>(std::floor(std::min({ x[0], x[1], x[2] }) - margin)));
                const int right = std::min(size - 1, static_cast<int>(std::ceil(std::max({ x[0], x[1], x[2] }) + margin)));
                for (int column = left; column <= right; ++column) {
                    const double cx = column + 0.5, cy = y + 0.5;
                    double weights[3];
                    weights[0] = ((x[1] - cx) * (py[2] - cy) - (x[2] - cx) * (py[1] - cy)) / area;
                    weights[1] = ((x[2] - cx) * (py[0] - cy) - (x[0] - cx) * (py[2] - cy)) / area;
                    weights[2] = 1 - weights[0] - weights[1];
                    double outside = 0;
                    for (size_t corner = 0; corner < 3; ++corner)
                        outside = std::max(outside, -weights[corner] * heights[corner]);
                    if (outside > margin || static_cast<float>(outside) >= distance[column])
                        continue;
                    distance[column] = static_cast<float>(outside);
                    Vector3 position, normal;
                    double total = 0;
                    for (size_t corner = 0; corner < 3; ++corner) {
                        position += triangle.positions[corner] * weights[corner];
                        const auto clamped = std::max(0.0, weights[corner]);
                        normal += chart.normals[triangle.vertices[corner]] * clamped;
                        total += clamped;
                    }
                    normal = normal / std::max(1e-12, total);
                    if (normal.lengthSquared() < 1e-24)
                        normal = Vector3::normal(triangle.positions[0], triangle.positions[1], triangle.positions[2]);
                    normal.normalize();
                    auto tangent = triangle.tangent - normal * Vector3::dotProduct(triangle.tangent, normal);
                    if (tangent.lengthSquared() < 1e-24)
                        tangent = triangle.tangent;
                    tangent.normalize();
                    const auto bitangent = Vector3::crossProduct(normal, tangent) * triangle.handedness;
                    const auto footprint = triangle.worldPerUv / size;
                    Vector3 evaluatedNormal(0, 0, 1);
                    double cavity = 0;
                    if (chart.folds) {
                        checkFoldCell(position, chart.foldScale);
                        // evaluate() samples neighbouring points along tangent/bitangent.
                        const auto epsilon = std::max(1e-5, footprint * 0.75);
                        checkFoldCell(position, chart.foldScale, epsilon);
                        chart.folds->evaluate(position, normal, tangent, bitangent, footprint, &evaluatedNormal, &cavity);
                    }
                    Pixel pixel;
                    if (normalMap)
                        pixel = { byte(evaluatedNormal.x() * 0.5 + 0.5), byte(evaluatedNormal.y() * 0.5 + 0.5), byte(evaluatedNormal.z() * 0.5 + 0.5), 255 };
                    else {
                        if (chart.pattern) {
                            // Inherited Worley/noise kernels use signed integer lattice cells.
                            // Leave headroom for all octave multipliers, warps and neighbour cells.
                            const auto scale = chart.pattern->settings().scale;
                            for (size_t axis = 0; axis < 3; ++axis) {
                                const auto coordinate = position[axis] / scale;
                                if (!std::isfinite(coordinate) || std::abs(coordinate) > 1000000)
                                    invalid("Surface pattern coordinate exceeds the safe noise lattice range");
                            }
                        }
                        const auto color = chart.pattern ? chart.pattern->colorAt(position, normal, footprint) : baseColor;
                        const auto shade = 1 - 0.3 * std::min(1.0, cavity);
                        pixel = { byte(color.r() * shade), byte(color.g() * shade), byte(color.b() * shade), byte(color.alpha()) };
                    }
                    const auto index = static_cast<size_t>(y) * output.width + column;
                    put(output, index, pixel);
                    covered[index] = 1;
                }
            }
        }
    }
    std::vector<TextureTangent> tangents(const Object& object, const UvAtlas& atlas)
    {
        std::vector<TextureTangent> result(object.triangles.size());
        for (size_t index = 0; index < result.size(); ++index) {
            if (!atlas.triangleUvKnown[index])
                continue;
            const auto& triangle = object.triangles[index];
            const auto& uv = atlas.triangleUvs[index];
            const auto edge1 = object.vertices[triangle[1]] - object.vertices[triangle[0]];
            const auto edge2 = object.vertices[triangle[2]] - object.vertices[triangle[0]];
            const auto du1 = uv[1].x() - uv[0].x();
            const auto dv1 = uv[1].y() - uv[0].y();
            const auto du2 = uv[2].x() - uv[0].x();
            const auto dv2 = uv[2].y() - uv[0].y();
            const auto determinant = du1 * dv2 - du2 * dv1;
            const auto normal = Vector3::crossProduct(edge1, edge2);
            if (std::abs(determinant) < 1e-18 || normal.lengthSquared() < 1e-30)
                continue;
            const auto alongU = (edge1 * dv2 - edge2 * dv1) / determinant;
            const auto alongV = (edge2 * du1 - edge1 * du2) / determinant;
            if (alongU.lengthSquared() < 1e-30)
                continue;
            result[index].direction = alongU.normalized();
            result[index].handedness = Vector3::dotProduct(Vector3::crossProduct(normal, result[index].direction), -alongV) < 0 ? -1 : 1;
            result[index].known = true;
        }
        return result;
    }
}
TextureBakeResult TextureBaker::bake(const Object& object, const UvAtlas& atlas, const TextureBakeInputs& inputs, const TextureBakeOptions& options)
{
    validate(object, atlas, inputs, options);
    bool hasMetallic = false;
    bool hasRoughness = false;
    bool hasEmissive = false;
    for (const auto& layout : atlas.layouts) {
        const auto found = inputs.materials.find(layout.sourceId);
        if (found == inputs.materials.end())
            continue;
        hasMetallic = hasMetallic || found->second.metallic > 0;
        hasRoughness = hasRoughness || found->second.roughness < 1;
        hasEmissive = hasEmissive || found->second.emissive > 0;
    }
    hasRoughness = hasRoughness || hasMetallic;
    auto charts = surfaces(object, atlas, inputs, options);
    const bool hasNormal = std::any_of(charts.begin(), charts.end(), [](const SurfaceChart& chart) { return bool(chart.folds); });
    const size_t budget = options.maximumPayloadBytes ? options.maximumPayloadBytes : maximumBudget;
    size_t payload = 0;
    auto account = [&](size_t count, size_t elementSize) {
        if (count > (budget - payload) / elementSize)
            throw TextureBakeError(TextureBakeErrorCode::limitExceeded, "Texture output exceeds its logical payload budget");
        payload += count * elementSize;
    };
    account(object.triangles.size(), sizeof(TextureTangent));
    account(options.colorSize * options.colorSize, 4);
    if (hasNormal)
        account(options.normalSize * options.normalSize, 4);
    for (bool enabled : { hasMetallic, hasRoughness, hasEmissive }) {
        if (enabled)
            account(options.materialSize * options.materialSize, 4);
    }
    TextureBakeResult result;
    result.payloadBytes = payload;
    result.tangents = tangents(object, atlas);
    result.color = image(options.colorSize);
    std::set<size_t> colorSkipped, normalSkipped;
    for (const auto& chart : charts) {
        colorSkipped.insert(chart.layoutIndex);
        if (chart.folds)
            normalSkipped.insert(chart.layoutIndex);
    }
    auto colorCoverage = rectangles(result.color, atlas, 32.0 / 4096, options, [&](size_t ordinal, const UvMapPacker::Layout& layout, double u, double v) {
        for (const auto& gradient : atlas.gradients) {
            if (gradient.layoutIndex != ordinal)
                continue;
            const auto coordinate = layout.flipped ? v : u;
            const auto sample = std::min(gradient.resolution - 1, static_cast<size_t>(coordinate * gradient.resolution));
            const auto amount = static_cast<double>(sample) / (gradient.resolution - 1);
            Pixel pixel;
            for (size_t channel = 0; channel < 4; ++channel) {
                const auto large = static_cast<uint8_t>(gradient.largeColor[channel] * 255);
                const auto small = static_cast<uint8_t>(gradient.smallColor[channel] * 255);
                pixel[channel] = static_cast<uint8_t>(large * (1 - amount) + small * amount);
            }
            return pixel;
        }
        if (layout.id.isNull())
            return colorPixel(layout.color);
        const auto found = inputs.images.find(layout.id);
        if (found == inputs.images.end())
            invalid("Texture chart image has no resolved pixels");
        const auto& source = found->second;
        // The reference draws its integer-sized pixmap at the fractional chart origin.
        // Sample that translated pixel lattice rather than snapping the origin down.
        const auto left = layout.left * options.colorSize;
        const auto top = layout.top * options.colorSize;
        const auto width = static_cast<size_t>(layout.width * options.colorSize);
        const auto height = static_cast<size_t>(layout.height * options.colorSize);
        if (!layout.flipped) {
            u = std::clamp(u - (0.5 + left - std::floor(left)) / width, 0.0, 1.0);
            v = std::clamp(v - (0.5 + top - std::floor(top)) / height, 0.0, 1.0);
        }
        if (layout.flipped)
            std::swap(u, v);
        const auto x = std::min(source.width - 1, static_cast<size_t>(u * source.width));
        const auto y = std::min(source.height - 1, static_cast<size_t>(v * source.height));
        Pixel pixel;
        std::copy_n(source.pixels + y * source.rowStride + x * 4, 4, pixel.begin());
        return pixel; }, colorSkipped);
    for (const auto& chart : charts)
        paintSurface(result.color, colorCoverage, chart, atlas.layouts[chart.layoutIndex].color, false, options);
    dilate(result.color, colorCoverage, options);
    if (hasNormal) {
        result.normal = image(options.normalSize);
        auto normalCoverage = rectangles(*result.normal, atlas, 8.0 / 2048, options, [](size_t, const UvMapPacker::Layout&, double, double) -> Pixel { return { 128, 128, 255, 255 }; }, normalSkipped);
        for (const auto& chart : charts) {
            if (chart.folds)
                paintSurface(*result.normal, normalCoverage, chart, atlas.layouts[chart.layoutIndex].color, true, options);
        }
        dilate(*result.normal, normalCoverage, options);
    }
    auto materialImage = [&](int kind) {
        auto output = image(options.materialSize);
        auto coverage = rectangles(output, atlas, 8.0 / 1024, options, [&](size_t, const UvMapPacker::Layout& layout, double, double) -> Pixel {
            const auto found = inputs.materials.find(layout.sourceId);
            const auto material = found == inputs.materials.end() ? TextureMaterial {} : found->second;
            if (kind == 2) {
                const auto strength = std::min(1.0, material.emissive);
                return { byte(layout.color.r() * strength), byte(layout.color.g() * strength), byte(layout.color.b() * strength), 255 };
            }
            const auto value = byte(kind == 0 ? material.metallic : material.roughness);
            return { value, value, value, 255 };
        });
        dilate(output, coverage, options);
        return output;
    };
    if (hasMetallic)
        result.metallic = materialImage(0);
    if (hasRoughness)
        result.roughness = materialImage(1);
    if (hasEmissive)
        result.emissive = materialImage(2);
    checkCancellation(options);
    return result;
}
}

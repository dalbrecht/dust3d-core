// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/uv/source_texture_baker.h>
#include <limits>
namespace dust3d {
namespace {
    using Channels = std::array<double, 4>;
    using Pixel = std::array<uint8_t, 4>;
    [[noreturn]] void invalid(const char* message) { throw TextureBakeError(TextureBakeErrorCode::invalidInput, message); }
    [[noreturn]] void limit(const char* message) { throw TextureBakeError(TextureBakeErrorCode::limitExceeded, message); }
    void checkCancellation(const SourceTextureBakeOptions& options)
    {
        if (options.cancellation && options.cancellation->load(std::memory_order_relaxed))
            throw TextureBakeError(TextureBakeErrorCode::cancelled, "Source texture baking cancelled");
    }
    struct Work {
        const SourceTextureBakeOptions& options;
        size_t samples = 0;
        void step()
        {
            if (samples == options.maximumRasterSamples)
                limit("Source texture raster work exceeds its configured quota");
            if (samples++ % 4096 == 0)
                checkCancellation(options);
        }
    };
    bool unit(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
    uint8_t byte(double value)
    {
        return static_cast<uint8_t>(std::round(std::clamp(value, 0.0, 1.0) * 255));
    }
    double decode(double value) { return value <= .04045 ? value / 12.92 : std::pow((value + .055) / 1.055, 2.4); }
    uint8_t encode(double value)
    {
        return byte(value <= .0031308 ? value * 12.92 : 1.055 * std::pow(value, 1.0 / 2.4) - .055);
    }
    void validatePolicy(const SourceTexturePolicy& policy)
    {
        if ((policy.alphaMode != SourceTextureAlphaMode::opaque && policy.alphaMode != SourceTextureAlphaMode::mask && policy.alphaMode != SourceTextureAlphaMode::blend) || !std::isfinite(policy.alphaCutoff) || policy.alphaCutoff < 0)
            invalid("Source texture alpha policy is invalid");
    }
    void validateView(const TextureImageView& view)
    {
        if (!view.width || !view.height || view.width > 8192 || view.height > 8192 || !view.pixels)
            invalid("Legacy texture image dimensions or pixels are invalid");
        size_t row = view.width * 4;
        if (view.rowStride < row || view.byteCount < row || (view.height > 1 && view.rowStride > (view.byteCount - row) / (view.height - 1)))
            invalid("Legacy texture image readable stride/span is invalid");
    }
    void validateChart(const SourceUvChart& chart)
    {
        try {
            (void)chart.map({ 0, 0 });
            (void)chart.map({ 1, 1 });
        } catch (const UvAtlasError& error) {
            throw TextureBakeError(TextureBakeErrorCode::invalidInput, error.what());
        }
    }
    bool contains(const SourceUvChart& chart, const Vector2& uv)
    {
        return unit(uv.x()) && unit(uv.y()) && uv.x() >= chart.left - 1e-7 && uv.y() >= chart.top - 1e-7
            && uv.x() <= chart.left + chart.width + 1e-7 && uv.y() <= chart.top + chart.height + 1e-7;
    }
    struct Event {
        double x;
        double top;
        double bottom;
        size_t ordinal;
        bool starts;
    };
    bool hasChartOverlap(const std::vector<SourceUvChart>& charts, const SourceTextureBakeOptions& options, double padding)
    {
        std::vector<Event> events;
        events.reserve(charts.size() * 2);
        for (size_t index = 0; index < charts.size(); ++index) {
            checkCancellation(options);
            auto chart = charts[index];
            const double right = std::min(1.0, chart.left + chart.width + padding);
            const double bottom = std::min(1.0, chart.top + chart.height + padding);
            chart.left = std::max(0.0, chart.left - padding);
            chart.top = std::max(0.0, chart.top - padding);
            chart.width = right - chart.left;
            chart.height = bottom - chart.top;
            events.push_back({ chart.left, chart.top, chart.top + chart.height, index, true });
            events.push_back({ chart.left + chart.width, chart.top, chart.top + chart.height, index, false });
        }
        std::sort(events.begin(), events.end(), [](const Event& left, const Event& right) {
            if (left.x != right.x)
                return left.x < right.x;
            if (left.starts != right.starts)
                return !left.starts;
            return left.ordinal < right.ordinal;
        });
        std::map<std::pair<double, size_t>, double> active;
        for (const auto& event : events) {
            checkCancellation(options);
            auto key = std::make_pair(event.top, event.ordinal);
            if (!event.starts) {
                active.erase(key);
                continue;
            }
            auto next = active.lower_bound(key);
            if ((next != active.end() && next->first.first < event.bottom) || (next != active.begin() && std::prev(next)->second > event.top)) {
                return true;
            }
            active.emplace(key, event.bottom);
        }
        return false;
    }
    void rejectOverlaps(const std::vector<SourceUvChart>& charts, const SourceTextureBakeOptions& options, double padding = 0)
    {
        if (!hasChartOverlap(charts, options, padding))
            return;
        if (padding > 0)
            limit("Output texture resolution cannot reserve one texel of chart padding; increase map dimensions");
        invalid("Source texture charts overlap");
    }
    struct Geometry {
        std::array<Vector2, 3> pixels;
        double determinant;
    };
    Geometry geometry(const SourceUvTriangle& triangle, size_t dimension)
    {
        Geometry result;
        for (size_t corner = 0; corner < 3; ++corner)
            result.pixels[corner] = triangle.packedUvs[corner] * static_cast<double>(dimension);
        auto first = result.pixels[1] - result.pixels[0];
        auto second = result.pixels[2] - result.pixels[0];
        result.determinant = first.x() * second.y() - second.x() * first.y();
        if (!std::isfinite(result.determinant) || result.determinant == 0)
            invalid("Imported packed texture triangle is degenerate");
        return result;
    }
    void validate(const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, const SourceTextureBakeOptions& options)
    {
        checkCancellation(options);
        for (auto size : { options.colorSize, options.materialSize, options.normalSize })
            if (!size || size > 8192)
                invalid("Source texture dimensions must be in 1..8192");
        if (options.maximumPayloadBytes > 256 * 1024 * 1024 || !options.maximumRasterSamples || options.maximumRasterSamples > 1000000000 || !options.maximumChartCount || options.maximumChartCount > 100000)
            invalid("Source texture payload/work quotas exceed supported ranges");
        if (inputs.materials.size() > 10000)
            limit("Source texture material catalog exceeds its work limit");
        for (const auto& [identity, material] : inputs.materials) {
            checkCancellation(options);
            if (identity.empty() || (material.vertexColorEncoding != SourceColorEncoding::linear && material.vertexColorEncoding != SourceColorEncoding::srgb)
                || !unit(material.metallic) || !unit(material.roughness))
                invalid("Source texture material identity, encoding or scalars are invalid");
            for (double value : material.baseColor)
                if (!unit(value))
                    invalid("Source texture base-color factor is invalid");
            for (double value : material.emissive)
                if (!unit(value))
                    invalid("Source texture emissive factor is invalid");
            validatePolicy(material.policy);
            if (material.colorTexture && (!material.colorTexture->width() || !material.colorTexture->height()))
                invalid("Source texture sampler must own a nonempty image");
        }
        validatePolicy(inputs.legacy.policy);
        for (const auto* view : { &inputs.legacy.color, &inputs.legacy.normal, &inputs.legacy.metallic, &inputs.legacy.roughness, &inputs.legacy.emissive }) {
            if (*view) {
                if (!atlas.legacyChart)
                    invalid("Legacy texture images require a packed legacy region");
                validateView(**view);
            }
        }
        std::vector<SourceUvChart> charts;
        if (atlas.legacyChart) {
            validateChart(*atlas.legacyChart);
            if (!inputs.legacy.color)
                invalid("A legacy atlas region requires its resolved color image");
            charts.push_back(*atlas.legacyChart);
        }
        for (size_t index = 0; index < atlas.triangles.size(); ++index) {
            checkCancellation(options);
            const auto& triangle = atlas.triangles[index];
            if (triangle.modelKey.empty()) {
                if (triangle.importedChart || triangle.authoredUvs)
                    invalid("Procedural texture triangles cannot claim imported charts or authored UVs");
                if (triangle.known) {
                    if (!atlas.legacyChart)
                        invalid("Known procedural texture triangles require a legacy region");
                    for (const auto& uv : triangle.packedUvs)
                        if (!contains(*atlas.legacyChart, uv))
                            invalid("Procedural packed UVs exceed their legacy region");
                }
                continue;
            }
            if (!triangle.known || !triangle.importedChart || inputs.triangleColors.size() != atlas.triangles.size())
                invalid("Imported texture triangles require aligned colors and known packed charts");
            validateChart(*triangle.importedChart);
            if (charts.size() == options.maximumChartCount)
                limit("Source texture chart count exceeds its work limit");
            charts.push_back(*triangle.importedChart);
            for (const auto& uv : triangle.packedUvs)
                if (!contains(*triangle.importedChart, uv))
                    invalid("Imported packed UVs exceed their chart");
            (void)geometry(triangle, options.colorSize);
            const auto found = inputs.materials.find(triangle.modelKey);
            if (found == inputs.materials.end())
                invalid("Imported texture triangle has no exact-key material owner");
            if (found->second.colorTexture && !triangle.authoredUvs)
                invalid("Textured imported triangles require authored source UVs");
            if (triangle.authoredUvs)
                for (const auto& uv : *triangle.authoredUvs)
                    if (!std::isfinite(uv.x()) || !std::isfinite(uv.y()))
                        invalid("Authored texture coordinates must be finite");
            for (const auto& color : inputs.triangleColors[index])
                for (size_t channel = 0; channel < 4; ++channel)
                    if (!unit(color[channel]))
                        invalid("Source texture corner colors must be normalized and finite");
        }
        rejectOverlaps(charts, options);
        size_t minimumSize = std::min(options.colorSize, options.materialSize);
        if (inputs.legacy.normal)
            minimumSize = std::min(minimumSize, options.normalSize);
        rejectOverlaps(charts, options, 1.0 / minimumSize);
    }
    double lod(const Geometry& geometry, const SourceUvTriangle& triangle, const SourceTextureMaterial& material)
    {
        if (!material.colorTexture)
            return 0;
        const auto& uv = *triangle.authoredUvs;
        const auto first = geometry.pixels[1] - geometry.pixels[0];
        const auto second = geometry.pixels[2] - geometry.pixels[0];
        double du1 = uv[1].x() - uv[0].x(), du2 = uv[2].x() - uv[0].x();
        double dv1 = uv[1].y() - uv[0].y(), dv2 = uv[2].y() - uv[0].y();
        double duDx = (du1 * second.y() - du2 * first.y()) / geometry.determinant;
        double duDy = (first.x() * du2 - second.x() * du1) / geometry.determinant;
        double dvDx = (dv1 * second.y() - dv2 * first.y()) / geometry.determinant;
        double dvDy = (first.x() * dv2 - second.x() * dv1) / geometry.determinant;
        double rho = std::max(std::hypot(duDx * material.colorTexture->width(), dvDx * material.colorTexture->height()),
            std::hypot(duDy * material.colorTexture->width(), dvDy * material.colorTexture->height()));
        if (!std::isfinite(duDx) || !std::isfinite(duDy) || !std::isfinite(dvDx) || !std::isfinite(dvDy) || !std::isfinite(rho))
            invalid("Source UV footprint exceeds finite sampling arithmetic");
        return rho > 0 ? std::log2(rho) : 0;
    }
    Pixel evaluate(const SourceUvTriangle& triangle, const SourceTextureMaterial& material, const std::array<Channels, 3>& colors, const Channels& weights, double level, int kind)
    {
        if (kind == 1 || kind == 2) {
            uint8_t value = byte(kind == 1 ? material.metallic : material.roughness);
            return { value, value, value, 255 };
        }
        if (kind == 3)
            return { encode(material.emissive[0]), encode(material.emissive[1]), encode(material.emissive[2]), 255 };
        if (kind == 4)
            return { 128, 128, 255, 255 };
        Channels sampled { 1, 1, 1, 1 };
        if (material.colorTexture) {
            Vector2 uv;
            for (size_t corner = 0; corner < 3; ++corner)
                uv = uv + triangle.authoredUvs->at(corner) * weights[corner];
            sampled = material.colorTexture->sampleLinear(uv.x(), uv.y(), level);
        }
        Channels value {};
        for (size_t channel = 0; channel < 4; ++channel) {
            for (size_t corner = 0; corner < 3; ++corner)
                value[channel] += colors[corner][channel] * weights[corner];
            value[channel] *= material.baseColor[channel] * sampled[channel];
        }
        if (material.policy.alphaMode == SourceTextureAlphaMode::opaque)
            value[3] = 1;
        return { encode(value[0]), encode(value[1]), encode(value[2]), byte(value[3]) };
    }
    void put(TextureImage& image, size_t index, const Pixel& pixel) { std::copy(pixel.begin(), pixel.end(), image.pixels.begin() + index * 4); }
    std::vector<uint32_t> chartOwnershipForCharts(size_t size, const std::vector<const SourceUvChart*>& charts, Work& work, double margin, bool shouldSpread)
    {
        constexpr uint32_t unknown = std::numeric_limits<uint32_t>::max();
        std::vector<uint32_t> owners(size * size, unknown);
        std::vector<uint32_t> queue;
        if (shouldSpread)
            queue.reserve(size * size);
        for (size_t ordinal = 0; ordinal < charts.size(); ++ordinal) {
            auto chart = *charts[ordinal];
            // When requested, reserve complete bilinear collars before assigning
            // distant gaps so Manhattan ties cannot steal diagonal corners.
            const double right = std::min(1.0, chart.left + chart.width + margin);
            const double bottom = std::min(1.0, chart.top + chart.height + margin);
            chart.left = std::max(0.0, chart.left - margin);
            chart.top = std::max(0.0, chart.top - margin);
            chart.width = right - chart.left;
            chart.height = bottom - chart.top;
            size_t startX = static_cast<size_t>(std::floor(chart.left * size));
            size_t startY = static_cast<size_t>(std::floor(chart.top * size));
            size_t endX = std::min(size, static_cast<size_t>(std::ceil((chart.left + chart.width) * size)));
            size_t endY = std::min(size, static_cast<size_t>(std::ceil((chart.top + chart.height) * size)));
            for (size_t y = startY; y < endY; ++y) {
                checkCancellation(work.options);
                for (size_t x = startX; x < endX; ++x) {
                    work.step();
                    double u = ((x + .5) / size - chart.left) / chart.width;
                    double v = ((y + .5) / size - chart.top) / chart.height;
                    if (u < 0 || u >= 1 || v < 0 || v >= 1)
                        continue;
                    const size_t index = y * size + x;
                    if (owners[index] != unknown)
                        invalid("Packed charts claim the same output texel ownership");
                    owners[index] = static_cast<uint32_t>(ordinal);
                    if (shouldSpread)
                        queue.push_back(static_cast<uint32_t>(index));
                }
            }
        }
        if (!shouldSpread)
            return owners;
        // Seed entire rectangles, not colored triangle pixels: even distant uncovered
        // chart corners and nearby padding must belong to their own material.
        for (size_t cursor = 0; cursor < queue.size(); ++cursor) {
            work.step();
            const size_t index = queue[cursor], x = index % size, y = index / size;
            auto spread = [&](size_t next) {
                if (owners[next] != unknown)
                    return;
                owners[next] = owners[index];
                queue.push_back(static_cast<uint32_t>(next));
            };
            if (x)
                spread(index - 1);
            if (x + 1 < size)
                spread(index + 1);
            if (y)
                spread(index - size);
            if (y + 1 < size)
                spread(index + size);
        }
        return owners;
    }

    std::vector<uint32_t> chartOwnership(size_t size, const SourceUvAtlas& atlas, Work& work)
    {
        std::vector<const SourceUvChart*> charts;
        if (atlas.legacyChart)
            charts.push_back(&*atlas.legacyChart);
        for (const auto& triangle : atlas.triangles)
            if (triangle.importedChart)
                charts.push_back(&*triangle.importedChart);
        return chartOwnershipForCharts(size, charts, work, 1.0 / size, true);
    }

    void dilate(TextureImage& image, std::vector<uint8_t>& coverage, const std::vector<uint32_t>& owners, Work& work)
    {
        std::vector<uint32_t> queue;
        queue.reserve(coverage.size());
        for (size_t index = 0; index < coverage.size(); ++index) {
            work.step();
            if (coverage[index])
                queue.push_back(static_cast<uint32_t>(index));
        }
        for (size_t cursor = 0; cursor < queue.size(); ++cursor) {
            work.step();
            size_t index = queue[cursor], x = index % image.width, y = index / image.width;
            auto spread = [&](size_t next) {
                if (coverage[next] || owners[next] != owners[index])
                    return;
                coverage[next] = 1;
                std::copy_n(image.pixels.data() + index * 4, 4, image.pixels.data() + next * 4);
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
    TextureImage render(size_t size, int kind, const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, Work& work)
    {
        TextureImage image { size, size, std::vector<uint8_t>(size * size * 4) };
        Pixel initial = kind == 2 ? Pixel { 255, 255, 255, 255 } : kind == 4 ? Pixel { 128, 128, 255, 255 }
                                                                             : Pixel { 0, 0, 0, uint8_t(kind == 0 ? 0 : 255) };
        for (size_t index = 0; index < size * size; ++index) {
            work.step();
            put(image, index, initial);
        }
        std::vector<uint8_t> coverage(size * size);
        const auto owners = chartOwnership(size, atlas, work);
        if (atlas.legacyChart) {
            const auto& chart = *atlas.legacyChart;
            const auto* view = kind == 0 ? &inputs.legacy.color : kind == 1 ? &inputs.legacy.metallic
                : kind == 2                                                 ? &inputs.legacy.roughness
                : kind == 3                                                 ? &inputs.legacy.emissive
                                                                            : &inputs.legacy.normal;
            size_t painted = 0;
            for (size_t y = 0; y < size; ++y) {
                checkCancellation(work.options);
                for (size_t x = 0; x < size; ++x) {
                    work.step();
                    double u = ((x + .5) / size - chart.left) / chart.width;
                    double v = ((y + .5) / size - chart.top) / chart.height;
                    if (u < 0 || u >= 1 || v < 0 || v >= 1)
                        continue;
                    if (chart.flipped)
                        std::swap(u, v);
                    Pixel pixel = initial;
                    if (*view) {
                        const auto& source = **view;
                        size_t sourceX = std::min(source.width - 1, static_cast<size_t>(u * source.width));
                        size_t sourceY = std::min(source.height - 1, static_cast<size_t>(v * source.height));
                        std::copy_n(source.pixels + sourceY * source.rowStride + sourceX * 4, 4, pixel.begin());
                        if (kind == 4 && chart.flipped) {
                            uint8_t red = pixel[0];
                            pixel[0] = 255 - pixel[1];
                            pixel[1] = 255 - red;
                        }
                    }
                    const size_t index = y * size + x;
                    put(image, index, pixel);
                    coverage[index] = 1;
                    ++painted;
                }
            }
            if (!painted)
                limit("Output texture resolution cannot retain the legacy atlas region");
        }
        uint32_t chartOwner = atlas.legacyChart ? 1 : 0;
        for (size_t ordinal = 0; ordinal < atlas.triangles.size(); ++ordinal) {
            checkCancellation(work.options);
            const auto& triangle = atlas.triangles[ordinal];
            if (triangle.modelKey.empty())
                continue;
            const uint32_t currentOwner = chartOwner++;
            const auto& material = inputs.materials.at(triangle.modelKey);
            const auto shape = geometry(triangle, size);
            double level = kind == 0 ? lod(shape, triangle, material) : 0;
            std::array<Channels, 3> colors;
            for (size_t corner = 0; corner < 3; ++corner)
                for (size_t channel = 0; channel < 4; ++channel) {
                    double value = inputs.triangleColors[ordinal][corner][channel];
                    colors[corner][channel] = channel < 3 && material.vertexColorEncoding == SourceColorEncoding::srgb ? decode(value) : value;
                }
            double minimumX = 1, minimumY = 1, maximumX = 0, maximumY = 0;
            for (const auto& uv : triangle.packedUvs) {
                minimumX = std::min(minimumX, uv.x());
                minimumY = std::min(minimumY, uv.y());
                maximumX = std::max(maximumX, uv.x());
                maximumY = std::max(maximumY, uv.y());
            }
            size_t startX = static_cast<size_t>(std::floor(minimumX * size)), startY = static_cast<size_t>(std::floor(minimumY * size));
            size_t endX = std::min(size, static_cast<size_t>(std::ceil(maximumX * size))), endY = std::min(size, static_cast<size_t>(std::ceil(maximumY * size)));
            size_t painted = 0;
            auto first = shape.pixels[1] - shape.pixels[0], second = shape.pixels[2] - shape.pixels[0];
            for (size_t y = startY; y < endY; ++y) {
                checkCancellation(work.options);
                for (size_t x = startX; x < endX; ++x) {
                    work.step();
                    auto offset = Vector2(x + .5, y + .5) - shape.pixels[0];
                    Channels weights {};
                    weights[1] = (offset.x() * second.y() - offset.y() * second.x()) / shape.determinant;
                    weights[2] = (first.x() * offset.y() - first.y() * offset.x()) / shape.determinant;
                    weights[0] = 1 - weights[1] - weights[2];
                    if (!std::isfinite(weights[0]) || !std::isfinite(weights[1]) || !std::isfinite(weights[2]))
                        invalid("Packed texture barycentric arithmetic is not finite");
                    if (weights[0] < -1e-9 || weights[1] < -1e-9 || weights[2] < -1e-9)
                        continue;
                    double sum = 0;
                    for (size_t corner = 0; corner < 3; ++corner) {
                        weights[corner] = std::clamp(weights[corner], 0.0, 1.0);
                        sum += weights[corner];
                    }
                    for (size_t corner = 0; corner < 3; ++corner)
                        weights[corner] /= sum;
                    size_t index = y * size + x;
                    if (coverage[index] || owners[index] != currentOwner)
                        invalid("Imported texture triangle crosses chart-local texel ownership");
                    put(image, index, evaluate(triangle, material, colors, weights, level, kind));
                    coverage[index] = 1;
                    ++painted;
                }
            }
            if (!painted)
                limit("Output texture resolution cannot retain every imported triangle; increase map dimensions");
        }
        dilate(image, coverage, owners, work);
        return image;
    }

    TextureImage filterMip(const TextureImage& source, int kind, const std::vector<const SourceUvChart*>& charts, const std::vector<uint32_t>& previousOwners, Work& work)
    {
        const size_t size = source.width / 2;
        const auto owners = chartOwnershipForCharts(size, charts, work, 1.0 / size, true);
        TextureImage image { size, size, std::vector<uint8_t>(size * size * 4) };
        std::vector<uint8_t> coverage(size * size);
        const double scale = double(source.width) / size;
        for (size_t y = 0; y < size; ++y) {
            checkCancellation(work.options);
            const double top = y * scale, bottom = (y + 1) * scale;
            for (size_t x = 0; x < size; ++x) {
                work.step();
                const size_t destination = y * size + x;
                const double left = x * scale, right = (x + 1) * scale;
                Channels sum {};
                double weight = 0;
                const size_t endY = std::min(source.height, static_cast<size_t>(std::ceil(bottom)));
                const size_t endX = std::min(source.width, static_cast<size_t>(std::ceil(right)));
                for (size_t sourceY = static_cast<size_t>(std::floor(top)); sourceY < endY; ++sourceY)
                    for (size_t sourceX = static_cast<size_t>(std::floor(left)); sourceX < endX; ++sourceX) {
                        work.step();
                        const size_t origin = sourceY * source.width + sourceX;
                        if (previousOwners[origin] != owners[destination])
                            continue;
                        const double area = (std::min(right, double(sourceX + 1)) - std::max(left, double(sourceX)))
                            * (std::min(bottom, double(sourceY + 1)) - std::max(top, double(sourceY)));
                        if (area <= 0)
                            continue;
                        for (size_t channel = 0; channel < 4; ++channel) {
                            double value = source.pixels[origin * 4 + channel] / 255.0;
                            if (channel < 3) {
                                if (kind == 0 || kind == 3)
                                    value = decode(value);
                                else if (kind == 4)
                                    value = value * 2 - 1;
                            }
                            sum[channel] += value * area;
                        }
                        weight += area;
                    }
                if (!weight)
                    continue;
                for (auto& value : sum)
                    value /= weight;
                if (kind == 4) {
                    const double length = std::sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
                    for (size_t channel = 0; channel < 3; ++channel)
                        sum[channel] = .5 + .5 * (length > 1e-12 ? sum[channel] / length : (channel == 2 ? 1 : 0));
                }
                Pixel pixel {};
                for (size_t channel = 0; channel < 4; ++channel)
                    pixel[channel] = channel < 3 && (kind == 0 || kind == 3) ? encode(sum[channel]) : byte(sum[channel]);
                put(image, destination, pixel);
                coverage[destination] = 1;
            }
        }
        // Lower-resolution collars may claim gaps outside the previous collar.
        // Fill those texels from their own chart, never a neighboring material.
        dilate(image, coverage, owners, work);
        for (auto covered : coverage) {
            work.step();
            if (!covered)
                throw TextureBakeError(TextureBakeErrorCode::bakingFailed, "Mip chart has no retained source coverage");
        }
        return image;
    }
}
SourceTextureBakeResult SourceTextureBaker::bake(const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, const SourceTextureBakeOptions& options)
{
    validate(atlas, inputs, options);
    const size_t maximum = options.maximumPayloadBytes ? options.maximumPayloadBytes : 256 * 1024 * 1024;
    RetainedPayloadBudget planned(maximum);
    auto charge = [](RetainedPayloadBudget& budget, size_t count, size_t width) {
        if (!budget.tryAdd(count, width))
            limit("Source texture owned output exceeds its retained payload quota");
    };
    charge(planned, 1, sizeof(SourceTextureBakeResult));
    charge(planned, atlas.triangles.size(), sizeof(std::optional<SourceTexturePolicy>));
    charge(planned, options.colorSize * options.colorSize, 4);
    charge(planned, options.materialSize * options.materialSize, 12);
    if (inputs.legacy.normal)
        charge(planned, options.normalSize * options.normalSize, 4);
    Work work { options };
    SourceTextureBakeResult result;
    result.trianglePolicies.resize(atlas.triangles.size());
    for (size_t index = 0; index < atlas.triangles.size(); ++index) {
        checkCancellation(options);
        const auto& triangle = atlas.triangles[index];
        if (!triangle.modelKey.empty())
            result.trianglePolicies[index] = inputs.materials.at(triangle.modelKey).policy;
        else if (triangle.known)
            result.trianglePolicies[index] = inputs.legacy.policy;
    }
    result.color = render(options.colorSize, 0, atlas, inputs, work);
    result.metallic = render(options.materialSize, 1, atlas, inputs, work);
    result.roughness = render(options.materialSize, 2, atlas, inputs, work);
    result.emissive = render(options.materialSize, 3, atlas, inputs, work);
    if (inputs.legacy.normal)
        result.normal = render(options.normalSize, 4, atlas, inputs, work);
    RetainedPayloadBudget retained(maximum);
    charge(retained, 1, sizeof(SourceTextureBakeResult));
    charge(retained, result.trianglePolicies.capacity(), sizeof(std::optional<SourceTexturePolicy>));
    for (const auto* image : { &result.color, &result.metallic, &result.roughness, &result.emissive })
        charge(retained, image->pixels.capacity(), 1);
    if (result.normal)
        charge(retained, result.normal->pixels.capacity(), 1);
    checkCancellation(options);
    result.payloadBytes = retained.usedBytes();
    result.rasterSamples = work.samples;
    return result;
}

SourceTextureMipBakeResult SourceTextureBaker::bakeMipChain(const SourceUvAtlas& atlas, const SourceTextureBakeInputs& inputs, const SourceTextureMipBakeOptions& options)
{
    if (!options.maximumLevels || options.maximumLevels > 14)
        invalid("Source texture mip level count must be in 1..14");
    SourceTextureMipBakeResult result;
    result.base = bake(atlas, inputs, options.bake);
    Work work { options.bake, result.base.rasterSamples };
    std::vector<SourceUvChart> charts;
    bool hasLegacyOwnership = !atlas.legacyChart;
    if (atlas.legacyChart && options.legacyAtlas && !options.legacyAtlas->layouts.empty()) {
        const auto& legacy = *options.legacyAtlas;
        if (legacy.layouts.size() > options.bake.maximumChartCount)
            limit("Procedural mip chart count exceeds its work limit");
        for (const auto& layout : legacy.layouts) {
            checkCancellation(options.bake);
            SourceUvChart local { layout.left, layout.top, layout.width, layout.height, false };
            validateChart(local);
            const auto first = atlas.legacyChart->map(local.map({ 0, 0 }));
            const auto last = atlas.legacyChart->map(local.map({ 1, 1 }));
            charts.push_back({ first.x(), first.y(), last.x() - first.x(), last.y() - first.y(), false });
            validateChart(charts.back());
        }
        bool allKnownTrianglesHaveOwner = true;
        for (size_t index = 0; index < atlas.triangles.size(); ++index) {
            checkCancellation(options.bake);
            const auto& triangle = atlas.triangles[index];
            if (!triangle.modelKey.empty() || !triangle.known)
                continue;
            if (legacy.triangleUvs.size() != atlas.triangles.size() || legacy.triangleUvKnown.size() != atlas.triangles.size() || legacy.triangleUvKnown[index] != 1)
                invalid("Procedural mip atlas does not match known source triangles");
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& local = legacy.triangleUvs[index][corner];
                if (!unit(local.x()) || !unit(local.y()))
                    invalid("Procedural mip source UVs must be normalized and finite");
                const auto expected = atlas.legacyChart->map(local);
                if (std::abs(expected.x() - triangle.packedUvs[corner].x()) > 1e-7 || std::abs(expected.y() - triangle.packedUvs[corner].y()) > 1e-7)
                    invalid("Procedural mip atlas UVs do not match the packed source");
            }
            bool hasOwner = false;
            for (const auto& chart : charts) {
                work.step();
                if (std::all_of(triangle.packedUvs.begin(), triangle.packedUvs.end(), [&](const Vector2& uv) { return contains(chart, uv); })) {
                    hasOwner = true;
                    break;
                }
            }
            if (!hasOwner)
                allKnownTrianglesHaveOwner = false;
        }
        // Without a proof for every known triangle, keep the valid base bake and
        // forbid mip sampling; a guessed island could mix unrelated surfaces.
        hasLegacyOwnership = allKnownTrianglesHaveOwner;
        result.hasUnprovenLegacyOwnership = !allKnownTrianglesHaveOwner;
    }
    for (const auto& triangle : atlas.triangles) {
        checkCancellation(options.bake);
        if (triangle.importedChart) {
            if (charts.size() == options.bake.maximumChartCount)
                limit("Combined mip chart count exceeds its work limit");
            charts.push_back(*triangle.importedChart);
        }
    }
    size_t minimumSize = std::min(result.base.color.width, result.base.metallic.width);
    if (result.base.normal)
        minimumSize = std::min(minimumSize, result.base.normal->width);
    std::array<size_t, 5> mapSizes { result.base.color.width, result.base.metallic.width, result.base.roughness.width, result.base.emissive.width, result.base.normal ? result.base.normal->width : 0 };
    size_t additional = 0;
    auto hasInteriorCenters = [&](size_t size) {
        for (const auto& chart : charts) {
            checkCancellation(options.bake);
            const double x = std::max(0.0, std::ceil(chart.left * size - .5));
            const double y = std::max(0.0, std::ceil(chart.top * size - .5));
            if (x >= size || y >= size || x + .5 >= (chart.left + chart.width) * size || y + .5 >= (chart.top + chart.height) * size)
                return false;
        }
        return true;
    };
    auto hasAllInteriorCenters = [&] {
        for (size_t size : mapSizes)
            if (size && !hasInteriorCenters(size))
                return false;
        return true;
    };
    rejectOverlaps(charts, options.bake);
    const bool canFilter = hasLegacyOwnership && hasAllInteriorCenters() && !hasChartOverlap(charts, options.bake, 1.0 / minimumSize);
    while (canFilter && minimumSize > 1 && additional + 1 < options.maximumLevels) {
        minimumSize /= 2;
        for (auto& size : mapSizes)
            size /= 2;
        if (!hasAllInteriorCenters() || hasChartOverlap(charts, options.bake, 1.0 / minimumSize))
            break;
        ++additional;
    }
    std::vector<const SourceUvChart*> chartViews;
    for (const auto& chart : charts)
        chartViews.push_back(&chart);
    const size_t maximum = options.bake.maximumPayloadBytes ? options.bake.maximumPayloadBytes : 256 * 1024 * 1024;
    auto charge = [](RetainedPayloadBudget& budget, size_t count, size_t width = 1) {
        if (!budget.tryAdd(count, width))
            limit("Source texture mip chain exceeds its retained payload quota");
    };
    auto accountBase = [&](RetainedPayloadBudget& budget) {
        charge(budget, 1, sizeof(SourceTextureMipBakeResult));
        charge(budget, result.base.trianglePolicies.capacity(), sizeof(std::optional<SourceTexturePolicy>));
        for (const auto* image : { &result.base.color, &result.base.metallic, &result.base.roughness, &result.base.emissive })
            charge(budget, image->pixels.capacity());
        if (result.base.normal)
            charge(budget, result.base.normal->pixels.capacity());
    };
    RetainedPayloadBudget planned(maximum);
    accountBase(planned);
    auto planLevels = [&](size_t size) {
        charge(planned, additional, sizeof(TextureImage));
        for (size_t level = 0; level < additional; ++level) {
            size /= 2;
            charge(planned, size * size, 4);
        }
    };
    planLevels(result.base.color.width);
    for (size_t map = 0; map < 3; ++map)
        planLevels(result.base.metallic.width);
    if (result.base.normal)
        planLevels(result.base.normal->width);
    auto admitBaseSamples = [&](std::vector<uint32_t>& owners, size_t size, int kind) {
        if (!atlas.legacyChart || !options.legacyAtlas)
            return;
        const auto& view = kind == 0 ? inputs.legacy.color : kind == 1 ? inputs.legacy.metallic
            : kind == 2                                                ? inputs.legacy.roughness
            : kind == 3                                                ? inputs.legacy.emissive
                                                                       : inputs.legacy.normal;
        if (!view)
            return;
        const auto& packed = *atlas.legacyChart;
        for (size_t index = 0; index < owners.size(); ++index) {
            work.step();
            if (owners[index] >= options.legacyAtlas->layouts.size())
                continue;
            double u = ((index % size + .5) / size - packed.left) / packed.width;
            double v = ((index / size + .5) / size - packed.top) / packed.height;
            if (packed.flipped)
                std::swap(u, v);
            const size_t x = std::min(view->width - 1, static_cast<size_t>(std::clamp(u, 0.0, 1.0) * view->width));
            const size_t y = std::min(view->height - 1, static_cast<size_t>(std::clamp(v, 0.0, 1.0) * view->height));
            const auto& layout = options.legacyAtlas->layouts[owners[index]];
            const SourceUvChart original { layout.left, layout.top, layout.width, layout.height, false };
            // Base resampling can land just beyond an island's original texel
            // centers. Such pixels cannot seed new collars, even if inside its
            // transformed rectangle; their nearest source sample belongs to a gap.
            if (!contains(original, { (x + .5) / view->width, (y + .5) / view->height }))
                owners[index] = std::numeric_limits<uint32_t>::max();
        }
    };
    auto appendLevels = [&](std::vector<TextureImage>& levels, const TextureImage& base, int kind) {
        levels.reserve(additional);
        for (size_t level = 0; level < additional; ++level) {
            const auto& source = levels.empty() ? base : levels.back();
            auto previousOwners = chartOwnershipForCharts(source.width, chartViews, work, 0, false);
            if (!level)
                admitBaseSamples(previousOwners, source.width, kind);
            levels.push_back(filterMip(source, kind, chartViews, previousOwners, work));
        }
    };
    appendLevels(result.colorLevels, result.base.color, 0);
    appendLevels(result.metallicLevels, result.base.metallic, 1);
    appendLevels(result.roughnessLevels, result.base.roughness, 2);
    appendLevels(result.emissiveLevels, result.base.emissive, 3);
    if (result.base.normal)
        appendLevels(result.normalLevels, *result.base.normal, 4);
    RetainedPayloadBudget retained(maximum);
    accountBase(retained);
    for (const auto* levels : { &result.colorLevels, &result.metallicLevels, &result.roughnessLevels, &result.emissiveLevels, &result.normalLevels }) {
        charge(retained, levels->capacity(), sizeof(TextureImage));
        for (const auto& image : *levels)
            charge(retained, image.pixels.capacity());
    }
    checkCancellation(options.bake);
    result.maximumSamplingLod = additional;
    result.payloadBytes = retained.usedBytes();
    result.rasterSamples = work.samples;
    return result;
}
}

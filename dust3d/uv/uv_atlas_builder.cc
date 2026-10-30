// SPDX-License-Identifier: MIT
// Atlas construction derived from Dust3D, Copyright (c) 2018 Dust3D Project.
#include <algorithm>
#include <cmath>
#include <dust3d/base/object.h>
#include <dust3d/base/retained_payload_budget.h>
#include <dust3d/uv/uv_atlas_builder.h>
#include <limits>

namespace dust3d {
namespace {
    using TriangleKey = std::array<PositionKey, 3>;
    using Uvs = std::array<Vector2, 3>;

    class OutputBudget {
    public:
        explicit OutputBudget(size_t maximum)
            : m_budget(maximum)
        {
        }
        void add(size_t count, size_t width)
        {
            if (!m_budget.tryAdd(count, width))
                throw UvAtlasError(UvAtlasErrorCode::limitExceeded, "The UV atlas output exceeds its configured payload budget.");
        }
        size_t used() const { return m_budget.usedBytes(); }

    private:
        RetainedPayloadBudget m_budget;
    };

    void validate(const Object& object, const std::map<Uuid, UvAtlasComponent>& components)
    {
        const double exclusive = std::ldexp(1.0, std::numeric_limits<long>::digits);
        for (const auto& vertex : object.vertices) {
            for (size_t axis = 0; axis < 3; ++axis) {
                const double scaled = vertex[axis] * 100000;
                if (!std::isfinite(scaled) || scaled < -exclusive || scaled >= exclusive)
                    throw UvAtlasError(UvAtlasErrorCode::invalidInput, "A UV vertex is outside the finite position-key range.");
            }
        }
        for (const auto& triangle : object.triangles) {
            if (triangle.size() != 3 || std::any_of(triangle.begin(), triangle.end(), [&](size_t index) { return index >= object.vertices.size(); }))
                throw UvAtlasError(UvAtlasErrorCode::invalidInput, "A UV triangle has an invalid size or vertex index.");
        }
        for (const auto& component : components) {
            for (size_t channel = 0; channel < 4; ++channel) {
                const auto value = component.second.color[channel];
                if (!std::isfinite(value) || value < 0 || value > 1)
                    throw UvAtlasError(UvAtlasErrorCode::invalidInput, "A UV component color is outside its finite normalized range.");
            }
            if (component.second.image) {
                const auto& image = *component.second.image;
                if (image.id.isNull() || image.width == 0 || image.height == 0 || image.width > 8192 || image.height > 8192)
                    throw UvAtlasError(UvAtlasErrorCode::invalidInput, "A UV source image needs an identity and dimensions from 1 to 8192.");
            }
        }
        for (const auto& component : object.componentTriangleUvs) {
            for (const auto& triangle : component.second) {
                for (const auto& uv : triangle.second) {
                    if (!std::isfinite(uv.x()) || !std::isfinite(uv.y()) || uv.x() < 0 || uv.x() > 1 || uv.y() < 0 || uv.y() > 1)
                        throw UvAtlasError(UvAtlasErrorCode::invalidInput, "A local UV coordinate is outside its finite normalized chart range.");
                }
            }
        }
    }

    TriangleKey key(const Object& object, const std::vector<size_t>& triangle, size_t rotation = 0)
    {
        return { PositionKey(object.vertices[triangle[rotation % 3]]), PositionKey(object.vertices[triangle[(rotation + 1) % 3]]), PositionKey(object.vertices[triangle[(rotation + 2) % 3]]) };
    }

    std::pair<TriangleKey, size_t> canonicalTriangle(const TriangleKey& triangle)
    {
        TriangleKey best = triangle;
        size_t rotation = 0;
        for (size_t candidate = 1; candidate < 3; ++candidate) {
            TriangleKey rotated { triangle[candidate], triangle[(candidate + 1) % 3], triangle[(candidate + 2) % 3] };
            if (rotated < best) {
                best = rotated;
                rotation = candidate;
            }
        }
        return { best, rotation };
    }

    void resolveComponents(const Object& object, UvAtlas& result)
    {
        std::map<TriangleKey, Uuid> owners;
        for (const auto& component : object.componentTriangleUvs) {
            for (const auto& triangle : component.second)
                owners.insert({ triangle.first, component.first });
        }
        for (const auto& broken : object.brokenTrianglesToComponentIdMap)
            owners.insert({ broken.first, broken.second });
        const auto retainedCount = std::min(object.triangleComponentIds.size(), object.triangles.size());
        result.triangleComponentIds.assign(object.triangleComponentIds.begin(), object.triangleComponentIds.begin() + retainedCount);
        result.triangleComponentIds.resize(object.triangles.size());
        if (owners.empty())
            return;
        for (size_t index = 0; index < object.triangles.size(); ++index) {
            for (size_t rotation = 0; rotation < 3 && result.triangleComponentIds[index].isNull(); ++rotation) {
                const auto found = owners.find(key(object, object.triangles[index], rotation));
                if (found != owners.end())
                    result.triangleComponentIds[index] = found->second;
            }
        }
        auto edgeKey = [&](size_t a, size_t b) {
            PositionKey first(object.vertices[a]), second(object.vertices[b]);
            return second < first ? std::make_pair(second, first) : std::make_pair(first, second);
        };
        std::map<std::pair<PositionKey, PositionKey>, std::vector<size_t>> edges;
        for (size_t index = 0; index < object.triangles.size(); ++index) {
            const auto& triangle = object.triangles[index];
            for (size_t corner = 0; corner < 3; ++corner)
                edges[edgeKey(triangle[corner], triangle[(corner + 1) % 3])].push_back(index);
        }
        // Preserve the reference scan/edge priority when neighbouring materials compete.
        for (bool changed = true; changed;) {
            changed = false;
            for (size_t index = 0; index < object.triangles.size(); ++index) {
                if (!result.triangleComponentIds[index].isNull())
                    continue;
                const auto& triangle = object.triangles[index];
                for (size_t corner = 0; corner < 3 && result.triangleComponentIds[index].isNull(); ++corner) {
                    for (const auto neighbour : edges[edgeKey(triangle[corner], triangle[(corner + 1) % 3])]) {
                        if (!result.triangleComponentIds[neighbour].isNull()) {
                            result.triangleComponentIds[index] = result.triangleComponentIds[neighbour];
                            changed = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}

UvAtlas UvAtlasBuilder::build(const Object& object, const std::map<Uuid, UvAtlasComponent>& components, const UvAtlasOptions& options)
{
    constexpr size_t defaultPayload = 64 * 1024 * 1024;
    constexpr size_t maximumPayload = 256 * 1024 * 1024;
    if (options.textureSize == 0 || options.textureSize > 8192)
        throw UvAtlasError(UvAtlasErrorCode::invalidInput, "UV texture scale must be from 1 to 8192.");
    if (options.maximumPayloadBytes > maximumPayload)
        throw UvAtlasError(UvAtlasErrorCode::limitExceeded, "The UV atlas output budget exceeds 256 MiB.");
    OutputBudget budget(options.maximumPayloadBytes == 0 ? defaultPayload : options.maximumPayloadBytes);
    budget.add(object.triangles.size(), sizeof(Uvs) + sizeof(Uuid) + sizeof(uint8_t));
    validate(object, components);
    UvAtlas result;
    UvMapPacker packer;
    packer.setUsePackedChartExtents(options.usePackedChartExtents);
    size_t layoutCount = 0;
    auto addPart = [&](const UvMapPacker::Part& part) {
        budget.add(1, sizeof(UvMapPacker::Layout));
        budget.add(part.localUv.size(), sizeof(decltype(part.localUv)::value_type));
        packer.addPart(part);
        ++layoutCount;
    };

    std::map<PositionKey, Color> vertexColors;
    for (const auto& component : object.componentTriangleUvs) {
        const auto found = components.find(component.first);
        const auto color = found == components.end() ? Color(1, 1, 1) : found->second.color;
        for (const auto& triangle : component.second) {
            for (const auto& position : triangle.first)
                vertexColors.insert({ position, color });
        }
    }
    for (const auto& seam : object.seamTriangleUvs) {
        if (seam.first.empty() && seam.second.empty())
            continue;
        const auto& triangle = seam.first.empty() ? *seam.second.begin() : *seam.first.begin();
        auto colorAt = [&](size_t index) {
            const auto found = vertexColors.find(triangle[index]);
            return found == vertexColors.end() ? Color(200.0 / 255, 200.0 / 255, 200.0 / 255) : found->second;
        };
        budget.add(1, sizeof(UvAtlasGradient));
        result.gradients.push_back({ layoutCount, colorAt(seam.first.empty() ? 2 : 0), colorAt(seam.first.empty() ? 0 : 2), 512 });
        UvMapPacker::Part part;
        part.color = Color(1, 1, 1);
        part.width = part.height = 512;
        const float minimum = 0.5f / 512;
        const float maximum = 1.0f - minimum;
        const float middle = (minimum + maximum) * 0.5f;
        for (const auto& entry : seam.first)
            part.localUv[entry] = { Vector2(minimum, minimum), Vector2(minimum, maximum), Vector2(maximum, middle) };
        for (const auto& entry : seam.second)
            part.localUv[entry] = { Vector2(maximum, minimum), Vector2(maximum, maximum), Vector2(minimum, middle) };
        addPart(part);
    }

    std::map<PositionKey, Vector3> positions;
    for (const auto& position : object.vertices)
        positions.insert({ PositionKey(position), position });
    std::map<Uuid, double> areas;
    double totalArea = 0;
    for (const auto& component : object.componentTriangleUvs) {
        const auto found = components.find(component.first);
        if (found == components.end() || found->second.image)
            continue;
        double area = 0;
        for (const auto& triangle : component.second) {
            auto a = positions.find(triangle.first[0]), b = positions.find(triangle.first[1]), c = positions.find(triangle.first[2]);
            if (a != positions.end() && b != positions.end() && c != positions.end())
                area += Vector3::area(a->second, b->second, c->second);
        }
        areas[component.first] = area;
        totalArea += area;
    }
    const double areaScale = totalArea > 0 ? options.textureSize / std::sqrt(totalArea) : 1;
    for (const auto& component : object.componentTriangleUvs) {
        const auto found = components.find(component.first);
        if (found == components.end())
            continue;
        UvMapPacker::Part part;
        part.sourceId = component.first;
        part.color = found->second.color;
        if (found->second.image) {
            part.id = found->second.image->id;
            part.width = found->second.image->width;
            part.height = found->second.image->height;
        } else {
            part.width = part.height = std::max(1.0, std::sqrt(areas[component.first]) * areaScale);
        }
        part.localUv = component.second;
        addPart(part);
    }
    std::map<Uuid, UvMapPacker::Part> brokenParts;
    for (const auto& broken : object.brokenTrianglesToComponentIdMap)
        brokenParts[broken.second].localUv.insert({ broken.first, Uvs {} });
    for (auto& broken : brokenParts) {
        const auto found = components.find(broken.first);
        if (found == components.end())
            continue;
        auto& part = broken.second;
        part.sourceId = broken.first;
        part.color = found->second.color;
        part.width = part.height = 1;
        addPart(part);
    }
    packer.pack();
    const auto& layouts = packer.packedLayouts();
    if (layouts.size() != layoutCount)
        throw UvAtlasError(UvAtlasErrorCode::packingFailed, "The atlas packer did not return every chart.");
    result.packedTextureSize = packer.packedTextureSize();
    if (!std::isfinite(result.packedTextureSize) || (!layouts.empty() && result.packedTextureSize <= 0))
        throw UvAtlasError(UvAtlasErrorCode::packingFailed, "The atlas packer returned an invalid texture size.");
    std::map<TriangleKey, Uvs> merged;
    for (const auto& layout : layouts) {
        if (!std::isfinite(layout.left) || !std::isfinite(layout.top) || !std::isfinite(layout.width) || !std::isfinite(layout.height))
            throw UvAtlasError(UvAtlasErrorCode::packingFailed, "The atlas packer returned nonfinite layout bounds.");
        for (const auto& triangle : layout.globalUv) {
            for (const auto& uv : triangle.second) {
                if (!std::isfinite(uv.x()) || !std::isfinite(uv.y()))
                    throw UvAtlasError(UvAtlasErrorCode::packingFailed, "The atlas packer returned a nonfinite UV coordinate.");
            }
            // Equivalent cyclic keys share chart priority, while winding remains distinct.
            const auto [canonical, rotation] = canonicalTriangle(triangle.first);
            Uvs uvs;
            for (size_t corner = 0; corner < 3; ++corner)
                uvs[corner] = triangle.second[(corner + rotation) % 3];
            merged.insert({ canonical, uvs });
        }
    }
    result.layouts = layouts;
    result.triangleUvs.resize(object.triangles.size());
    result.triangleUvKnown.resize(object.triangles.size(), 0);
    for (size_t index = 0; index < object.triangles.size(); ++index) {
        const auto [canonical, rotation] = canonicalTriangle(key(object, object.triangles[index]));
        const auto found = merged.find(canonical);
        if (found != merged.end()) {
            for (size_t corner = 0; corner < 3; ++corner)
                result.triangleUvs[index][(corner + rotation) % 3] = found->second[corner];
            result.triangleUvKnown[index] = 1;
        }
    }
    resolveComponents(object, result);
    result.payloadBytes = budget.used();
    return result;
}
}

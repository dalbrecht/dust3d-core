// SPDX-License-Identifier: MIT
#include "image_forever.h"
#include "uv_map_generator.h"
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTransform>
#include <algorithm>
#include <cmath>
#include <dust3d/uv/cloth_folds.h>
#include <dust3d/uv/texture_baker.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/GenerationPipeline.h>
#include <stdexcept>

namespace {
const std::string firstComponent = "{cccccccc-cccc-4ccc-8ccc-cccccccccccc}";
const std::string secondComponent = "{dddddddd-dddd-4ddd-8ddd-dddddddddddd}";

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct ReferenceInput {
    std::unique_ptr<dust3d::Object> object = std::make_unique<dust3d::Object>();
    std::unique_ptr<dust3d::Snapshot> snapshot = std::make_unique<dust3d::Snapshot>();
};

ReferenceInput makeSquareInput()
{
    ReferenceInput input;
    auto& object = *input.object;
    for (size_t square = 0; square < 2; ++square) {
        const double x = square * 2;
        const auto base = object.vertices.size();
        object.vertices.insert(object.vertices.end(), { { x, 0, 0 }, { x + 1, 0, 0 }, { x + 1, 1, 0 }, { x, 1, 0 } });
        const auto identity = square == 0 ? firstComponent : secondComponent;
        for (const auto& triangle : { std::array<size_t, 3> { base, base + 1, base + 2 }, std::array<size_t, 3> { base, base + 2, base + 3 } }) {
            object.triangles.push_back({ triangle[0], triangle[1], triangle[2] });
            std::array<dust3d::PositionKey, 3> key { dust3d::PositionKey(object.vertices[triangle[0]]), dust3d::PositionKey(object.vertices[triangle[1]]), dust3d::PositionKey(object.vertices[triangle[2]]) };
            std::array<dust3d::Vector2, 3> uv;
            for (size_t corner = 0; corner < 3; ++corner) {
                const auto& vertex = object.vertices[triangle[corner]];
                uv[corner] = { vertex.x() - x, vertex.y() };
            }
            object.componentTriangleUvs[dust3d::Uuid(identity)][key] = uv;
        }
        input.snapshot->components[identity]["color"] = square == 0 ? "#3366CC" : "#8066CC33";
    }
    object.vertexColors.resize(object.vertices.size(), dust3d::Color(1, 1, 1));
    object.triangleAndQuads = object.triangles;
    return input;
}

struct ReferenceBakeResult {
    std::unique_ptr<dust3d::Object> object;
    std::unique_ptr<QImage> color;
    std::unique_ptr<QImage> normal;
    std::unique_ptr<QImage> metallic;
    std::unique_ptr<QImage> roughness;
    std::unique_ptr<QImage> emissive;
};

bool comparePortableBakes = false;

std::pair<dust3d::UvAtlas, dust3d::TextureBakeResult> portableBake(const ReferenceInput& input)
{
    std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
    dust3d::TextureBakeInputs resolved;
    std::map<dust3d::Uuid, QImage> decoded;
    auto value = [](const std::map<std::string, std::string>& fields, const std::string& key) {
        const auto found = fields.find(key);
        return found == fields.end() ? std::string() : found->second;
    };
    auto color = [](const std::string& text) {
        const QColor parsed(QString::fromStdString(text));
        require(parsed.isValid(), "Comparison fixture color must parse");
        return dust3d::Color(parsed.red() / 255.0, parsed.green() / 255.0, parsed.blue() / 255.0, parsed.alpha() / 255.0);
    };
    for (const auto& entry : input.snapshot->components) {
        const dust3d::Uuid id(entry.first);
        auto& component = components[id];
        const auto text = value(entry.second, "color");
        if (!text.empty())
            component.color = color(text);
        const auto imageText = value(entry.second, "colorImageId");
        if (!imageText.empty()) {
            const dust3d::Uuid imageId(imageText);
            const auto source = ImageForever::get(imageId);
            require(source && !source->isNull(), "Comparison image must be registered");
            decoded[imageId] = source->convertToFormat(QImage::Format_RGBA8888);
            const auto& image = decoded.at(imageId);
            component.image = dust3d::UvAtlasImage { imageId, static_cast<size_t>(image.width()), static_cast<size_t>(image.height()) };
            resolved.images[imageId] = { static_cast<size_t>(image.width()), static_cast<size_t>(image.height()), static_cast<size_t>(image.bytesPerLine()), static_cast<size_t>(image.sizeInBytes()), image.constBits() };
        }
        const auto part = input.snapshot->parts.find(value(entry.second, "linkData"));
        if (value(entry.second, "linkDataType") == "partId" && part != input.snapshot->parts.end()) {
            auto scalar = [&](const char* key, double fallback) {
                const auto text = value(part->second, key);
                return text.empty() ? fallback : std::stod(text);
            };
            resolved.materials[id] = { scalar("metallic", 0), scalar("roughness", 1), scalar("emissive", 0) };
        }
        if (!value(entry.second, "wrap").empty()) {
            auto& surface = resolved.surfaces[id];
            const auto type = dust3d::SurfacePattern::typeFromString(value(entry.second, "wrapPattern"));
            if (type != dust3d::SurfacePattern::Type::None) {
                dust3d::SurfacePattern::Settings settings;
                settings.type = type;
                settings.baseColor = component.color;
                const auto ink = value(entry.second, "wrapPatternColor");
                settings.patternColor = ink.empty() ? dust3d::SurfacePattern::defaultPatternColor(component.color) : color(ink);
                const auto scale = value(entry.second, "wrapPatternScale");
                settings.scale = scale.empty() ? 0.06 : std::stod(scale);
                const auto belly = value(entry.second, "wrapBelly");
                settings.belly = belly.empty() ? 0 : std::stod(belly);
                settings.seed = dust3d::SurfacePattern::seedFromString(entry.first);
                surface.pattern = settings;
            }
            const auto strength = value(entry.second, "wrapWrinkles");
            surface.wrinkleStrength = strength.empty() ? 0 : std::stod(strength);
            surface.folds = dust3d::ClothFolds::deserialize(value(entry.second, "__wrapFolds"));
        }
    }
    auto atlas = dust3d::UvAtlasBuilder::build(*input.object, components);
    auto result = dust3d::TextureBaker::bake(*input.object, atlas, resolved);
    return { std::move(atlas), std::move(result) };
}

void comparePortableMap(const ReferenceBakeResult& reference, const QImage& expected, const dust3d::TextureImage& actual, const char* label)
{
    require(actual.width == static_cast<size_t>(expected.width()) && actual.height == static_cast<size_t>(expected.height()), "Comparison map dimensions must match");
    size_t samples = 0;
    int maximumError = 0;
    for (const auto& uvs : *reference.object->triangleVertexUvs()) {
        for (int row = 1; row < 8; ++row) {
            for (int column = 1; column < 8 - row; ++column) {
                const double weights[3] { row / 8.0, column / 8.0, 1 - (row + column) / 8.0 };
                double u = 0, v = 0;
                for (size_t corner = 0; corner < 3; ++corner) {
                    u += uvs[corner].x() * weights[corner];
                    v += uvs[corner].y() * weights[corner];
                }
                const auto x = std::min(actual.width - 1, static_cast<size_t>(u * actual.width));
                const auto y = std::min(actual.height - 1, static_cast<size_t>(v * actual.height));
                const auto expectedPixel = expected.pixelColor(static_cast<int>(x), static_cast<int>(y));
                const int channels[4] { expectedPixel.red(), expectedPixel.green(), expectedPixel.blue(), expectedPixel.alpha() };
                for (size_t channel = 0; channel < 4; ++channel) {
                    if (channel < 3 && channels[3] == 0)
                        continue; // RGB beneath zero alpha is unobservable and differs by storage policy.
                    const auto error = std::abs(channels[channel] - actual.pixels[(y * actual.width + x) * 4 + channel]);
                    if (error > 2 && maximumError <= 2) {
                        std::cerr << "first mismatch " << label << " xy " << x << "," << y << " uv " << u << "," << v << " channel " << channel << " reference " << channels[channel] << " portable " << int(actual.pixels[(y * actual.width + x) * 4 + channel]) << "\n";
                    }
                    maximumError = std::max(maximumError, error);
                }
                ++samples;
            }
        }
    }
    std::cerr << "portable " << label << ": " << samples << " interior probes, maximum byte error " << maximumError << "\n";
    require(maximumError <= 2, std::string("Portable interior map differs beyond two-byte rounding tolerance: ") + label);
}

ReferenceBakeResult bake(ReferenceInput input)
{
    std::optional<std::pair<dust3d::UvAtlas, dust3d::TextureBakeResult>> portable;
    if (comparePortableBakes)
        portable = portableBake(input);
    UvMapGenerator generator(std::move(input.object), std::move(input.snapshot));
    generator.generate();
    ReferenceBakeResult result { generator.takeObject(), generator.takeResultTextureColorImage(), generator.takeResultTextureNormalImage(), generator.takeResultTextureMetalnessImage(), generator.takeResultTextureRoughnessImage(), generator.takeResultTextureEmissiveImage() };
    require(result.object && result.color && !result.color->isNull(), "Reference generation must produce an object and color image");
    require(result.color->width() == 4096 && result.color->height() == 4096, "Record the reference's fixed color atlas size explicitly");
    require(result.object->triangleVertexUvs() && result.object->triangleVertexUvs()->size() == result.object->triangles.size(), "Reference UV array must align with original object triangles");
    if (portable) {
        double maximumUvError = 0;
        const auto& expectedUvs = *result.object->triangleVertexUvs();
        require(portable->first.triangleUvs.size() == expectedUvs.size(), "Comparison triangle UV counts must agree");
        for (size_t index = 0; index < expectedUvs.size(); ++index) {
            for (size_t corner = 0; corner < 3; ++corner) {
                for (size_t axis = 0; axis < 2; ++axis)
                    maximumUvError = std::max(maximumUvError, std::abs(expectedUvs[index][corner][axis] - portable->first.triangleUvs[index][corner][axis]));
            }
        }
        require(maximumUvError <= 1e-10, "Portable/reference comparison requires matching original corner UVs");
        comparePortableMap(result, *result.color, portable->second.color, "color");
        const std::array<const QImage*, 4> expected { result.normal.get(), result.metallic.get(), result.roughness.get(), result.emissive.get() };
        const std::array<const std::optional<dust3d::TextureImage>*, 4> actual { &portable->second.normal, &portable->second.metallic, &portable->second.roughness, &portable->second.emissive };
        const std::array<const char*, 4> names { "normal", "metallic", "roughness", "emissive" };
        for (size_t index = 0; index < expected.size(); ++index) {
            require(bool(expected[index]) == actual[index]->has_value(), "Reference and portable optional map activation must agree");
            if (expected[index])
                comparePortableMap(result, *expected[index], **actual[index], names[index]);
        }
    }
    return result;
}

QColor sampleTriangle(const ReferenceBakeResult& result, const QImage& image, size_t triangle, const std::array<double, 3>& weights)
{
    const auto* uvs = result.object->triangleVertexUvs();
    require(uvs && triangle < uvs->size(), "Probe triangle must have reference UVs");
    double u = 0, v = 0;
    for (size_t corner = 0; corner < 3; ++corner) {
        u += (*uvs)[triangle][corner].x() * weights[corner];
        v += (*uvs)[triangle][corner].y() * weights[corner];
    }
    require(std::isfinite(u) && std::isfinite(v) && u >= 0 && v >= 0 && u <= 1 && v <= 1, "Probe coordinates must remain inside the atlas");
    const int x = std::min(image.width() - 1, static_cast<int>(u * image.width()));
    const int y = std::min(image.height() - 1, static_cast<int>(v * image.height()));
    return image.pixelColor(x, y);
}

QColor sampleSquare(const ReferenceBakeResult& result, const QImage& image, size_t square, double u, double v)
{
    if (v <= u)
        return sampleTriangle(result, image, square * 2, { 1 - u, u - v, v });
    return sampleTriangle(result, image, square * 2 + 1, { 1 - v, u, v - u });
}

QJsonArray encodeColorChannels(const QColor& color) { return { color.red(), color.green(), color.blue(), color.alpha() }; }

void requireNearColor(const QColor& actual, const QColor& expected, int tolerance, const char* message)
{
    require(std::abs(actual.red() - expected.red()) <= tolerance && std::abs(actual.green() - expected.green()) <= tolerance && std::abs(actual.blue() - expected.blue()) <= tolerance && std::abs(actual.alpha() - expected.alpha()) <= tolerance, message);
}

void saveDiagnostics(const ReferenceBakeResult& result, const QString& directory, const QString& name)
{
    if (directory.isEmpty())
        return;
    require(QDir().mkpath(directory), "Cannot create explicitly requested reference output directory");
    require(result.color->save(QDir(directory).filePath(name + "-color.png")), "Cannot save requested color diagnostic");
}

QJsonObject probeSolidCharts(const QString& output)
{
    auto result = bake(makeSquareInput());
    const auto opaque = sampleSquare(result, *result.color, 0, .4, .3);
    const auto partial = sampleSquare(result, *result.color, 1, .4, .3);
    requireNearColor(opaque, QColor(51, 102, 204), 0, "Opaque solid chart must preserve source channels");
    requireNearColor(partial, QColor(102, 204, 51, 128), 2, "Partial-alpha chart must retain source color within premultiplied byte rounding");
    require(!result.normal && !result.metallic && !result.roughness && !result.emissive, "Plain charts must not invent optional maps");
    saveDiagnostics(result, output, "solid");
    return { { "opaque_rgba", encodeColorChannels(opaque) }, { "partial_rgba", encodeColorChannels(partial) } };
}

class RegisteredImages {
public:
    RegisteredImages() = default;
    RegisteredImages(const RegisteredImages&) = delete;
    RegisteredImages& operator=(const RegisteredImages&) = delete;
    ~RegisteredImages()
    {
        for (const auto& identity : m_identities)
            ImageForever::remove(identity);
    }
    void add(const QImage& image, const dust3d::Uuid& identity)
    {
        require(ImageForever::add(&image, identity) == identity, "Reference image registration must preserve explicit identity");
        m_identities.push_back(identity);
    }

private:
    std::vector<dust3d::Uuid> m_identities;
};

void verifyFlippedPixmapPhase()
{
    QImage source(512, 1, QImage::Format_ARGB32);
    for (int x = 0; x < 512; ++x) {
        const double amount = static_cast<double>(x) / 511;
        source.setPixelColor(x, 0, QColor(static_cast<int>(255 * (1 - amount)), 0, static_cast<int>(255 * amount), 255));
    }
    const auto scaled = source.scaled(QSize(2, 2));
    const auto center = scaled.rect().center();
    QTransform matrix;
    matrix.translate(center.x(), center.y());
    matrix.rotate(90);
    const auto rotated = scaled.transformed(matrix).mirrored(true, false);
    QImage output(32, 32, QImage::Format_ARGB32);
    output.fill(Qt::white);
    QPainter painter(&output);
    // Preserve the inherited overload selection: two scalar coordinates select
    // drawPixmap(int, int, ...), so fractional positions are truncated.
    const double origin = 10.4;
    painter.drawPixmap(origin, origin, QPixmap::fromImage(rotated));
    painter.end();
    const auto pixel = output.pixelColor(11, 11);
    require(pixel.red() == 63 && pixel.blue() == 191, "Reference rotated pixmap must retain its independently measured pixel-center phase");
}

QJsonObject probeImageCharts(const QString& output, bool translucent = false)
{
    verifyFlippedPixmapPhase();
    auto input = makeSquareInput();
    const std::array<QColor, 6> palette { Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::yellow, Qt::magenta };
    const std::array<std::string, 2> imageIds { "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}", "{ffffffff-ffff-4fff-8fff-ffffffffffff}" };
    RegisteredImages registry;
    std::array<QImage, 2> sources { QImage(2, 3, QImage::Format_ARGB32), QImage(3, 2, QImage::Format_ARGB32) };
    for (size_t square = 0; square < 2; ++square) {
        auto& source = sources[square];
        require(!source.isNull(), "Analytic reference source image must allocate");
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                auto ink = palette[static_cast<size_t>(y * source.width() + x)];
                if (translucent)
                    ink.setAlpha(128);
                source.setPixelColor(x, y, ink);
            }
        }
        registry.add(source, dust3d::Uuid(imageIds[square]));
        auto& component = input.snapshot->components[square == 0 ? firstComponent : secondComponent];
        component["colorImageId"] = imageIds[square];
        component["color"] = "#101010";
    }
    std::optional<std::pair<dust3d::UvAtlas, dust3d::TextureBakeResult>> portable;
    if (translucent)
        portable = portableBake(input);
    auto result = bake(std::move(input));
    QJsonArray samples;
    size_t flipped = 0;
    for (size_t square = 0; square < 2; ++square) {
        const auto& uv = (*result.object->triangleVertexUvs())[square * 2];
        flipped += std::abs(uv[1].x() - uv[0].x()) < std::abs(uv[1].y() - uv[0].y());
        const auto& source = sources[square];
        for (int y = 0; y < source.height(); ++y) {
            for (int x = 0; x < source.width(); ++x) {
                const auto actual = sampleSquare(result, *result.color, square, (x + .5) / source.width(), (y + .5) / source.height());
                if (!translucent)
                    requireNearColor(actual, source.pixelColor(x, y), 0, "Image centers must preserve orientation and ignore solid color modulation");
                samples.append(encodeColorChannels(actual));
            }
        }
    }
    require(flipped > 0, "Asymmetric image fixture must actually exercise reference UV-axis transposition");
    saveDiagnostics(result, output, "image");
    if (portable) {
        for (size_t offset = 3; offset < portable->second.color.pixels.size(); offset += 4)
            require(portable->second.color.pixels[offset] == 128, "Portable image sampling must preserve source opacity without double compositing");
        return { { "flipped_charts", static_cast<qint64>(flipped) }, { "source_alpha", 128 }, { "portable_alpha", 128 }, { "reference_centers", samples } };
    }
    return { { "flipped_charts", static_cast<qint64>(flipped) }, { "source_pixel_centers", samples } };
}

QJsonObject probeGradientChart(const QString& output)
{
    auto input = makeSquareInput();
    input.snapshot->components[firstComponent]["color"] = "#FF0000";
    input.snapshot->components[secondComponent]["color"] = "#0000FF";
    const std::array<size_t, 3> indices { 0, 1, 7 };
    input.object->triangles.push_back({ indices[0], indices[1], indices[2] });
    std::array<dust3d::PositionKey, 3> key { dust3d::PositionKey(input.object->vertices[indices[0]]), dust3d::PositionKey(input.object->vertices[indices[1]]), dust3d::PositionKey(input.object->vertices[indices[2]]) };
    input.object->seamTriangleUvs.push_back({ { key }, {} });
    auto result = bake(std::move(input));
    const auto large = sampleTriangle(result, *result.color, 4, { .45, .45, .1 });
    const auto small = sampleTriangle(result, *result.color, 4, { .05, .05, .9 });
    require(large.red() > large.blue() && small.blue() > small.red(), "Seam source interpolation must retain large/small endpoint direction");
    requireNearColor(QColor(large.red() + large.blue(), large.green(), 0, large.alpha()), QColor(254, 0, 0, 255), 2, "Reference gradient must retain encoded red/blue interpolation");
    saveDiagnostics(result, output, "gradient");
    return { { "large_side", encodeColorChannels(large) }, { "small_side", encodeColorChannels(small) } };
}

QJsonObject probeMaterialMaps(const QString& output)
{
    auto input = makeSquareInput();
    const std::array<std::string, 2> parts { "{11111111-1111-4111-8111-111111111111}", "{22222222-2222-4222-8222-222222222222}" };
    for (size_t square = 0; square < 2; ++square) {
        auto& component = input.snapshot->components[square == 0 ? firstComponent : secondComponent];
        component["linkDataType"] = "partId";
        component["linkData"] = parts[square];
        component["color"] = "#FF8000";
        input.snapshot->parts[parts[square]] = {};
    }
    auto& part = input.snapshot->parts[parts[0]];
    part["metallic"] = ".5";
    part["roughness"] = ".25";
    part["emissive"] = "2";
    auto result = bake(std::move(input));
    require(result.metallic && result.roughness && result.emissive, "Configured material must create actual optional images");
    require(result.metallic->width() == 1024 && result.roughness->width() == 1024, "Record fixed scalar map dimensions");
    const auto metal = sampleSquare(result, *result.metallic, 0, .4, .3);
    const auto rough = sampleSquare(result, *result.roughness, 0, .4, .3);
    const auto emission = sampleSquare(result, *result.emissive, 0, .4, .3);
    requireNearColor(metal, QColor(128, 128, 128), 0, "Metallic must preserve scalar lround conversion");
    requireNearColor(rough, QColor(64, 64, 64), 0, "Roughness must preserve scalar lround conversion");
    requireNearColor(emission, QColor(255, 128, 0), 0, "Reference emission clips strength above one");
    requireNearColor(sampleSquare(result, *result.metallic, 1, .4, .3), QColor(0, 0, 0), 0, "Unconfigured metallic uses zero");
    requireNearColor(sampleSquare(result, *result.roughness, 1, .4, .3), QColor(255, 255, 255), 0, "Unconfigured roughness uses one");
    saveDiagnostics(result, output, "material");
    return { { "metallic_rgba", encodeColorChannels(metal) }, { "roughness_rgba", encodeColorChannels(rough) }, { "emissive_strength_2_rgba", encodeColorChannels(emission) } };
}

QJsonObject probeProceduralMaps(const QString& output)
{
    auto input = makeSquareInput();
    auto& component = input.snapshot->components[firstComponent];
    component["color"] = "#FFFFFF";
    component["wrap"] = "Cloth";
    component["wrapPattern"] = "Spots";
    component["wrapPatternColor"] = "#000000";
    component["wrapPatternScale"] = ".2";
    component["wrapWrinkles"] = "1";
    dust3d::ClothFolds::Fold fold;
    fold.kind = dust3d::ClothFolds::Kind::Tension;
    fold.points = { { .5, .1, 0 }, { .5, .9, 0 } };
    fold.normals = { { 0, 0, 1 }, { 0, 0, 1 } };
    fold.width = .12;
    fold.depth = .05;
    component["__wrapFolds"] = dust3d::ClothFolds::serialize({ fold });
    // A valid partial chart keeps the fixed4096 reference bake inexpensive while
    // exercising world-space pattern/fold evaluation at actual covered texels.
    for (auto& triangle : input.object->componentTriangleUvs[dust3d::Uuid(firstComponent)]) {
        for (auto& uv : triangle.second) {
            uv[0] *= .1;
            uv[1] *= .1;
        }
    }
    auto result = bake(std::move(input));
    require(result.normal && result.normal->width() == 2048, "Serialized folds must produce a real half-resolution normal atlas");
    int darkest = 255, lightest = 0;
    size_t nonflat = 0;
    for (size_t y = 0; y < 16; ++y) {
        for (size_t x = 0; x < 16; ++x) {
            const double u = (x + .5) / 16;
            const double v = (y + .5) / 16;
            const auto color = sampleSquare(result, *result.color, 0, u, v);
            darkest = std::min(darkest, color.red());
            lightest = std::max(lightest, color.red());
            const auto normal = sampleSquare(result, *result.normal, 0, u, v);
            require(normal.alpha() == 255 && normal.blue() > 0, "Procedural normals retain valid encoded hemisphere data");
            nonflat += std::abs(normal.red() - 128) > 2 || std::abs(normal.green() - 128) > 2;
        }
    }
    require(lightest - darkest > 64, "Spots must paint nonuniform world-space colors rather than a solid placeholder");
    require(nonflat > 0, "Fold normals must contain actual surface perturbation");
    requireNearColor(sampleSquare(result, *result.normal, 1, .4, .3), QColor(128, 128, 255), 0, "Other charts retain flat tangent-space normals");
    const auto* tangents = result.object->triangleTangents();
    require(tangents && tangents->size() == result.object->triangles.size(), "Reference normal baking must also resolve triangle tangents");
    for (const auto& tangent : *tangents)
        require(std::isfinite(tangent.lengthSquared()) && tangent.lengthSquared() > 0, "Resolved reference tangents must be finite and nonzero");
    saveDiagnostics(result, output, "procedural");
    if (!output.isEmpty())
        require(result.normal->save(QDir(output).filePath("procedural-normal.png")), "Cannot save requested normal diagnostic");
    return { { "sample_red_range", QJsonArray { darkest, lightest } }, { "nonflat_normal_probes", static_cast<qint64>(nonflat) }, { "tangent_count", static_cast<qint64>(tangents->size()) } };
}

QJsonObject probeTransparentChart(const QString& output)
{
    auto input = makeSquareInput();
    input.snapshot->components[firstComponent]["color"] = "#0000FF00";
    input.snapshot->components[secondComponent]["color"] = "#FF0000";
    auto result = bake(std::move(input));
    const auto center = sampleSquare(result, *result.color, 0, .4, .3);
    saveDiagnostics(result, output, "transparent");
    // This is an observation, not an assertion that sentinel-based alpha loss is correct.
    return { { "requested_rgba", QJsonArray { 0, 255, 0, 0 } }, { "observed_rgba", encodeColorChannels(center) }, { "transparent_chart_overwritten", center.alpha() != 0 } };
}

std::vector<uint8_t> readFixture(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    require(stream.good(), "Reference corpus fixture is missing");
    const auto length = stream.tellg();
    require(length >= 0 && length <= 256 * 1024 * 1024, "Reference fixture exceeds the 256 MiB input limit");
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    stream.seekg(0);
    if (!bytes.empty())
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(stream.good() && stream.peek() == std::ifstream::traits_type::eof(), "Reference fixture changed while being read");
    return bytes;
}

QJsonObject probeInputBounds()
{
    QTemporaryDir directory;
    require(directory.isValid(), "Reference input test needs an isolated temporary directory");
    const auto path = QDir(directory.path()).filePath("oversized.ds3").toStdString();
    {
        std::ofstream stream(path, std::ios::binary);
        stream.seekp(256 * 1024 * 1024);
        stream.put(0);
        require(stream.good(), "Cannot create sparse oversized reference input");
    }
    bool rejected = false;
    try {
        readFixture(path);
    } catch (const std::runtime_error& error) {
        rejected = std::string(error.what()).find("256 MiB") != std::string::npos;
    }
    require(rejected, "Oversized reference input must reject before allocation");
    return { { "oversized_input_rejected_before_allocation", true } };
}

QJsonObject probeCorpusUvs(const std::string& directory, const QString& output)
{
    QJsonArray models;
    std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
    for (const std::string name : { "Capsule", "Sword", "RoundedCrossing" }) {
        const auto bytes = readFixture(directory + "/" + name + ".ds3");
        std::string warning;
        auto snapshot = dust3d::native::readDocument(bytes.data(), bytes.size(), warning);
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.generate();
        ReferenceInput input;
        input.object.reset(generator.takeObject());
        input.snapshot.reset(generator.takeSnapshot());
        require(generator.isSuccessful() && input.object && input.snapshot, "Reference corpus needs valid generated owners");
        std::map<dust3d::Uuid, dust3d::UvAtlasComponent> components;
        for (const auto& entry : input.snapshot->components) {
            require(dust3d::String::valueOrEmpty(entry.second, "colorImageId").empty(), "The corpus probe requires image-free inputs; asset decoding is outside this comparison");
            auto& component = components[dust3d::Uuid(entry.first)];
            const auto color = entry.second.find("color");
            if (color != entry.second.end())
                component.color = dust3d::Color(color->second);
        }
        const auto portable = dust3d::UvAtlasBuilder::build(*input.object, components);
        auto result = bake(std::move(input));
        require(portable.triangleUvs.size() == result.object->triangles.size() && portable.triangleUvKnown.size() == result.object->triangles.size(), "Portable differential records must remain object-triangle aligned");
        size_t zeros = 0;
        size_t triangleIndex = 0;
        double maximumUvError = 0;
        for (const auto& triangle : *result.object->triangleVertexUvs()) {
            require(triangle.size() == 3, "Reference differential UV records must have three corners");
            bool isZero = true;
            for (const auto& uv : triangle)
                isZero = isZero && uv.x() == 0 && uv.y() == 0;
            zeros += isZero;
            require((portable.triangleUvKnown[triangleIndex] != 0) == !isZero, "Portable known markers and reference zero records diverge on this corpus");
            for (size_t corner = 0; corner < 3; ++corner) {
                require(std::isfinite(triangle[corner].x()) && std::isfinite(triangle[corner].y()) && std::isfinite(portable.triangleUvs[triangleIndex][corner].x()) && std::isfinite(portable.triangleUvs[triangleIndex][corner].y()), "Nonfinite UVs cannot pass a numerical differential comparison");
                maximumUvError = std::max(maximumUvError, std::abs(portable.triangleUvs[triangleIndex][corner].x() - triangle[corner].x()));
                maximumUvError = std::max(maximumUvError, std::abs(portable.triangleUvs[triangleIndex][corner].y() - triangle[corner].y()));
            }
            ++triangleIndex;
        }
        require(maximumUvError <= 1e-10, "Portable original-object corner UVs diverge from the preserved Qt orchestration");
        models.append(QJsonObject { { "name", QString::fromStdString(name) }, { "triangles", static_cast<qint64>(result.object->triangles.size()) }, { "zero_uv_records", static_cast<qint64>(zeros) }, { "maximum_portable_uv_error", maximumUvError } });
        saveDiagnostics(result, output, QString::fromStdString(name));
    }
    return { { "models", models } };
}
}

int main(int argc, char* argv[])
{
    try {
        require(argc == 3 || argc == 4, "Usage: reference-probe SCENARIO FIXTURE_DIRECTORY [OUTPUT_DIRECTORY]");
        require(qputenv("QT_QPA_PLATFORM", "offscreen"), "Cannot select the offscreen reference platform");
        // Qt must not consume a caller's platform flags before our CLI validates them.
        int platformArgumentCount = 1;
        char* platformArguments[] { argv[0], nullptr };
        QGuiApplication application(platformArgumentCount, platformArguments);
        std::string scenario(argv[1]);
        if (scenario.rfind("portable-", 0) == 0) {
            comparePortableBakes = true;
            scenario.erase(0, 9);
        }
        const QString output = argc == 4 ? QString::fromUtf8(argv[3]) : QString();
        QJsonObject observations;
        if (scenario == "solid")
            observations = probeSolidCharts(output);
        else if (scenario == "image")
            observations = probeImageCharts(output);
        else if (scenario == "image-alpha")
            observations = probeImageCharts(output, true);
        else if (scenario == "gradient")
            observations = probeGradientChart(output);
        else if (scenario == "material")
            observations = probeMaterialMaps(output);
        else if (scenario == "transparent")
            observations = probeTransparentChart(output);
        else if (scenario == "procedural")
            observations = probeProceduralMaps(output);
        else if (scenario == "corpus")
            observations = probeCorpusUvs(argv[2], output);
        else if (scenario == "input-bounds")
            observations = probeInputBounds();
        else
            throw std::runtime_error("Unknown reference scenario");
        observations["scenario"] = QString::fromStdString(scenario);
        observations["qt_version"] = QT_VERSION_STR;
        observations["qt_runtime_version"] = qVersion();
        observations["reference_only"] = true;
        std::cout << QJsonDocument(observations).toJson(QJsonDocument::Compact).constData() << '\n';
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}

#include <Dust3DBridge.h>
#include <cmath>
#include <cstring>
#include <dust3d/base/object.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/GenerationPipeline.h>
#include <native/Engine/UvMetadata.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Owner = std::unique_ptr<Dust3DUvMesh, decltype(&dust3d_uv_mesh_destroy)>;
void require(bool value, const std::string& message)
{
    if (!value)
        throw std::runtime_error(message);
}
std::vector<uint8_t> read(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    require(stream.good(), "Missing UV boundary fixture");
    return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}
void nullsAndOptions()
{
    Dust3DError error {};
    Dust3DUvOptions options { sizeof(options), 0, 0, nullptr, 0 };
    require(!dust3d_uv_mesh_load(nullptr, 0, nullptr, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "UV options must be explicit");
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "UV input must be a valid document");
    require(!dust3d_uv_mesh_geometry(nullptr) && !dust3d_uv_mesh_triangles(nullptr) && !dust3d_uv_mesh_charts(nullptr) && !dust3d_uv_mesh_gradients(nullptr), "Null UV owners have no borrowed arrays");
    require(dust3d_uv_mesh_triangle_count(nullptr) == 0 && dust3d_uv_mesh_chart_count(nullptr) == 0 && dust3d_uv_mesh_gradient_count(nullptr) == 0 && dust3d_uv_mesh_component_count(nullptr) == 0 && dust3d_uv_mesh_payload_bytes(nullptr) == 0 && dust3d_uv_mesh_texture_size(nullptr) == 0, "Null UV owner counts must be zero");
    require(std::strcmp(dust3d_uv_mesh_component_id(nullptr, 0), "") == 0, "Null UV source strings must be empty");
    dust3d_uv_mesh_destroy(nullptr);
    options.struct_size = 0;
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid options size must reject");
    options.struct_size = sizeof(options);
    options.texture_size = 8193;
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Oversized texture dimensions must reject");
    options.texture_size = 0;
    options.maximum_payload_bytes = DUST3D_MAX_UV_PAYLOAD_BYTES + size_t(1);
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_LIMIT_EXCEEDED, "Oversized UV budget must reject");
    options.maximum_payload_bytes = 0;
    options.image_count = 1;
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, &error) && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Image descriptors need a valid array");
}

const std::string component = "{zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz}";
const std::string imageKey = "{iiiiiiii-iiii-iiii-iiii-iiiiiiiiiiii}";

template <typename Action>
void rejects(Action action, Dust3DStatus status)
{
    try {
        action();
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == status && *error.what(), "Metadata errors must retain their classification/cause");
        return;
    }
    throw std::runtime_error("Invalid UV metadata unexpectedly succeeded");
}

void analyticIdentitiesAndVisuals()
{
    dust3d::Snapshot source;
    source.components[component]["color"] = "#80Aa00fF";
    dust3d::Object object;
    object.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    object.triangles = { { 0, 1, 2 } };
    object.componentTriangleUvs[dust3d::Uuid(component)][{ dust3d::PositionKey(object.vertices[0]), dust3d::PositionKey(object.vertices[1]), dust3d::PositionKey(object.vertices[2]) }] = { dust3d::Vector2(0, 0), dust3d::Vector2(1, 0), dust3d::Vector2(0, 1) };
    object.triangleComponentIds = { dust3d::Uuid(component) };
    dust3d::native::UvMetadata metadata(source, 1024 * 1024);
    metadata.prepare(object, source, {}, 4096);
    metadata.appendTriangle(0);
    require(metadata.components()[0] == component && metadata.triangles()[0].component_kind == DUST3D_UV_COMPONENT && metadata.triangles()[0].component_index == 0, "Original opaque component keys remain owned and associated");
    const auto& color = metadata.charts()[0].color;
    require(color[0] == 170.0 / 255 && color[1] == 0 && color[2] == 1 && color[3] == 128.0 / 255, "Strict mixed-case channels preserve alpha-first source colors");
    rejects([&] { metadata.appendTriangle(SIZE_MAX); }, DUST3D_STATUS_GENERATION_FAILED);
    source.components[component.substr(1, 36)] = source.components[component];
    dust3d::native::UvMetadata aliases(source, 1024 * 1024);
    aliases.prepare(object, source, {}, 4096);
    aliases.appendTriangle(0);
    require(aliases.components().size() == 2 && aliases.triangles()[0].component_kind == DUST3D_UV_AMBIGUOUS && aliases.triangles()[0].component_index == SIZE_MAX, "Engine aliases stay explicitly ambiguous while retaining both original spellings");
    source.components[component.substr(1, 36)]["color"] = "#ff0000";
    rejects([&] {
        dust3d::native::UvMetadata conflicting(source, 1024 * 1024);
        conflicting.prepare(object, source, {}, 4096);
    },
        DUST3D_STATUS_UNSUPPORTED_ASSET);
    source.components.erase(component.substr(1, 36));
    for (const auto* invalid : { "red", "#GG1122", "#12345", "#0011223344" }) {
        source.components[component]["color"] = invalid;
        rejects([&] {
            dust3d::native::UvMetadata unsupported(source, 1024 * 1024);
            unsupported.prepare(object, source, {}, 4096);
        },
            DUST3D_STATUS_UNSUPPORTED_ASSET);
    }
    source.components[component]["color"] = "#ffffff";
    source.components[component]["colorImageId"] = imageKey;
    rejects([&] {
        dust3d::native::UvMetadata unresolved(source, 1024 * 1024);
        unresolved.prepare(object, source, {}, 4096);
    },
        DUST3D_STATUS_UNSUPPORTED_ASSET);
    dust3d::native::UvMetadata resolved(source, 1024 * 1024);
    resolved.prepare(object, source, { { dust3d::Uuid(imageKey), { dust3d::Uuid(imageKey), 64, 128 } } }, 4096);
    require(std::string(resolved.charts()[0].image_id) == imageKey, "Image identity reaches C chart descriptors without a registry");
    dust3d::Snapshot emptyMembership;
    dust3d::native::UvMetadata generated(emptyMembership, 1024 * 1024);
    source.components[component].erase("colorImageId");
    generated.prepare(object, source, {}, 4096);
    generated.appendTriangle(0);
    require(generated.triangles()[0].component_kind == DUST3D_UV_UNKNOWN && generated.triangles()[0].component_index == SIZE_MAX, "Generated identities cannot impersonate an original key");
    rejects([&] { dust3d::native::UvMetadata tiny(source, 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
    require(metadata.components()[0] == component && metadata.charts()[0].color[3] == 128.0 / 255, "Later independent metadata preparation does not mutate an existing owner");
}

std::string archive(const std::string& xml)
{
    const auto header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>";
    std::ostringstream prefix;
    prefix << "DUST3D 1.0 xml " << std::setw(10) << std::setfill('0') << (26 + header.size()) << '\n';
    require(prefix.str().size() == 26, "Independent archive prefix width must be exact");
    return prefix.str() + header + xml;
}

void filteredTriangles()
{
    const std::string firstPart = "{00000100-0000-4000-8000-000000000100}";
    const std::string secondPart = "{00000101-0000-4000-8000-000000000101}";
    const auto xml = "<canvas originX='0' originY='0' originZ='0'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='" + firstPart
        + "' x='0' y='0' z='0' radius='.001'/><node id='{00001001-0000-4000-8000-000000001001}' partId='" + secondPart
        + "' x='1000000' y='1000000' z='1000000' radius='.001'/></nodes><edges/><parts><part id='" + firstPart
        + "'/><part id='" + secondPart + "'/></parts><components><component id='{00000200-0000-4000-8000-000000000200}' combineMode='Uncombined' linkDataType='partId' linkData='" + secondPart
        + "'/><component id='{00000201-0000-4000-8000-000000000201}' combineMode='Uncombined' linkDataType='partId' linkData='" + firstPart + "'/></components></canvas>";
    const auto bytes = archive(xml);
    size_t originalCount = 0;
    bool hasDroppedPrefix = false;
    {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        std::string warning;
        auto snapshot = dust3d::native::readDocument(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), warning);
        dust3d::MeshGenerator generator(snapshot.get());
        snapshot.release();
        generator.generate();
        std::unique_ptr<dust3d::Object> object(generator.takeObject());
        require(generator.isSuccessful() && object, "Independent precision fixture must generate");
        originalCount = object->triangles.size();
        hasDroppedPrefix = object->vertices[object->triangles.front()[0]].x() > 1;
    }
    require(hasDroppedPrefix, "Precision fixture must drop a prefix before retained triangles to exercise ordinal remapping");
    Dust3DError error {};
    Dust3DUvOptions options { sizeof(options), 0, 0, nullptr, 0 };
    Owner owner(dust3d_uv_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(owner != nullptr, std::string("Precision metadata failed: ") + error.message);
    const auto count = dust3d_uv_mesh_triangle_count(owner.get());
    require(count > 0 && count < originalCount && count * 3 == dust3d_mesh_vertex_count(dust3d_uv_mesh_geometry(owner.get())), "Dropped float triangles must not leave extra UV records");
    const auto* vertices = dust3d_mesh_vertices(dust3d_uv_mesh_geometry(owner.get()));
    const auto* triangles = dust3d_uv_mesh_triangles(owner.get());
    for (size_t index = 0; index < count; ++index) {
        require(std::abs(vertices[index * 3].position[0]) < 1, "Only the nearby precision component survives float export");
        require(triangles[index].component_kind == DUST3D_UV_COMPONENT && std::string(dust3d_uv_mesh_component_id(owner.get(), triangles[index].component_index)) == "{00000201-0000-4000-8000-000000000201}", "Retained UV ownership must follow the surviving component, not original ordinals");
    }
}

void publicVisualInputs()
{
    const std::string part = "{00000100-0000-4000-8000-000000000100}";
    auto model = [&](const std::string& attributes) {
        return archive("<canvas originX='0' originY='0' originZ='0'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='" + part
            + "' x='0' y='0' z='0' radius='.1'/></nodes><edges/><parts><part id='" + part
            + "'/></parts><components><component id='" + component + "' linkDataType='partId' linkData='" + part + "' " + attributes + "/></components></canvas>");
    };
    Dust3DUvImage image {};
    std::memcpy(image.image_id, imageKey.c_str(), imageKey.size() + 1);
    image.width = 48;
    image.height = 96;
    Dust3DUvOptions options { sizeof(options), 0, 1024, &image, 1 };
    Dust3DError error {};
    const auto bytes = model("color='#8000FF00' colorImageId='" + imageKey + "'");
    Owner owner(dust3d_uv_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(owner != nullptr, std::string("Resolved public image input failed: ") + error.message);
    const auto* chart = dust3d_uv_mesh_charts(owner.get());
    require(chart && chart[0].color[1] == 1 && chart[0].color[3] == 128.0 / 255 && std::string(chart[0].image_id) == imageKey, "Public loader resolves original alpha-first color and image identity");
    require(std::string(dust3d_uv_mesh_component_id(owner.get(), chart[0].component_index)) == component, "Public chart retains an original opaque component key");
    options.images = nullptr;
    options.image_count = 0;
    Owner missing(dust3d_uv_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(!missing && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && std::string(error.message).find("image") != std::string::npos, "Unresolved images must fail explicitly without pretending to be solid charts");
    for (const auto* color : { "red", "#GG1122", "#12345" }) {
        const auto unsupported = model(std::string("color='") + color + "'");
        Owner rejected(dust3d_uv_mesh_load(reinterpret_cast<const uint8_t*>(unsupported.data()), unsupported.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(!rejected && error.status == DUST3D_STATUS_UNSUPPORTED_ASSET && error.message[0] != '\0', "Unsupported public colors fail with original diagnostic");
    }
    const auto plain = model("");
    Owner defaults(dust3d_uv_mesh_load(reinterpret_cast<const uint8_t*>(plain.data()), plain.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(defaults && dust3d_uv_mesh_charts(defaults.get())[0].color[0] == 1 && dust3d_uv_mesh_charts(defaults.get())[0].color[3] == 1, "Missing colors use opaque reference white");
    require(dust3d_uv_mesh_charts(owner.get())[0].color[3] == 128.0 / 255, "Failed and subsequent calls cannot alter a retained metadata owner");
    require(!dust3d_uv_mesh_load(nullptr, 0, &options, nullptr), "Optional error buffer may be omitted safely");
}

void imageOptions(const std::string& directory)
{
    const auto bytes = read(directory + "/Capsule.ds3");
    Dust3DUvImage image {};
    std::memcpy(image.image_id, imageKey.c_str(), imageKey.size() + 1);
    image.width = 64;
    image.height = 128;
    Dust3DUvOptions options { sizeof(options), 0, 1024, &image, 1 };
    Dust3DError error {};
    Owner valid(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(valid != nullptr, "Unused valid image descriptors must not alter a document's source charts");
    for (const size_t dimension : { size_t(0), size_t(8193) }) {
        image.width = dimension;
        Owner invalid(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(!invalid && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid image dimensions must reject before generation");
    }
    image.width = 64;
    auto duplicate = std::vector<Dust3DUvImage> { image, image };
    options.images = duplicate.data();
    options.image_count = 2;
    Owner aliases(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(!aliases && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Duplicate resolved image identities must reject");
    options.images = &image;
    options.image_count = 1;
    std::memset(image.image_id, 'x', sizeof(image.image_id));
    Owner unterminated(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(!unterminated && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Fixed-buffer image identity must be terminated");
    std::memset(image.image_id, 0, sizeof(image.image_id));
    Owner empty(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
    require(!empty && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Empty image identity must reject");
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> ordinary(dust3d_mesh_load(bytes.data(), bytes.size(), &error), dust3d_mesh_destroy);
    require(ordinary != nullptr, "Opt-in image failure cannot damage the ordinary preview path");
}
void fixtures(const std::string& directory)
{
    for (const std::string name : { "Capsule.ds3", "Sword.ds3", "RoundedCrossing.ds3" }) {
        const auto bytes = read(directory + "/" + name);
        Dust3DUvOptions options { sizeof(options), 0, 0, nullptr, 0 };
        Dust3DError error {};
        Owner owner(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, name + ": " + error.message);
        const auto* geometry = dust3d_uv_mesh_geometry(owner.get());
        const auto count = dust3d_uv_mesh_triangle_count(owner.get());
        require(count > 0 && count * 3 == dust3d_mesh_vertex_count(geometry), "UV records must align with retained expanded triangles");
        const auto* triangles = dust3d_uv_mesh_triangles(owner.get());
        size_t known = 0;
        for (size_t index = 0; index < count; ++index) {
            known += triangles[index].known != 0;
            for (const auto& corner : triangles[index].uv) {
                for (const auto value : corner)
                    require(std::isfinite(value) && value >= 0 && value <= 1, "Published UV coordinates stay in the atlas");
            }
            if (triangles[index].component_kind == DUST3D_UV_COMPONENT)
                require(triangles[index].component_index < dust3d_uv_mesh_component_count(owner.get()), "Known source indices reference original identities");
            else
                require(triangles[index].component_index == SIZE_MAX, "Unknown/ambiguous component records cannot invent an identity");
        }
        require(known > 0 && dust3d_uv_mesh_chart_count(owner.get()) > 0 && dust3d_uv_mesh_texture_size(owner.get()) > 0, "Real models must retain usable charts");
        if (name == "RoundedCrossing.ds3")
            require(known < count, "Unmapped boolean surfaces must remain explicitly unknown");
        if (name == "Sword.ds3")
            require(dust3d_uv_mesh_gradient_count(owner.get()) > 0, "Real seam descriptors must reach C consumers");
        for (size_t index = 0; index < dust3d_uv_mesh_gradient_count(owner.get()); ++index) {
            const auto& gradient = dust3d_uv_mesh_gradients(owner.get())[index];
            require(gradient.chart_index < dust3d_uv_mesh_chart_count(owner.get()) && gradient.resolution == 512, "Gradient ordinals reference owned atlas charts");
        }
        const auto payload = dust3d_uv_mesh_payload_bytes(owner.get());
        require(payload > 0 && std::strcmp(dust3d_uv_mesh_component_id(owner.get(), SIZE_MAX), "") == 0, "Budget and string bounds must be explicit");
        options.maximum_payload_bytes = payload;
        Owner exact(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(exact && dust3d_uv_mesh_payload_bytes(exact.get()) == payload, "Exact retained metadata budget must pass");
        options.maximum_payload_bytes = payload - 1;
        Owner rejected(dust3d_uv_mesh_load(bytes.data(), bytes.size(), &options, &error), dust3d_uv_mesh_destroy);
        require(!rejected && error.status == DUST3D_STATUS_LIMIT_EXCEEDED && error.message[0] != '\0', "One-byte-short metadata budget must fail descriptively");
        const auto first = triangles[0];
        Dust3DIndexedMesh* indexed = dust3d_mesh_create_indexed(geometry, 0, &error);
        require(indexed && dust3d_indexed_mesh_index_count(indexed) == count * 3, "Borrowed UV geometry supports independent indexed copies");
        owner.reset();
        require(std::isfinite(first.uv[0][0]) && dust3d_indexed_mesh_index_count(indexed) == count * 3, "Copied metadata and indexed geometry survive UV owner release");
        dust3d_indexed_mesh_destroy(indexed);
        std::cout << name << ": " << known << '/' << count << " C UV records, " << payload << " metadata bytes\n";
    }
}
}
int main(int argc, char* argv[])
{
    try {
        require(argc == 2, "Expected UV fixture directory");
        nullsAndOptions();
        analyticIdentitiesAndVisuals();
        filteredTriangles();
        imageOptions(argv[1]);
        publicVisualInputs();
        fixtures(argv[1]);
        std::cout << "UV C boundary contracts passed\n";
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}

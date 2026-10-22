// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <json/json.hpp>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;
using Owner = std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)>;
const std::string modelA = "{00000300-0000-4000-8000-000000000300}";
const std::string modelB = "{ABCDEFAB-0000-4000-8000-000000000301}";
const std::string partA = "{00000100-0000-4000-8000-000000000100}";
const std::string partB = partA.substr(1, 36);
const std::string partC = "{00000101-0000-4000-8000-000000000101}";
const std::string componentA = "{00000200-0000-4000-8000-000000000200}";
const std::string componentB = componentA.substr(1, 36);
const std::string componentC = "{00000201-0000-4000-8000-000000000201}";

void require(bool value, const std::string& message)
{
    if (!value)
        throw std::runtime_error(message);
}
void appendWord(Bytes& bytes, uint32_t value)
{
    for (size_t shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<uint8_t>(value >> shift));
}
void appendFloat(Bytes& bytes, float value)
{
    uint32_t word;
    std::memcpy(&word, &value, sizeof(word));
    appendWord(bytes, word);
}
struct ModelFixture {
    nlohmann::json document;
    Bytes binary;
    ModelFixture(const Bytes& image, const std::string& mime, bool hasCustomMaterial)
    {
        for (float value : { 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f,
                 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f,
                 0.f, 0.f, 1.f, 0.f, 0.f, 1.f })
            appendFloat(binary, value);
        document = nlohmann::json::parse(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":96}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"material":0}]}],"materials":[{}]})");
        auto& material = document["materials"][0];
        if (!image.empty()) {
            document["bufferViews"].push_back({ { "buffer", 0 }, { "byteOffset", binary.size() }, { "byteLength", image.size() } });
            document["images"] = nlohmann::json::array({ { { "bufferView", 3 }, { "mimeType", mime } } });
            document["textures"] = nlohmann::json::array({ { { "source", 0 } } });
            material["pbrMetallicRoughness"]["baseColorTexture"] = { { "index", 0 } };
            binary.insert(binary.end(), image.begin(), image.end());
        }
        if (hasCustomMaterial) {
            material["pbrMetallicRoughness"]["baseColorFactor"] = { .25, .5, .75, .5 };
            material["pbrMetallicRoughness"]["metallicFactor"] = .125;
            material["pbrMetallicRoughness"]["roughnessFactor"] = .625;
            material["emissiveFactor"] = { .1, .2, .3 };
            material["alphaMode"] = "MASK";
            material["alphaCutoff"] = .375;
            material["doubleSided"] = true;
            if (!image.empty()) {
                document["textures"][0]["sampler"] = 0;
                document["samplers"] = nlohmann::json::array({ { { "magFilter", 9728 }, { "minFilter", 9984 }, { "wrapS", 33071 }, { "wrapT", 33648 } } });
            }
        }
    }
    Bytes bytes()
    {
        document["buffers"][0]["byteLength"] = binary.size();
        std::string json = document.dump();
        while (json.size() % 4)
            json += ' ';
        auto storage = binary;
        while (storage.size() % 4)
            storage.push_back(0);
        Bytes result;
        appendWord(result, 0x46546c67);
        appendWord(result, 2);
        appendWord(result, static_cast<uint32_t>(28 + json.size() + storage.size()));
        appendWord(result, static_cast<uint32_t>(json.size()));
        appendWord(result, 0x4e4f534a);
        result.insert(result.end(), json.begin(), json.end());
        appendWord(result, static_cast<uint32_t>(storage.size()));
        appendWord(result, 0x004e4942);
        result.insert(result.end(), storage.begin(), storage.end());
        return result;
    }
};
using Asset = std::pair<std::string, Bytes>;
std::string document()
{
    std::string nodes, parts, components;
    const std::array<std::string, 3> identities { partA, partB, partC };
    const std::array<std::string, 3> componentIds { componentA, componentB, componentC };
    for (size_t index = 0; index < identities.size(); ++index) {
        std::ostringstream node;
        node << "{0000100" << index << "-0000-4000-8000-00000000100" << index << "}";
        nodes += "<node id='" + node.str() + "' partId='" + identities[index] + "' x='.5' y='.5' z='.5' radius='.1'/>";
        parts += "<part id='" + identities[index] + "' target='ImportedModel' importedModelId='"
            + (index == 2 ? modelB : modelA.substr(1, 36)) + "'/>";
        components += "<component id='" + componentIds[index] + "' linkDataType='partId' linkData='" + identities[index == 2 ? 0 : index] + "'/>";
    }
    components += "<component id='{00000202-0000-4000-8000-000000000202}'/>";
    return "<canvas originX='.5' originY='.5' originZ='.5'><nodes>" + nodes + "</nodes><parts>" + parts + "</parts><components>" + components + "</components></canvas>";
}
Bytes archive(const std::string& xml, const std::vector<Asset>& assets)
{
    std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/>";
    size_t offset = xml.size();
    for (const auto& asset : assets) {
        header += "<asset name='" + asset.first + "' offset='" + std::to_string(offset) + "' size='" + std::to_string(asset.second.size()) + "'/>";
        offset += asset.second.size();
    }
    header += "</ds3>\n";
    const std::string prefix = "DUST3D 1.0 xml ";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    const auto text = stream.str();
    Bytes result(text.begin(), text.end());
    for (const auto& asset : assets)
        result.insert(result.end(), asset.second.begin(), asset.second.end());
    return result;
}
Bytes read(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Missing independently encoded image fixture");
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
Owner load(const Bytes& bytes, size_t maximumBytes = 0)
{
    Dust3DImportedAssetOptions options { sizeof(Dust3DImportedAssetOptions), maximumBytes };
    Dust3DError error {};
    Owner result(dust3d_imported_assets_load(bytes.data(), bytes.size(), &options, &error), dust3d_imported_assets_destroy);
    require(result && error.status == DUST3D_STATUS_OK, std::string("Material owner failed: ") + error.message);
    return result;
}
void reject(const Bytes& bytes, Dust3DStatus status, const std::string& cause, size_t maximumBytes = 0)
{
    Dust3DImportedAssetOptions options { sizeof(Dust3DImportedAssetOptions), maximumBytes };
    Dust3DError error {};
    Owner result(dust3d_imported_assets_load(bytes.data(), bytes.size(), &options, &error), dust3d_imported_assets_destroy);
    require(!result && error.status == status && std::string(error.message).find(cause) != std::string::npos,
        std::string("Material rejection lost status/cause: ") + error.message);
}
Dust3DImportedMaterialView material(const Dust3DImportedAssets* owner, const std::string& identity)
{
    for (size_t index = 0; index < dust3d_imported_assets_model_count(owner); ++index) {
        const auto view = dust3d_imported_assets_material(owner, index);
        if (view.model_id && view.model_id == identity)
            return view;
    }
    throw std::runtime_error("Material identity disappeared or changed case");
}
void testFieldsBindingsAndLifetime(const Bytes& png, const Bytes& jpeg)
{
    ModelFixture first(png, "image/png", true), second(jpeg, "image/jpeg", false);
    auto bytes = archive(document(), { { "models/" + modelA + ".glb", first.bytes() }, { "models/" + modelB + ".glb", second.bytes() } });
    const auto original = bytes;
    auto owner = load(bytes);
    require(bytes == original, "Material inspection changed original archive bytes");
    require(dust3d_imported_assets_model_count(owner.get()) == 2 && dust3d_imported_assets_binding_count(owner.get()) == 4,
        "Shared models or uninstanced parts were lost/duplicated");
    const auto custom = material(owner.get(), modelA);
    require(std::equal(std::begin(custom.base_color), std::end(custom.base_color), std::array<double, 4> { .25, .5, .75, .5 }.begin()),
        "Base color factor was changed or premultiplied");
    require(custom.metallic == .125 && custom.roughness == .625
            && custom.emissive[0] == .1 && custom.emissive[1] == .2 && custom.emissive[2] == .3,
        "Imported material scalars were lost");
    require(custom.alpha_mode == DUST3D_IMPORTED_ALPHA_MASK && custom.alpha_cutoff == .375 && custom.is_double_sided == 1, "Alpha or sidedness state was lost");
    require(custom.has_sampler == 1 && custom.has_magnification_filter == 1 && custom.has_minification_filter == 1
            && custom.magnification_filter == 9728 && custom.minification_filter == 9984 && custom.wrap_s == 33071 && custom.wrap_t == 33648,
        "Explicit sampler fields were lost");
    require(custom.image.mime_type && std::string(custom.image.mime_type) == "image/png"
            && custom.image.byte_count == png.size() && std::equal(png.begin(), png.end(), custom.image.bytes),
        "PNG bytes do not survive the public owner");
    const auto defaults = material(owner.get(), modelB);
    require(defaults.base_color[0] == 1 && defaults.base_color[1] == 1 && defaults.base_color[2] == 1 && defaults.base_color[3] == 1
            && defaults.metallic == 1 && defaults.roughness == 1 && defaults.emissive[0] == 0 && defaults.emissive[1] == 0 && defaults.emissive[2] == 0
            && defaults.alpha_mode == DUST3D_IMPORTED_ALPHA_OPAQUE && defaults.alpha_cutoff == .5 && defaults.is_double_sided == 0,
        "Absent material defaults changed");
    require(defaults.has_sampler == 1 && defaults.has_magnification_filter == 0 && defaults.has_minification_filter == 0
            && defaults.wrap_s == 10497 && defaults.wrap_t == 10497,
        "Default sampler presence was confused with filter presence");
    require(defaults.image.mime_type && std::string(defaults.image.mime_type) == "image/jpeg" && defaults.image.byte_count == jpeg.size()
            && std::equal(jpeg.begin(), jpeg.end(), defaults.image.bytes),
        "JPEG bytes do not survive the public owner");
    std::vector<std::tuple<std::string, std::string, std::string>> bindings;
    for (size_t index = 0; index < dust3d_imported_assets_binding_count(owner.get()); ++index) {
        const auto binding = dust3d_imported_assets_binding(owner.get(), index);
        require(binding.part_id && binding.model_index < 2, "Invalid public material binding");
        const auto model = dust3d_imported_assets_material(owner.get(), binding.model_index);
        bindings.emplace_back(binding.component_id ? binding.component_id : "", binding.part_id, model.model_id);
    }
    auto expected = std::vector<std::tuple<std::string, std::string, std::string>> {
        { componentA, partA, modelA }, { componentB, partB, modelA }, { componentC, partA, modelA }, { "", partC, modelB }
    };
    std::sort(bindings.begin(), bindings.end());
    std::sort(expected.begin(), expected.end());
    require(bindings == expected, "Opaque component/part aliases or uninstanced bindings collapsed");
    std::fill(bytes.begin(), bytes.end(), 0);
    auto later = load(original);
    later.reset();
    require(std::equal(png.begin(), png.end(), custom.image.bytes), "Borrowed view depended on input or another owner");
    const auto copy = Bytes(custom.image.bytes, custom.image.bytes + custom.image.byte_count);
    const auto copiedIdentity = std::string(custom.model_id);
    const auto payload = dust3d_imported_assets_payload_bytes(owner.get());
    require(payload > png.size() + jpeg.size(), "Retained quota omitted owned records/identities");
    auto exact = load(original, payload);
    reject(original, DUST3D_STATUS_LIMIT_EXCEEDED, "payload", payload - 1);
    reject(original, DUST3D_STATUS_LIMIT_EXCEEDED, "payload", 1);
    owner.reset();
    require(copy == png && copiedIdentity == modelA, "Independent copies changed after owner destruction");
}
void testAbsentDataAndErrors(const Bytes& png)
{
    ModelFixture plain({}, "", false);
    auto bytes = archive(document(), { { "models/" + modelA + ".glb", plain.bytes() }, { "models/" + modelB + ".glb", plain.bytes() } });
    auto owner = load(bytes);
    const auto view = material(owner.get(), modelA);
    require(view.has_sampler == 0 && view.image.mime_type == nullptr && view.image.bytes == nullptr && view.image.byte_count == 0, "Absent image/sampler was invented");
    plain.document["materials"][0]["alphaMode"] = "BLEND";
    bytes = archive(document(), { { "models/" + modelA + ".glb", plain.bytes() }, { "models/" + modelB + ".glb", plain.bytes() } });
    require(material(load(bytes).get(), modelA).alpha_mode == DUST3D_IMPORTED_ALPHA_BLEND, "Blend mode was lost");
    const auto emptyBytes = archive("<canvas/>", {});
    auto empty = load(emptyBytes);
    Dust3DError geometryError {};
    std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> noGeometry(
        dust3d_mesh_load(emptyBytes.data(), emptyBytes.size(), &geometryError), dust3d_mesh_destroy);
    require(!noGeometry && geometryError.status == DUST3D_STATUS_INVALID_DOCUMENT,
        "Metadata-only empty acceptance changed the legacy geometry contract");
    require(dust3d_imported_assets_model_count(empty.get()) == 0 && dust3d_imported_assets_binding_count(empty.get()) == 0, "Empty metadata owner invented models");
    auto procedural = document();
    size_t targetOffset = 0;
    while ((targetOffset = procedural.find("target='ImportedModel'", targetOffset)) != std::string::npos)
        procedural.replace(targetOffset, std::strlen("target='ImportedModel'"), "target='Model'");
    auto noImports = load(archive(procedural, {}));
    require(dust3d_imported_assets_model_count(noImports.get()) == 0 && dust3d_imported_assets_binding_count(noImports.get()) == 0,
        "Unused procedural import metadata became a required asset");
    auto badReference = document();
    const auto referenceOffset = badReference.find("linkData='" + partA + "'");
    require(referenceOffset != std::string::npos, "Reference mutation marker missing");
    badReference.replace(referenceOffset, std::strlen("linkData='") + partA.size() + 1,
        "linkData='{00000199-0000-4000-8000-000000000199}'");
    reject(archive(badReference, {}), DUST3D_STATUS_INVALID_DOCUMENT, "missing part");
    auto badNumber = document();
    const auto numberOffset = badNumber.find("x='.5'");
    require(numberOffset != std::string::npos, "Numeric mutation marker missing");
    badNumber.replace(numberOffset, std::strlen("x='.5'"), "x='invalid-number'");
    std::string numberCause;
    try {
        std::stod("invalid-number");
    } catch (const std::invalid_argument& cause) {
        numberCause = cause.what();
    }
    require(!numberCause.empty(), "Numeric rejection oracle is missing");
    reject(archive(badNumber, {}), DUST3D_STATUS_INVALID_DOCUMENT, numberCause);
    ModelFixture image(png, "image/png", true);
    reject(archive(document(), {}), DUST3D_STATUS_INVALID_DOCUMENT, "Missing required", 1);
    reject(archive(document(), { { "models/" + modelA + ".glb", { 1, 2, 3 } } }), DUST3D_STATUS_INVALID_DOCUMENT, "Embedded model");
    reject(archive(document(), { { "models/" + modelA + ".glb", image.bytes() }, { "models/" + modelA.substr(1, 36) + ".glb", image.bytes() } }),
        DUST3D_STATUS_INVALID_DOCUMENT, "ambiguous");
    image.document["images"][0]["uri"] = "https://example.invalid/never-fetch.png";
    reject(archive(document(), { { "models/" + modelA + ".glb", image.bytes() } }), DUST3D_STATUS_UNSUPPORTED_ASSET, "external");
    ModelFixture opaque({ 1, 2, 3, 4 }, "image/png", false);
    auto notPixels = load(archive(document(), { { "models/" + modelA + ".glb", opaque.bytes() }, { "models/" + modelB + ".glb", opaque.bytes() } }));
    require(material(notPixels.get(), modelA).image.byte_count == 4, "Encoded ownership improperly claimed pixel decoding");
}
void testBoundaryContract()
{
    Dust3DError error {};
    Dust3DImportedAssetOptions options { 0, 0 };
    Owner invalid(dust3d_imported_assets_load(nullptr, 0, &options, &error), dust3d_imported_assets_destroy);
    require(!invalid && error.status == DUST3D_STATUS_INVALID_DOCUMENT && error.message[0], "Short options were accepted");
    options.struct_size = sizeof(options);
    options.maximum_payload_bytes = DUST3D_MAX_IMPORTED_ASSET_PAYLOAD_BYTES + size_t(1);
    invalid.reset(dust3d_imported_assets_load(nullptr, 0, &options, &error));
    require(!invalid && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid quota was accepted");
    invalid.reset(dust3d_imported_assets_load(nullptr, 1, nullptr, &error));
    require(!invalid && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid archive pointer was accepted");
    require(!dust3d_imported_assets_load(nullptr, 0, nullptr, nullptr), "Null diagnostic must be safe");
    const auto materialView = dust3d_imported_assets_material(nullptr, SIZE_MAX);
    const auto binding = dust3d_imported_assets_binding(nullptr, SIZE_MAX);
    require(dust3d_imported_assets_model_count(nullptr) == 0 && dust3d_imported_assets_binding_count(nullptr) == 0
            && dust3d_imported_assets_payload_bytes(nullptr) == 0 && materialView.model_id == nullptr
            && materialView.image.bytes == nullptr && materialView.image.byte_count == 0
            && binding.component_id == nullptr && binding.part_id == nullptr && binding.model_index == 0,
        "Null accessors must return empty views");
    auto empty = load(archive("<canvas/>", {}));
    require(dust3d_imported_assets_material(empty.get(), SIZE_MAX).model_id == nullptr
            && dust3d_imported_assets_binding(empty.get(), SIZE_MAX).part_id == nullptr,
        "Out-of-range accessors must return empty views");
    dust3d_imported_assets_destroy(nullptr);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture directory is required");
        const auto png = read(std::string(argv[1]) + "/ImportedMaterial.png");
        const auto jpeg = read(std::string(argv[1]) + "/ImportedMaterial.jpg");
        testBoundaryContract();
        testFieldsBindingsAndLifetime(png, jpeg);
        testAbsentDataAndErrors(png);
        std::cout << "Imported material ownership contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

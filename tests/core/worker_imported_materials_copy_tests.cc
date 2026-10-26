// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerImportedMaterialsCopy.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void healthy(const char* directory, const char* name, bool hasMaterials)
{
    std::ifstream input(std::string(directory) + '/' + name, std::ios::binary);
    require(bool(input), "Licensed ordinary material fixture opens");
    const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
    Dust3DError error {};
    std::unique_ptr<Dust3DImportedAssets, decltype(&dust3d_imported_assets_destroy)> owner(
        dust3d_imported_assets_load(archive.data(), archive.size(), nullptr, &error), dust3d_imported_assets_destroy);
    require(owner != nullptr, error.message);
    auto copy = copyNativeImportedMaterials(owner.get(), {});
    require(copy.materials.size() == dust3d_imported_assets_model_count(owner.get()) && copy.bindings.size() == dust3d_imported_assets_binding_count(owner.get())
            && copy.materials.empty() == !hasMaterials,
        "Native material/binding counts and procedural empty facts remain exact");
    for (size_t index = 0; index < copy.materials.size(); ++index) {
        const auto expected = dust3d_imported_assets_material(owner.get(), index);
        const auto& actual = copy.materials[index];
        require(actual.modelId == expected.model_id && std::memcmp(actual.baseColor.data(), expected.base_color, sizeof(expected.base_color)) == 0
                && actual.metallic == expected.metallic && actual.roughness == expected.roughness && std::memcmp(actual.emissive.data(), expected.emissive, sizeof(expected.emissive)) == 0
                && actual.alphaCutoff == expected.alpha_cutoff && actual.alphaMode == uint32_t(expected.alpha_mode) && actual.isDoubleSided == expected.is_double_sided,
            "Every original identity, factor, alpha policy and sidedness survives copying");
        require(actual.sampler.has_value() == bool(expected.has_sampler), "Sampler absence is not fabricated");
        if (actual.sampler)
            require(actual.sampler->magnification.has_value() == bool(expected.has_magnification_filter)
                    && actual.sampler->minification.has_value() == bool(expected.has_minification_filter)
                    && actual.sampler->magnification.value_or(0) == expected.magnification_filter && actual.sampler->minification.value_or(0) == expected.minification_filter
                    && actual.sampler->wrapS == expected.wrap_s && actual.sampler->wrapT == expected.wrap_t,
                "Filter presence, GL constants and axis wrapping survive exactly");
        require(actual.image.has_value() == bool(expected.image.mime_type), "Image declaration absence stays explicit");
        if (actual.image)
            require(actual.image->bytes.size() == expected.image.byte_count && std::equal(actual.image->bytes.begin(), actual.image->bytes.end(), expected.image.bytes)
                    && (actual.image->encoding == ImportedImageEncoding::png ? std::strcmp(expected.image.mime_type, "image/png") == 0 : std::strcmp(expected.image.mime_type, "image/jpeg") == 0),
                "Declared PNG/JPEG and all opaque encoded bytes are preserved without decoding");
    }
    for (size_t index = 0; index < copy.bindings.size(); ++index) {
        const auto expected = dust3d_imported_assets_binding(owner.get(), index);
        const auto& actual = copy.bindings[index];
        require(actual.modelIndex == expected.model_index && actual.partId == expected.part_id && actual.componentId.has_value() == bool(expected.component_id)
                && (!actual.componentId || *actual.componentId == expected.component_id),
            "Original model references, part keys and optional instance keys survive");
    }
    if (hasMaterials)
        for (int field = 0; field < 5; ++field) {
            ImportedMaterialsLimits limits;
            if (field == 0)
                limits.maximumWireBytes = 1;
            if (field == 1)
                limits.maximumDecodedBytes = 1;
            if (field == 2)
                limits.maximumTextBytes = 1;
            if (field == 3)
                limits.maximumEncodedBytes = 1;
            if (field == 4)
                limits.maximumImageBytes = 1;
            bool rejected = false;
            try {
                (void)copyNativeImportedMaterials(owner.get(), limits);
            } catch (const ImportedMaterialsError& cause) {
                rejected = cause.code() == ImportedMaterialsErrorCode::limitExceeded && cause.what()[0];
            }
            require(rejected, "Copy-only quotas reuse the healthy owner without another native load");
        }
    const auto wire = encodeImportedMaterialsPayload(copy, {});
    owner.reset();
    require(encodeImportedMaterialsPayload(copy, {}) == wire && encodeImportedMaterialsPayload(decodeImportedMaterialsPayload(wire.data(), wire.size(), {}), {}) == wire,
        "All copied material/image/binding buffers survive native destruction and wire round trip");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Known-positive fixture directory only");
        healthy(argv[1], "ImportedSurface.ds3", true);
        healthy(argv[1], "Capsule.ds3", false);
        bool rejected = false;
        try {
            (void)copyNativeImportedMaterials(nullptr, {});
        } catch (const ImportedMaterialsError& cause) {
            rejected = cause.code() == ImportedMaterialsErrorCode::invalidInput;
        }
        require(rejected, "Missing material owner rejects before native accessors");
        std::cout << "Imported material native copy passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <cmath>
#include <iostream>
#include <limits>
#include <native/Engine/WorkerImportedMaterialReader.h>
#include <native/Engine/WorkerImportedMaterials.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool condition, const char* cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(ImportedMaterialsErrorCode code, Action action)
{
    try {
        action();
    } catch (const ImportedMaterialsError& cause) {
        if (cause.code() != code)
            std::cerr << cause.what() << '\n';
        require(cause.code() == code && cause.what()[0], "Imported material rejection retains original typed cause");
        return;
    }
    throw std::runtime_error("Invalid imported material facts accepted");
}
ImportedMaterialsPayload sample()
{
    ImportedMaterialsPayload value;
    ImportedMaterial first;
    first.modelId = "é";
    first.baseColor = { -0.0, 0.5, 1, 0.25 };
    first.metallic = 0.4;
    first.roughness = 0.6;
    first.emissive = { 0.1, 0.2, 0.3 };
    first.alphaMode = 2;
    first.alphaCutoff = 1.5;
    first.isDoubleSided = 1;
    ImportedMaterialSampler sampler;
    sampler.minification = 9987;
    sampler.wrapS = 33648;
    sampler.wrapT = 33071;
    first.sampler = sampler;
    // Opaque declared bytes exercise transfer, not image decoding.
    first.image = ImportedEncodedImage { ImportedImageEncoding::jpeg, { 255, 216, 0, 255 } };
    ImportedMaterial second;
    second.modelId = "é";
    value.materials = { first, second };
    value.bindings = { { 0, "partA", std::string("instanceA") }, { 0, "partA", std::string("instanceB") }, { 1, "partB", std::nullopt } };
    return value;
}
void set(std::vector<uint8_t>& wire, size_t offset, uint64_t value, size_t width)
{
    require(offset <= wire.size() && width <= wire.size() - offset, "Pure fact-packet mutation remains bounded");
    detail::writeUnsigned(wire.data() + offset, value, width);
}
void ownershipAndPolicies()
{
    auto value = sample();
    auto wire = encodeImportedMaterialsPayload(value, {});
    const auto expected = wire;
    const auto shape = inspectImportedMaterialsPayload(wire.data(), wire.size(), {});
    require(shape.materialCount == 2 && shape.bindingCount == 3 && shape.encodedBytes == 4, "Material, binding and opaque image counts stay exact");
    require(shape.logicalBytes == 2 * sizeof(ImportedMaterial) + 3 * sizeof(ImportedBinding) + shape.textBytes + shape.encodedBytes + 7, "Logical record/text/image charge includes each retained terminator");
    ImportedMaterialsLimits exact;
    exact.maximumWireBytes = wire.size();
    exact.maximumDecodedBytes = shape.logicalBytes;
    exact.maximumMaterials = 2;
    exact.maximumBindings = 3;
    exact.maximumTextBytes = shape.textBytes;
    exact.maximumEncodedBytes = 4;
    exact.maximumImageBytes = 4;
    auto copy = decodeImportedMaterialsPayload(wire.data(), wire.size(), exact);
    require(encodeImportedMaterialsPayload(copy, exact) == expected && std::signbit(copy.materials[0].baseColor[0]) && copy.materials[0].alphaCutoff == 1.5,
        "Double bits and nonnegative cutoff above one survive exact quotas");
    require(copy.materials[0].sampler && !copy.materials[0].sampler->magnification && copy.materials[0].sampler->minification == 9987
            && !copy.materials[1].sampler && copy.bindings[2].componentId == std::nullopt,
        "Filter, sampler and uninstanced component absence remain explicit");
    require(copy.materials[0].modelId != copy.materials[1].modelId && copy.materials[0].image->bytes == std::vector<uint8_t>({ 255, 216, 0, 255 }), "Unicode alias keys and opaque encoded bytes are not normalized");
    value = {};
    wire.assign(wire.size(), 0);
    require(encodeImportedMaterialsPayload(copy, {}) == expected, "Decoded material strings and encoded images own independent storage");
    for (int field = 0; field < 7; ++field) {
        auto limits = exact;
        if (field == 0)
            --limits.maximumWireBytes;
        if (field == 1)
            --limits.maximumDecodedBytes;
        if (field == 2)
            --limits.maximumMaterials;
        if (field == 3)
            --limits.maximumBindings;
        if (field == 4)
            --limits.maximumTextBytes;
        if (field == 5)
            --limits.maximumEncodedBytes;
        if (field == 6)
            --limits.maximumImageBytes;
        reject(ImportedMaterialsErrorCode::limitExceeded, [&] { encodeImportedMaterialsPayload(copy, limits); });
        reject(ImportedMaterialsErrorCode::limitExceeded, [&] { decodeImportedMaterialsPayload(expected.data(), expected.size(), limits); });
    }
    ImportedMaterialsLimits tiny;
    tiny.maximumWireBytes = 1;
    reject(ImportedMaterialsErrorCode::limitExceeded, [&] { encodeImportedMaterialsPayload(copy, tiny); });
    auto absent = encodeImportedMaterialsPayload({}, {});
    auto empty = decodeImportedMaterialsPayload(absent.data(), absent.size(), {});
    require(empty.materials.empty() && empty.bindings.empty(), "A procedural archive may have no imported facts");
    copy.materials[0].sampler->magnification = 9728;
    const auto explicitFilter = encodeImportedMaterialsPayload(copy, {});
    require(explicitFilter != expected && decodeImportedMaterialsPayload(explicitFilter.data(), explicitFilter.size(), {}).materials[0].sampler->magnification == 9728,
        "An explicit filter differs from unspecified filtering");
}
void materialValidation()
{
    const auto original = sample();
    auto invalid = [&](ImportedMaterialsPayload value, ImportedMaterialsErrorCode code) { reject(code, [&] { encodeImportedMaterialsPayload(value, {}); }); };
    for (const auto& key : { std::string(), std::string(39, 'x'), std::string("a\0b", 3), std::string("\xff", 1) }) {
        auto value = original;
        value.materials[0].modelId = key;
        invalid(value, ImportedMaterialsErrorCode::invalidIdentity);
        value = original;
        value.bindings[0].partId = key;
        invalid(value, ImportedMaterialsErrorCode::invalidIdentity);
        value = original;
        value.bindings[0].componentId = key;
        invalid(value, ImportedMaterialsErrorCode::invalidIdentity);
    }
    auto value = original;
    value.materials[1].modelId = value.materials[0].modelId;
    invalid(value, ImportedMaterialsErrorCode::invalidIdentity);
    for (double scalar : { -0.01, 1.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.materials[0].baseColor[0] = scalar;
        invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
        value = original;
        value.materials[0].emissive[0] = scalar;
        invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
        value = original;
        value.materials[0].metallic = scalar;
        invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
        value = original;
        value.materials[0].roughness = scalar;
        invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
    }
    for (double cutoff : { -0.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() }) {
        value = original;
        value.materials[0].alphaCutoff = cutoff;
        invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
    }
    value = original;
    value.materials[0].isDoubleSided = 2;
    invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
    value = original;
    value.materials[0].alphaMode = 3;
    invalid(value, ImportedMaterialsErrorCode::invalidMaterial);
    for (int field = 0; field < 4; ++field) {
        value = original;
        if (field == 0)
            value.materials[0].sampler->magnification = 0;
        if (field == 1)
            value.materials[0].sampler->minification = 0;
        if (field == 2)
            value.materials[0].sampler->wrapS = 0;
        if (field == 3)
            value.materials[0].sampler->wrapT = 0;
        invalid(value, ImportedMaterialsErrorCode::invalidSampler);
    }
    value = original;
    value.materials[0].image.reset();
    invalid(value, ImportedMaterialsErrorCode::invalidImage);
    value = original;
    value.materials[0].sampler.reset();
    invalid(value, ImportedMaterialsErrorCode::invalidImage);
    value = original;
    value.materials[0].image->bytes.clear();
    invalid(value, ImportedMaterialsErrorCode::invalidImage);
    value = original;
    value.materials[0].image->encoding = static_cast<ImportedImageEncoding>(3);
    invalid(value, ImportedMaterialsErrorCode::invalidImage);
}
void bindings()
{
    const auto original = sample();
    auto invalid = [&](ImportedMaterialsPayload value) { reject(ImportedMaterialsErrorCode::invalidBinding, [&] { encodeImportedMaterialsPayload(value, {}); }); };
    auto value = original;
    value.bindings[0].modelIndex = 2;
    invalid(value);
    value = original;
    value.bindings[1].modelIndex = 1;
    invalid(value);
    value = original;
    value.bindings[1].componentId = value.bindings[0].componentId;
    invalid(value);
    value = original;
    value.bindings[0].componentId.reset();
    invalid(value);
    value = original;
    value.bindings.push_back(value.bindings[2]);
    invalid(value);
    value = original;
    value.materials.clear();
    invalid(value);
    value = original;
    value.bindings[0].partId = "é";
    value.bindings[1].partId = "é";
    value.bindings[0].componentId = "é";
    value.bindings[1].componentId = "é";
    require(!encodeImportedMaterialsPayload(value, {}).empty(), "Binding identities preserve distinct Unicode spellings");
    value = original;
    std::swap(value.bindings[0], value.bindings[2]);
    require(!encodeImportedMaterialsPayload(value, {}).empty(), "Binding validity does not depend on record order");
}
class ScalarReader final : public detail::ImportedMaterialReader {
public:
    detail::ImportedMaterialView material;
    ScalarReader()
    {
        material.modelId = "raw-key";
        material.baseColor = { 1, 1, 1, 1 };
    }
    uint64_t materialCount() const noexcept override { return 1; }
    uint64_t bindingCount() const noexcept override { return 0; }
    void rewind() noexcept override { }
    detail::ImportedMaterialView readMaterial() override { return material; }
    detail::ImportedBindingView readBinding() override { return {}; }
    void finish(const detail::ImportedMaterialsMeasurement&) const override { }
};
void rawScalarPolicies()
{
    // These stable pure views exercise raw C-style fields without any native owner or parser.
    ScalarReader valid;
    require(detail::inspectImportedMaterialReader(valid, {}).materialCount == 1, "A scalar-only material reader uses the same facts policy");
    for (int field = 0; field < 3; ++field) {
        ScalarReader reader;
        if (field == 0)
            reader.material.hasSampler = 2;
        if (field == 1)
            reader.material.hasMagnification = 2;
        if (field == 2)
            reader.material.hasMinification = 2;
        reject(ImportedMaterialsErrorCode::invalidSampler, [&] { detail::inspectImportedMaterialReader(reader, {}); });
    }
    for (int field = 0; field < 6; ++field) {
        ScalarReader reader;
        if (field == 0)
            reader.material.hasMagnification = 1;
        if (field == 1)
            reader.material.hasMinification = 1;
        if (field == 2)
            reader.material.magnification = 9728;
        if (field == 3)
            reader.material.minification = 9987;
        if (field == 4)
            reader.material.wrapS = 10497;
        if (field == 5)
            reader.material.wrapT = 10497;
        reject(ImportedMaterialsErrorCode::invalidSampler, [&] { detail::inspectImportedMaterialReader(reader, {}); });
    }
    const uint8_t opaque[] { 1, 2, 3, 4 };
    for (int field = 0; field < 4; ++field) {
        ScalarReader reader;
        if (field == 0)
            reader.material.hasImage = 2;
        if (field == 1)
            reader.material.imageEncoding = 1;
        if (field == 2)
            reader.material.imageData = opaque;
        if (field == 3)
            reader.material.imageSize = sizeof(opaque);
        reject(ImportedMaterialsErrorCode::invalidImage, [&] { detail::inspectImportedMaterialReader(reader, {}); });
    }
    ScalarReader present;
    present.material.hasSampler = present.material.hasImage = 1;
    present.material.wrapS = present.material.wrapT = 10497;
    present.material.imageEncoding = 1;
    present.material.imageSize = sizeof(opaque);
    present.material.imageData = opaque;
    require(detail::copyImportedMaterialReader(present, {}).materials[0].image->bytes.size() == sizeof(opaque), "Borrowed opaque bytes become independent facts after shared preflight");
    present.material.imageSize = 0;
    reject(ImportedMaterialsErrorCode::invalidImage, [&] { detail::inspectImportedMaterialReader(present, {}); });
    present.material.imageSize = sizeof(opaque);
    present.material.imageData = nullptr;
    reject(ImportedMaterialsErrorCode::invalidImage, [&] { detail::inspectImportedMaterialReader(present, {}); });
    auto value = sample();
    value.materials[0].image->encoding = ImportedImageEncoding::png;
    for (uint32_t minification : { 9728U, 9729U, 9984U, 9985U, 9986U, 9987U }) {
        value.materials[0].sampler->magnification = 9729;
        value.materials[0].sampler->minification = minification;
        value.materials[0].sampler->wrapS = value.materials[0].sampler->wrapT = 10497;
        const auto wire = encodeImportedMaterialsPayload(value, {});
        require(decodeImportedMaterialsPayload(wire.data(), wire.size(), {}).materials[0].sampler->minification == minification, "Every supported explicit minification filter is preserved");
    }
    value.materials[0].sampler->minification.reset();
    require(!encodeImportedMaterialsPayload(value, {}).empty(), "Unspecified minification stays valid when the source image has a sampler");
    value.materials[0].sampler->minification = 9990;
    reject(ImportedMaterialsErrorCode::invalidSampler, [&] { encodeImportedMaterialsPayload(value, {}); });
}
void framing()
{
    const auto wire = encodeImportedMaterialsPayload(sample(), {});
    reject(ImportedMaterialsErrorCode::invalidInput, [&] { decodeImportedMaterialsPayload(nullptr, wire.size(), {}); });
    for (size_t size = 0; size < wire.size(); ++size)
        reject(ImportedMaterialsErrorCode::truncated, [&] { inspectImportedMaterialsPayload(wire.data(), size, {}); });
    auto extra = wire;
    extra.push_back(0);
    reject(ImportedMaterialsErrorCode::trailingData, [&] { decodeImportedMaterialsPayload(extra.data(), extra.size(), {}); });
    auto mutation = [&](size_t offset, uint64_t value, size_t width, ImportedMaterialsErrorCode code) {
        auto changed = wire;
        set(changed, offset, value, width);
        reject(code, [&] { decodeImportedMaterialsPayload(changed.data(), changed.size(), {}); });
    };
    mutation(0, 2, 2, ImportedMaterialsErrorCode::unsupportedVersion);
    mutation(2, 1, 2, ImportedMaterialsErrorCode::invalidLayout);
    mutation(4, 1, 2, ImportedMaterialsErrorCode::invalidLayout);
    mutation(6, 1, 2, ImportedMaterialsErrorCode::invalidLayout);
    mutation(8, UINT64_MAX, 8, ImportedMaterialsErrorCode::limitExceeded);
    mutation(16, UINT64_MAX, 8, ImportedMaterialsErrorCode::limitExceeded);
    mutation(24, maximumPayloadBytes + 1, 8, ImportedMaterialsErrorCode::limitExceeded);
    mutation(32, maximumPayloadBytes + 1, 8, ImportedMaterialsErrorCode::limitExceeded);
    mutation(40 + 84, 9728, 4, ImportedMaterialsErrorCode::invalidSampler);
    mutation(40 + 112, 28, 1, ImportedMaterialsErrorCode::invalidSampler);
    mutation(40 + 112, 25, 1, ImportedMaterialsErrorCode::invalidSampler);
    mutation(40 + 100, UINT32_MAX, 4, ImportedMaterialsErrorCode::truncated);
    mutation(40 + 104, UINT64_MAX, 8, ImportedMaterialsErrorCode::truncated);
    auto repartitioned = wire;
    const auto text = detail::readUnsigned(wire.data() + 24, 8), encoded = detail::readUnsigned(wire.data() + 32, 8);
    set(repartitioned, 24, text + 1, 8);
    set(repartitioned, 32, encoded - 1, 8);
    reject(ImportedMaterialsErrorCode::invalidLayout, [&] { decodeImportedMaterialsPayload(repartitioned.data(), repartitioned.size(), {}); });
    auto unconsumed = wire;
    unconsumed.push_back(0);
    set(unconsumed, 32, encoded + 1, 8);
    reject(ImportedMaterialsErrorCode::invalidLayout, [&] { decodeImportedMaterialsPayload(unconsumed.data(), unconsumed.size(), {}); });
    const size_t binding = 40 + 2 * 128 + 2 + 4 + 3;
    mutation(binding + 16, 2, 1, ImportedMaterialsErrorCode::invalidBinding);
    mutation(binding + 16, 0, 1, ImportedMaterialsErrorCode::invalidBinding);
    mutation(binding + 8, UINT32_MAX, 4, ImportedMaterialsErrorCode::truncated);
    mutation(binding + 12, UINT32_MAX, 4, ImportedMaterialsErrorCode::truncated);
    for (size_t offset = binding + 17; offset < binding + 24; ++offset)
        mutation(offset, 1, 1, ImportedMaterialsErrorCode::invalidLayout);
    for (size_t offset = 40 + 114; offset < 40 + 128; ++offset)
        mutation(offset, 1, 1, ImportedMaterialsErrorCode::invalidLayout);
    mutation(40 + 112, 128, 1, ImportedMaterialsErrorCode::invalidLayout);
    for (int field = 0; field < 7; ++field)
        for (uint64_t amount : { uint64_t(0), maximumPayloadBytes + 1 }) {
            ImportedMaterialsLimits limits;
            if (field == 0)
                limits.maximumWireBytes = amount;
            if (field == 1)
                limits.maximumDecodedBytes = amount;
            if (field == 2)
                limits.maximumMaterials = amount;
            if (field == 3)
                limits.maximumBindings = amount;
            if (field == 4)
                limits.maximumTextBytes = amount;
            if (field == 5)
                limits.maximumEncodedBytes = amount;
            if (field == 6)
                limits.maximumImageBytes = amount;
            reject(ImportedMaterialsErrorCode::invalidLimits, [&] { validateImportedMaterialsLimits(limits); });
        }
}
}
int main()
{
    try {
        ownershipAndPolicies();
        materialValidation();
        bindings();
        rawScalarPolicies();
        framing();
        std::cout << "Imported material worker contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

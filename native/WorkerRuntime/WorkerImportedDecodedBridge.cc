// SPDX-License-Identifier: MIT
#include "WorkerDecodedInternal.h"
#include <algorithm>
using namespace dust3d::worker;
namespace {
const ImportedRenderPayload* imported(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    const auto* value = std::get_if<ImportedRenderReply>(&reply->value);
    return value && value->value ? &*value->value : nullptr;
}
const char* identity(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size, bool component) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* value = imported(reply);
    if (!value || index >= (component ? value->surface.components.size() : value->surface.models.size()))
        return nullptr;
    const auto& key = component ? value->surface.components[index].id : value->surface.models[index];
    *size = key.size();
    return key.c_str();
}
}
extern "C" Dust3DWorkerImportedSummary dust3d_worker_decoded_imported_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* value = imported(reply);
    if (!value)
        return {};
    uint64_t pixels = 0;
    for (const auto& image : value->images)
        if (image)
            pixels += image->pixels.size();
    return { 1, value->surface.models.size(), value->surface.components.size(), value->surface.triangles.size(), pixels };
}
extern "C" Dust3DWorkerFailureDetail dust3d_worker_decoded_failure_detail(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return {};
    const auto* value = std::get_if<FailurePayload>(&reply->value);
    if (!value || !value->detail)
        return {};
    const auto& detail = *value->detail;
    return { 1, static_cast<uint16_t>(detail.domain), detail.code, detail.status, detail.causeTruncated };
}
extern "C" const char* dust3d_worker_decoded_imported_model(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept { return identity(reply, index, size, false); }
extern "C" const char* dust3d_worker_decoded_imported_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept { return identity(reply, index, size, true); }
extern "C" uint32_t dust3d_worker_decoded_imported_component_kind(const Dust3DWorkerDecodedReply* reply, size_t index) noexcept
{
    const auto* value = imported(reply);
    return value && index < value->surface.components.size() ? value->surface.components[index].kind : 0;
}
extern "C" int dust3d_worker_decoded_imported_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedTriangle* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = imported(reply);
    if (!value || index >= value->surface.triangles.size())
        return 0;
    const auto& triangle = value->surface.triangles[index];
    for (size_t corner = 0; corner < 3; ++corner) {
        std::copy(triangle.uv[corner].begin(), triangle.uv[corner].end(), output->uv[corner]);
        std::copy(triangle.colors[corner].begin(), triangle.colors[corner].end(), output->colors[corner]);
    }
    output->model_index = triangle.modelIndex;
    output->component_index = triangle.componentIndex;
    output->color_encoding = triangle.colorEncoding;
    output->uv_known = triangle.uvKnown;
    return 1;
}
extern "C" int dust3d_worker_decoded_imported_material(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedMaterial* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = imported(reply);
    if (!value || index >= value->materials.materials.size())
        return 0;
    const auto& material = value->materials.materials[index];
    output->model_id = material.modelId.c_str();
    output->model_id_size = material.modelId.size();
    std::copy(material.baseColor.begin(), material.baseColor.end(), output->base_color);
    output->metallic = material.metallic;
    output->roughness = material.roughness;
    std::copy(material.emissive.begin(), material.emissive.end(), output->emissive);
    output->alpha_mode = material.alphaMode;
    output->alpha_cutoff = material.alphaCutoff;
    output->is_double_sided = material.isDoubleSided;
    if (material.sampler) {
        const auto& sampler = *material.sampler;
        output->has_sampler = 1;
        output->has_magnification_filter = sampler.magnification.has_value();
        output->has_minification_filter = sampler.minification.has_value();
        output->magnification_filter = sampler.magnification.value_or(0);
        output->minification_filter = sampler.minification.value_or(0);
        output->wrap_s = sampler.wrapS;
        output->wrap_t = sampler.wrapT;
    }
    if (material.image) {
        output->has_image = 1;
        output->image_encoding = static_cast<uint32_t>(material.image->encoding);
        output->encoded = material.image->bytes.data();
        output->encoded_count = material.image->bytes.size();
    }
    return 1;
}
extern "C" int dust3d_worker_decoded_imported_image(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedImage* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = imported(reply);
    if (!value || index >= value->images.size())
        return 0;
    const auto& image = value->images[index];
    if (image)
        *output = { image->width, image->height, uint64_t(image->width) * 4, image->pixels.data(), image->pixels.size() };
    return 1;
}

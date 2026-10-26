// SPDX-License-Identifier: MIT
#include "WorkerDecodedInternal.h"
#include <algorithm>
using namespace dust3d::worker;
namespace {
const SourceBakedPayload* sourceBaked(const Dust3DWorkerDecodedReply* reply) noexcept
{
    return reply ? std::get_if<SourceBakedPayload>(&reply->value) : nullptr;
}
const LegacyBakedPayload* baked(const Dust3DWorkerDecodedReply* reply) noexcept
{
    return reply ? std::get_if<LegacyBakedPayload>(&reply->value) : nullptr;
}
const BakedTexturesPayload* textures(const Dust3DWorkerDecodedReply* reply) noexcept
{
    if (!reply)
        return nullptr;
    if (const auto* source = std::get_if<SourceBakedPayload>(&reply->value))
        return &source->textures;
    const auto* legacy = baked(reply);
    return legacy ? &legacy->textures : nullptr;
}

}
extern "C" Dust3DWorkerBakedSummary dust3d_worker_decoded_baked_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* value = textures(reply);
    if (!value)
        return {};
    uint64_t pixels = 0;
    for (const auto& map : value->maps)
        for (const auto& level : map)
            pixels += level.pixels.size();
    const auto* source = std::get_if<SourceBakedPayload>(&reply->value);
    return { source ? source->surface.triangles.size() : baked(reply)->tangents.size(), pixels, value->maximumSamplingLod };
}
extern "C" int dust3d_worker_decoded_baked_tangent(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerTextureTangent* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    if (reply) {
        if (const auto* source = std::get_if<SourceBakedPayload>(&reply->value)) {
            if (index >= source->surface.triangles.size())
                return 0;
            const auto& triangle = source->surface.triangles[index];
            std::copy(triangle.tangent.begin(), triangle.tangent.end(), output->direction);
            output->handedness = triangle.handedness;
            output->known = triangle.tangentKnown;
            return 1;
        }
    }
    const auto* value = baked(reply);
    if (!value || index >= value->tangents.size())
        return 0;
    const auto& source = value->tangents[index];
    std::copy(source.direction.begin(), source.direction.end(), output->direction);
    output->handedness = source.handedness;
    output->known = source.known;
    return 1;
}
extern "C" size_t dust3d_worker_decoded_baked_level_count(const Dust3DWorkerDecodedReply* reply, uint32_t role) noexcept
{
    const auto* value = textures(reply);
    return value && role < value->maps.size() ? value->maps[role].size() : 0;
}
extern "C" int dust3d_worker_decoded_baked_texture(const Dust3DWorkerDecodedReply* reply, uint32_t role, size_t level, Dust3DWorkerTextureView* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = textures(reply);
    if (!value || role >= value->maps.size() || level >= value->maps[role].size())
        return 0;
    const auto& source = value->maps[role][level];
    *output = { source.width, source.height, uint64_t(source.width) * 4, source.pixels.data(), source.pixels.size() };
    return 1;
}

extern "C" Dust3DWorkerBakedSurfaceSummary dust3d_worker_decoded_baked_surface_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* value = sourceBaked(reply);
    return value ? Dust3DWorkerBakedSurfaceSummary { value->surface.triangles.size(), value->surface.models.size(), value->surface.components.size() } : Dust3DWorkerBakedSurfaceSummary {};
}
extern "C" int dust3d_worker_decoded_baked_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerBakedTriangle* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = sourceBaked(reply);
    if (!value || index >= value->surface.triangles.size())
        return 0;
    const auto& source = value->surface.triangles[index];
    for (size_t corner = 0; corner < 3; ++corner)
        std::copy(source.uv[corner].begin(), source.uv[corner].end(), output->uv[corner]);
    output->alpha_cutoff = source.alphaCutoff;
    output->model_index = source.modelIndex;
    output->component_index = source.componentIndex;
    output->alpha_mode = source.alphaMode;
    output->uv_known = source.uvKnown;
    output->policy_known = source.policyKnown;
    output->is_double_sided = source.isDoubleSided;
    return 1;
}
extern "C" const char* dust3d_worker_decoded_baked_model(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* value = sourceBaked(reply);
    if (!value || index >= value->surface.models.size())
        return nullptr;
    const auto& key = value->surface.models[index];
    *size = key.size();
    return key.data();
}
extern "C" const char* dust3d_worker_decoded_baked_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) noexcept
{
    if (!size)
        return nullptr;
    *size = 0;
    const auto* value = sourceBaked(reply);
    if (!value || index >= value->surface.components.size())
        return nullptr;
    const auto& key = value->surface.components[index].id;
    *size = key.size();
    return key.data();
}
extern "C" uint32_t dust3d_worker_decoded_baked_component_kind(const Dust3DWorkerDecodedReply* reply, size_t index) noexcept
{
    const auto* value = sourceBaked(reply);
    return value && index < value->surface.components.size() ? value->surface.components[index].kind : 0;
}

extern "C" Dust3DWorkerFailureContext dust3d_worker_decoded_failure_context(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* failure = reply ? std::get_if<FailurePayload>(&reply->value) : nullptr;
    if (!failure || !failure->context)
        return {};
    const auto& context = *failure->context;
    return { static_cast<uint16_t>(context.role), context.identity.data(), context.identity.size() };
}

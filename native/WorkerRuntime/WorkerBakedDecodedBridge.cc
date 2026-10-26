// SPDX-License-Identifier: MIT
#include "WorkerDecodedInternal.h"
#include <algorithm>
using namespace dust3d::worker;
namespace {
const LegacyBakedPayload* baked(const Dust3DWorkerDecodedReply* reply) noexcept
{
    return reply ? std::get_if<LegacyBakedPayload>(&reply->value) : nullptr;
}
}
extern "C" Dust3DWorkerBakedSummary dust3d_worker_decoded_baked_summary(const Dust3DWorkerDecodedReply* reply) noexcept
{
    const auto* value = baked(reply);
    if (!value)
        return {};
    uint64_t pixels = 0;
    for (const auto& map : value->textures.maps)
        for (const auto& level : map)
            pixels += level.pixels.size();
    return { value->tangents.size(), pixels, value->textures.maximumSamplingLod };
}
extern "C" int dust3d_worker_decoded_baked_tangent(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerTextureTangent* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
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
    const auto* value = baked(reply);
    return value && role < value->textures.maps.size() ? value->textures.maps[role].size() : 0;
}
extern "C" int dust3d_worker_decoded_baked_texture(const Dust3DWorkerDecodedReply* reply, uint32_t role, size_t level, Dust3DWorkerTextureView* output) noexcept
{
    if (!output)
        return 0;
    *output = {};
    const auto* value = baked(reply);
    if (!value || role >= value->textures.maps.size() || level >= value->textures.maps[role].size())
        return 0;
    const auto& source = value->textures.maps[role][level];
    *output = { source.width, source.height, uint64_t(source.width) * 4, source.pixels.data(), source.pixels.size() };
    return 1;
}

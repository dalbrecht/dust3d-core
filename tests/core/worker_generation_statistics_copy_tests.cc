// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerGenerationStatisticsCopy.h>
using namespace dust3d::worker;
int main(int argc, char** argv)
{
    try {
        if (argc != 2)
            throw std::runtime_error("Existing positive fixture directory required");
        std::ifstream input(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        if (!input)
            throw std::runtime_error("Licensed Capsule fixture opens");
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        Dust3DError error {};
        std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> session(
            dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, &error), dust3d_generation_session_destroy);
        if (!session)
            throw std::runtime_error("Ordinary bounded native session opens");
        for (int iteration = 0; iteration < 2; ++iteration) {
            std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> mesh(
                dust3d_generation_session_load(session.get(), archive.data(), archive.size(), &error), dust3d_mesh_destroy);
            if (!mesh)
                throw std::runtime_error("Ordinary positive session generates");
            Dust3DGenerationStats native {};
            if (dust3d_generation_session_get_stats(session.get(), &native, sizeof(native), &error) != DUST3D_STATUS_OK)
                throw std::runtime_error("Actual native statistics query succeeds");
            const auto copy = copyNativeGenerationStatistics(native, {});
            if (copy.maximumCachePayloadBytes != native.maximum_cache_payload_bytes || copy.retainedPayloadBytes != native.retained_payload_bytes
                || copy.reusedComponents != native.reused_components || copy.rebuiltComponents != native.rebuilt_components
                || copy.rebuiltParts != native.rebuilt_parts || copy.cacheOutcome != native.cache_outcome || copy.cacheStatus != native.cache_status)
                throw std::runtime_error("Owned statistics must preserve actual per-call native observations");
            if (iteration == 1 && copy.reusedComponents == 0)
                throw std::runtime_error("Repeated ordinary Capsule generation must report actual component cache reuse");
            const auto wire = encodeGenerationStatistics(copy, {});
            if (encodeGenerationStatistics(decodeGenerationStatistics(wire.data(), wire.size(), {}), {}) != wire)
                throw std::runtime_error("Actual statistics survive independent codec ownership");
        }
        if (dust3d_generation_session_reset(session.get(), &error) != DUST3D_STATUS_OK)
            throw std::runtime_error("Ordinary explicit reset succeeds");
        Dust3DGenerationStats reset {};
        if (dust3d_generation_session_get_stats(session.get(), &reset, sizeof(reset), &error) != DUST3D_STATUS_OK
            || copyNativeGenerationStatistics(reset, {}).retainedPayloadBytes != 0)
            throw std::runtime_error("Actual reset retains measured empty cache state");
        Dust3DGenerationStats diagnostic {};
        diagnostic.maximum_cache_payload_bytes = 1024;
        diagnostic.cache_outcome = DUST3D_CACHE_BYPASSED;
        diagnostic.cache_status = DUST3D_STATUS_GENERATION_FAILED;
        diagnostic.cache_message[0] = 'x';
        diagnostic.cache_message[1] = static_cast<char>(255);
        const auto copiedIssue = copyNativeGenerationStatistics(diagnostic, {});
        if (copiedIssue.cacheStatus != DUST3D_STATUS_GENERATION_FAILED || copiedIssue.cacheCause != std::vector<uint8_t>({ 'x', 255 }))
            throw std::runtime_error("Copied cache-only issue preserves original status and opaque bytes independently of generation");
        bool quotaRejected = false;
        try {
            (void)copyNativeGenerationStatistics(diagnostic, { 65, 1 });
        } catch (const GenerationStatisticsError& cause) {
            quotaRejected = cause.code() == GenerationStatisticsErrorCode::limitExceeded;
        }
        if (!quotaRejected)
            throw std::runtime_error("Diagnostic copy quota is admitted before retaining original bytes");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

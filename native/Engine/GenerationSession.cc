#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include "SnapshotInvalidation.h"
#include "SourceProvenance.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dust3d/base/retained_payload_budget.h>

namespace {
struct Asset {
    std::string type;
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<uint8_t> payload;
    void accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept
    {
        const auto& [assetType, assetName, assetAttributes, assetPayload] = *this;
        budget.accountDynamicFields(assetType, assetName, assetAttributes, assetPayload);
    }
};

struct AssetView {
    Asset metadata;
    const uint8_t* data = nullptr;
    size_t size = 0;
};

// The optional revision collector cannot turn a cache allocation failure into a
// primary document failure. Payload spans never survive the current load call.
class AssetCollector final : public dust3d::native::DocumentAssetObserver {
public:
    std::vector<AssetView> assets;
    Dust3DError error {};

    void item(std::string_view type, std::string_view name, const uint8_t* data, size_t size) noexcept override
    {
        if (error.status != DUST3D_STATUS_OK)
            return;
        try {
            assets.push_back({ { std::string(type), std::string(name), {}, {} }, data, size });
        } catch (const std::exception& exception) {
            dust3d::native::setError(&error, DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
        } catch (...) {
            dust3d::native::setError(&error, DUST3D_STATUS_GENERATION_FAILED, "Unknown asset revision collection failure.");
        }
    }
    void attribute(std::string_view name, std::string_view value) noexcept override
    {
        if (error.status != DUST3D_STATUS_OK)
            return;
        try {
            assets.back().metadata.attributes.emplace_back(name, value);
        } catch (const std::exception& exception) {
            dust3d::native::setError(&error, DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
        } catch (...) {
            dust3d::native::setError(&error, DUST3D_STATUS_GENERATION_FAILED, "Unknown asset attribute collection failure.");
        }
    }
    void normalize()
    {
        for (auto& asset : assets)
            std::sort(asset.metadata.attributes.begin(), asset.metadata.attributes.end());
        std::sort(assets.begin(), assets.end(), [](const auto& left, const auto& right) { return left.metadata.name < right.metadata.name; });
    }
};

struct CacheState {
    dust3d::MeshGenerator::GeneratedCacheContext context;
    std::unique_ptr<dust3d::Snapshot> previous;
    std::vector<Asset> assets;
    void accountDynamicPayload(dust3d::RetainedPayloadBudget& budget) const noexcept
    {
        const auto& [generatedContext, previousSource, ownedAssets] = *this;
        budget.accountDynamicFields(generatedContext, previousSource, ownedAssets);
    }
};

bool sameAssets(const std::vector<Asset>& retained, const std::vector<AssetView>& current)
{
    if (retained.size() != current.size())
        return false;
    for (size_t index = 0; index < retained.size(); ++index) {
        const auto& old = retained[index];
        const auto& view = current[index];
        if (old.type != view.metadata.type || old.name != view.metadata.name || old.attributes != view.metadata.attributes
            || old.payload.size() != view.size || !std::equal(old.payload.begin(), old.payload.end(), view.data))
            return false;
    }
    return true;
}
}

struct Dust3DGenerationSession {
    Dust3DGenerationStats stats {};
    std::unique_ptr<CacheState> cache;

    explicit Dust3DGenerationSession(size_t maximumBytes) { stats.maximum_cache_payload_bytes = maximumBytes; }

    void clearStats() noexcept
    {
        const auto maximum = stats.maximum_cache_payload_bytes;
        stats = {};
        stats.maximum_cache_payload_bytes = maximum;
    }
    void cacheIssue(Dust3DStatus status, const char* message) noexcept
    {
        cache.reset();
        stats.retained_payload_bytes = 0;
        stats.cache_outcome = DUST3D_CACHE_BYPASSED;
        stats.cache_status = status;
        std::snprintf(stats.cache_message, sizeof(stats.cache_message), "%s", message);
    }
    void fail() noexcept
    {
        cache.reset();
        stats.retained_payload_bytes = 0;
        stats.cache_outcome = DUST3D_CACHE_RESET_AFTER_FAILURE;
    }

    Dust3DMesh* load(const uint8_t* bytes, size_t size, std::unique_ptr<dust3d::native::SourceProvenance>* sources = nullptr, size_t maximumSourceBytes = 0)
    {
        clearStats();
        AssetCollector collector;
        std::string warning;
        dust3d::native::ImportedModels imports;
        auto source = dust3d::native::readDocument(bytes, size, warning,
            stats.maximum_cache_payload_bytes ? &collector : nullptr, &imports);
        if (sources)
            *sources = std::make_unique<dust3d::native::SourceProvenance>(*source, maximumSourceBytes);
        std::unique_ptr<dust3d::Snapshot> transient;
        if (!stats.maximum_cache_payload_bytes) {
            cache.reset();
            stats.cache_outcome = DUST3D_CACHE_DISABLED;
        } else if (collector.error.status != DUST3D_STATUS_OK) {
            cacheIssue(collector.error.status, collector.error.message);
        } else {
            try {
                collector.normalize();
                if (cache && !sameAssets(cache->assets, collector.assets))
                    cache.reset();
                if (!cache) {
                    cache = std::make_unique<CacheState>();
                    // Preflight asset bytes before copying even one large opaque item.
                    dust3d::RetainedPayloadBudget budget(stats.maximum_cache_payload_bytes);
                    budget.accountObject(*cache);
                    budget.accountObject(*source);
                    for (const auto& asset : collector.assets) {
                        budget.accountObject(asset.metadata);
                        budget.tryAdd(asset.size);
                    }
                    if (budget.isExceeded())
                        throw dust3d::native::DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Asset revision payload exceeds the session cache budget.");
                    for (const auto& view : collector.assets) {
                        Asset owned = view.metadata;
                        owned.payload.assign(view.data, view.data + view.size);
                        cache->assets.push_back(std::move(owned));
                    }
                }
                const auto invalidation = dust3d::native::deriveSnapshotInvalidation(cache->previous.get(), *source);
                transient = std::make_unique<dust3d::Snapshot>(*source);
                for (auto& part : transient->parts)
                    part.second["__dirty"] = invalidation.dirtyPartIds.count(part.first) ? "true" : "false";
                for (auto& component : transient->components)
                    component.second["__dirty"] = invalidation.dirtyComponentIds.count(component.first) ? "true" : "false";
                transient->rootComponent["__dirty"] = invalidation.isRootDirty ? "true" : "false";
            } catch (const dust3d::native::DocumentError& exception) {
                cacheIssue(exception.status(), exception.what());
            } catch (const std::bad_alloc& exception) {
                cacheIssue(DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
            } catch (const std::exception& exception) {
                cacheIssue(DUST3D_STATUS_GENERATION_FAILED, exception.what());
            } catch (...) {
                cacheIssue(DUST3D_STATUS_GENERATION_FAILED, "Unknown cache preparation failure.");
            }
        }

        dust3d::MeshGenerator::GenerationMetrics metrics;
        std::unique_ptr<Dust3DMesh> mesh;
        bool isEmpty = false;
        try {
            mesh = dust3d::native::generateMesh(cache ? std::move(transient) : std::move(source), warning, &imports,
                cache ? &cache->context : nullptr, &metrics, sources ? sources->get() : nullptr);
        } catch (const dust3d::native::DocumentError& exception) {
            if (exception.status() != DUST3D_STATUS_NO_GEOMETRY)
                throw;
            isEmpty = true;
        }
        stats.reused_components = metrics.reusedComponents;
        stats.rebuilt_components = metrics.rebuiltComponents;
        stats.rebuilt_parts = metrics.rebuiltParts;
        if (cache) {
            cache->previous = std::move(source);
            dust3d::RetainedPayloadBudget budget(stats.maximum_cache_payload_bytes);
            budget.accountObject(*cache);
            if (budget.isExceeded()) {
                cacheIssue(DUST3D_STATUS_LIMIT_EXCEEDED, "Generated cache payload exceeds the session cache budget.");
            } else {
                stats.retained_payload_bytes = budget.usedBytes();
                stats.cache_outcome = DUST3D_CACHE_RETAINED;
            }
        }
        if (isEmpty)
            throw dust3d::native::DocumentError(DUST3D_STATUS_NO_GEOMETRY, "Generation completed with no mesh-producing geometry.");
        return mesh.release();
    }
};

Dust3DGenerationSession* dust3d_generation_session_create(size_t maximumBytes, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    if (maximumBytes > DUST3D_MAX_CACHE_PAYLOAD_BYTES) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "The session cache budget exceeds 256 MiB.");
        return nullptr;
    }
    try {
        return new Dust3DGenerationSession(maximumBytes);
    } catch (const std::bad_alloc& exception) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Cannot create a generation session.");
    }
    return nullptr;
}

void dust3d_generation_session_destroy(Dust3DGenerationSession* session) { delete session; }

namespace {
template <typename Output, typename Operation>
Output* loadSession(Dust3DGenerationSession* session, Dust3DError* error, Operation&& operation)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    if (!session) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "A generation session is required.");
        return nullptr;
    }
    try {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        try {
            return operation();
        } catch (const dust3d::native::DocumentError& exception) {
            if (exception.status() != DUST3D_STATUS_NO_GEOMETRY)
                session->fail();
            dust3d::native::setError(error, exception.status(), exception.what());
        } catch (const std::bad_alloc& exception) {
            session->fail();
            dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, exception.what());
        } catch (const std::exception& exception) {
            session->fail();
            dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, exception.what());
        } catch (...) {
            session->fail();
            dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Unknown generation session failure.");
        }
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Cannot lock the modeling engine.");
    }
    return nullptr;
}

}

Dust3DMesh* dust3d_generation_session_load(Dust3DGenerationSession* session, const uint8_t* bytes, size_t size, Dust3DError* error)
{
    return loadSession<Dust3DMesh>(session, error, [&] { return session->load(bytes, size); });
}

Dust3DSourcedMesh* dust3d_generation_session_load_sourced(Dust3DGenerationSession* session, const uint8_t* bytes, size_t size, const Dust3DSourceOptions* options, Dust3DError* error)
{
    const auto maximumBytes = dust3d::native::sourcePayloadLimit(options, error);
    if (!maximumBytes)
        return nullptr;
    return loadSession<Dust3DSourcedMesh>(session, error, [&] {
        std::unique_ptr<dust3d::native::SourceProvenance> sources;
        std::unique_ptr<Dust3DMesh> geometry(session->load(bytes, size, &sources, maximumBytes));
        auto owner = std::make_unique<Dust3DSourcedMesh>();
        owner->geometry = std::move(geometry);
        owner->sources = std::move(sources);
        return owner.release();
    });
}

Dust3DStatus dust3d_generation_session_reset(Dust3DGenerationSession* session, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    if (!session) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "A generation session is required.");
        return DUST3D_STATUS_INVALID_DOCUMENT;
    }
    try {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        session->cache.reset();
        session->clearStats();
        return DUST3D_STATUS_OK;
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Cannot lock the modeling engine.");
        return DUST3D_STATUS_GENERATION_FAILED;
    }
}

Dust3DStatus dust3d_generation_session_get_stats(const Dust3DGenerationSession* session, Dust3DGenerationStats* output, size_t capacity, Dust3DError* error)
{
    dust3d::native::setError(error, DUST3D_STATUS_OK, "");
    if (!session || !output) {
        dust3d::native::setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "A session and writable stats output are required.");
        return DUST3D_STATUS_INVALID_DOCUMENT;
    }
    if (capacity < sizeof(*output)) {
        dust3d::native::setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "The stats output buffer is too short.");
        return DUST3D_STATUS_LIMIT_EXCEEDED;
    }
    try {
        std::lock_guard<std::mutex> lock(dust3d::native::engineMutex());
        *output = session->stats;
        return DUST3D_STATUS_OK;
    } catch (...) {
        dust3d::native::setError(error, DUST3D_STATUS_GENERATION_FAILED, "Cannot lock the modeling engine.");
        return DUST3D_STATUS_GENERATION_FAILED;
    }
}

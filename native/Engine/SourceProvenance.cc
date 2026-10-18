#include "SourceProvenance.h"
#include "GenerationPipeline.h"
#include "ImportedModels.h"
#include <cmath>
#include <dust3d/base/object.h>
#include <limits>

namespace dust3d {
namespace native {
    size_t sourcePayloadLimit(const Dust3DSourceOptions* options, Dust3DError* error) noexcept
    {
        setError(error, DUST3D_STATUS_OK, "");
        if (!options || options->struct_size != sizeof(Dust3DSourceOptions)) {
            setError(error, DUST3D_STATUS_INVALID_DOCUMENT, "Source generation requires correctly sized options.");
            return 0;
        }
        if (options->maximum_payload_bytes > DUST3D_MAX_SOURCE_PAYLOAD_BYTES) {
            setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "The source payload budget exceeds 256 MiB.");
            return 0;
        }
        return options->maximum_payload_bytes == 0 ? DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES : options->maximum_payload_bytes;
    }

    SourceProvenance::SourceProvenance(const Snapshot& snapshot, size_t maximumBytes)
        : m_maximumBytes(maximumBytes)
    {
        const std::string empty;
        for (const auto& [key, attributes] : snapshot.nodes) {
            const auto part = attributes.find("partId");
            const auto& partKey = part == attributes.end() ? empty : part->second;
            charge(sizeof(SourceIdentity));
            charge(key.size());
            charge(1);
            charge(partKey.size());
            charge(1);
            const auto index = m_identities.size();
            m_identities.push_back({ key, partKey });
            const Uuid identity(key);
            if (identity.isNull())
                continue;
            auto [found, inserted] = m_identityIndices.emplace(identity, index);
            if (!inserted)
                found->second = SIZE_MAX;
        }
    }

    void SourceProvenance::charge(size_t bytes)
    {
        if (bytes > m_maximumBytes - m_payloadBytes)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "The source provenance payload exceeds its configured budget.");
        m_payloadBytes += bytes;
    }

    Dust3DSourceInfluence SourceProvenance::resolve(const Uuid& node, float weight) const
    {
        if (!std::isfinite(weight) || weight < 0)
            throw DocumentError(DUST3D_STATUS_GENERATION_FAILED, "The engine produced an invalid source association weight.");
        const auto found = m_identityIndices.find(node);
        if (found == m_identityIndices.end())
            return { DUST3D_SOURCE_UNKNOWN, SIZE_MAX, weight };
        if (found->second == SIZE_MAX)
            return { DUST3D_SOURCE_AMBIGUOUS, SIZE_MAX, weight };
        return { DUST3D_SOURCE_NODE_HINT, found->second, weight };
    }

    void SourceProvenance::appendCorner(const Object& object, const Vector3& position)
    {
        Dust3DSourceCorner corner { DUST3D_SOURCE_UNKNOWN, m_influences.size(), 0 };
        charge(sizeof(corner));
        bool canQuantize = true;
        const double exclusiveUpperBound = std::ldexp(1.0, std::numeric_limits<long>::digits);
        for (size_t axis = 0; axis < 3; ++axis) {
            // Match PositionKey's double multiplication, but never perform an
            // out-of-range floating-to-integer conversion at this new boundary.
            const double scaled = position[axis] * 100000;
            canQuantize = canQuantize && std::isfinite(scaled)
                && scaled >= -exclusiveUpperBound && scaled < exclusiveUpperBound;
        }
        if (canQuantize) {
            const PositionKey key(position);
            const auto weights = object.positionToNodeWeights.find(key);
            if (weights != object.positionToNodeWeights.end() && !weights->second.empty()) {
                corner.kind = DUST3D_SOURCE_WEIGHTED_HINT;
                for (const auto& [node, weight] : weights->second) {
                    const auto influence = resolve(node, weight);
                    charge(sizeof(influence));
                    m_influences.push_back(influence);
                    ++corner.influence_count;
                }
            } else {
                const auto node = object.positionToNodeIdMap.find(key);
                if (node != object.positionToNodeIdMap.end()) {
                    const auto influence = resolve(node->second, 1);
                    charge(sizeof(influence));
                    m_influences.push_back(influence);
                    corner.kind = influence.kind;
                    corner.influence_count = 1;
                }
            }
        }
        m_corners.push_back(corner);
    }
}
}

Dust3DSourcedMesh* dust3d_sourced_mesh_load(const uint8_t* bytes, size_t size, const Dust3DSourceOptions* options, Dust3DError* error)
{
    using namespace dust3d::native;
    const auto maximumBytes = sourcePayloadLimit(options, error);
    if (!maximumBytes)
        return nullptr;
    try {
        std::lock_guard<std::mutex> lock(engineMutex());
        std::string warning;
        ImportedModels imports;
        auto snapshot = readDocument(bytes, size, warning, nullptr, &imports);
        auto owner = std::make_unique<Dust3DSourcedMesh>();
        owner->sources = std::make_unique<SourceProvenance>(*snapshot, maximumBytes);
        owner->geometry = generateMesh(std::move(snapshot), warning, &imports, nullptr, nullptr, owner->sources.get());
        return owner.release();
    } catch (const DocumentError& exception) {
        setError(error, exception.status(), exception.what());
    } catch (const std::bad_alloc&) {
        setError(error, DUST3D_STATUS_LIMIT_EXCEEDED, "Not enough memory to export mesh source associations.");
    } catch (const std::exception& exception) {
        setError(error, DUST3D_STATUS_INVALID_DOCUMENT, exception.what());
    } catch (...) {
        setError(error, DUST3D_STATUS_GENERATION_FAILED, "An unknown error occurred while exporting mesh source associations.");
    }
    return nullptr;
}

void dust3d_sourced_mesh_destroy(Dust3DSourcedMesh* mesh) { delete mesh; }
const Dust3DMesh* dust3d_sourced_mesh_geometry(const Dust3DSourcedMesh* mesh) { return mesh ? mesh->geometry.get() : nullptr; }
const Dust3DSourceCorner* dust3d_sourced_mesh_corners(const Dust3DSourcedMesh* mesh) { return mesh && !mesh->sources->corners().empty() ? mesh->sources->corners().data() : nullptr; }
const Dust3DSourceInfluence* dust3d_sourced_mesh_influences(const Dust3DSourcedMesh* mesh) { return mesh && !mesh->sources->influences().empty() ? mesh->sources->influences().data() : nullptr; }
size_t dust3d_sourced_mesh_corner_count(const Dust3DSourcedMesh* mesh) { return mesh ? mesh->sources->corners().size() : 0; }
size_t dust3d_sourced_mesh_influence_count(const Dust3DSourcedMesh* mesh) { return mesh ? mesh->sources->influences().size() : 0; }
size_t dust3d_sourced_mesh_identity_count(const Dust3DSourcedMesh* mesh) { return mesh ? mesh->sources->identities().size() : 0; }
const char* dust3d_sourced_mesh_node_id(const Dust3DSourcedMesh* mesh, size_t index) { return mesh && index < mesh->sources->identities().size() ? mesh->sources->identities()[index].node.c_str() : ""; }
const char* dust3d_sourced_mesh_part_id(const Dust3DSourcedMesh* mesh, size_t index) { return mesh && index < mesh->sources->identities().size() ? mesh->sources->identities()[index].part.c_str() : ""; }
size_t dust3d_sourced_mesh_payload_bytes(const Dust3DSourcedMesh* mesh) { return mesh ? mesh->sources->payloadBytes() : 0; }

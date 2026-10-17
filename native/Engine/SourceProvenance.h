#ifndef DUST3D_NATIVE_SOURCE_PROVENANCE_H_
#define DUST3D_NATIVE_SOURCE_PROVENANCE_H_

#include "EngineInternal.h"
#include <dust3d/base/uuid.h>
#include <map>

namespace dust3d {
class Object;
class Vector3;
namespace native {
    struct SourceIdentity {
        std::string node;
        std::string part;
    };

    // Membership is captured before generator preprocessing mutates its snapshot.
    class SourceProvenance {
    public:
        SourceProvenance(const Snapshot& snapshot, size_t maximumBytes);
        void appendCorner(const Object& object, const Vector3& position);
        const std::vector<SourceIdentity>& identities() const { return m_identities; }
        const std::vector<Dust3DSourceCorner>& corners() const { return m_corners; }
        const std::vector<Dust3DSourceInfluence>& influences() const { return m_influences; }
        size_t payloadBytes() const { return m_payloadBytes; }

    private:
        void charge(size_t bytes);
        Dust3DSourceInfluence resolve(const Uuid& node, float weight) const;
        std::vector<SourceIdentity> m_identities;
        std::vector<Dust3DSourceCorner> m_corners;
        std::vector<Dust3DSourceInfluence> m_influences;
        std::map<Uuid, size_t> m_identityIndices;
        size_t m_maximumBytes;
        size_t m_payloadBytes = 0;
    };
}
}
#endif

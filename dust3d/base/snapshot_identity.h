// SPDX-License-Identifier: MIT
#ifndef DUST3D_BASE_SNAPSHOT_IDENTITY_H_
#define DUST3D_BASE_SNAPSHOT_IDENTITY_H_

#include <dust3d/base/uuid.h>
#include <map>
#include <string>
#include <vector>

namespace dust3d {

/// Resolves inherited UUID spellings without renaming authored snapshot keys.
/// Owns its keys and borrows no collection. Rebuild after changing the key set
/// before using alias fallback; exact lookup always consults the live collection.
class SnapshotIdentityIndex {
public:
    using Elements = std::map<std::string, std::map<std::string, std::string>>;

    explicit SnapshotIdentityIndex(const Elements& elements)
    {
        for (const auto& element : elements) {
            const Uuid identity(element.first);
            if (!identity.isNull())
                m_aliases[identity].push_back(element.first);
        }
    }

    template <typename Iterator>
    struct Result {
        Iterator element;
        bool ambiguous;
    };

    /// Exact spelling wins. Missing or ambiguous fallback never chooses an owner.
    /// The returned iterator belongs to liveElements, not the indexed collection.
    template <typename Map>
    auto find(Map& liveElements, const std::string& reference) const -> Result<decltype(liveElements.end())>
    {
        const auto exact = liveElements.find(reference);
        if (exact != liveElements.end())
            return { exact, false };
        const Uuid identity(reference);
        if (identity.isNull())
            return { liveElements.end(), false };
        const auto alias = m_aliases.find(identity);
        if (alias == m_aliases.end())
            return { liveElements.end(), false };
        if (alias->second.size() != 1)
            return { liveElements.end(), true };
        return { liveElements.find(alias->second.front()), false };
    }

    template <typename Map>
    auto find(Map& liveElements, const Uuid& reference) const -> Result<decltype(liveElements.end())>
    {
        return find(liveElements, reference.toString());
    }

private:
    std::map<Uuid, std::vector<std::string>> m_aliases;
};

}
#endif

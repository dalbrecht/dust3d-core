// SPDX-License-Identifier: MIT
#include "WorkerSourceHints.h"
#include "WorkerSourceHintValidation.h"
#include "WorkerWire.h"
#include <cmath>
#include <cstring>

namespace dust3d {
namespace worker {
    namespace {
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "Source wire weights require IEEE-754 binary32.");
        [[noreturn]] void reject(SourceHintPayloadErrorCode code, const char* cause)
        {
            throw SourceHintPayloadError(code, cause);
        }
        void charge(uint64_t& total, uint64_t count, uint64_t unit, uint64_t maximum)
        {
            if (total > maximum || count > (maximum - total) / unit)
                reject(SourceHintPayloadErrorCode::limitExceeded, "Worker source payload exceeds its aggregate quota.");
            total += count * unit;
        }
        struct Plan {
            size_t bytes, cornersOffset, influencesOffset;
            uint64_t decoded;
        };
        Plan plan(uint64_t identities, uint64_t corners, uint64_t influences, uint64_t text, const SourceHintPayloadLimits& limits)
        {
            validateSourceHintPayloadLimits(limits);
            if (!corners || corners % 3)
                reject(SourceHintPayloadErrorCode::invalidCorner, "Worker sources require complete nonempty triangle corners.");
            if (identities > limits.maximumIdentities || corners > limits.maximumCorners || influences > limits.maximumInfluences || text > limits.maximumTextBytes)
                reject(SourceHintPayloadErrorCode::limitExceeded, "Worker source counts exceed configured limits.");
            uint64_t bytes = sourceHintPrefixBytes;
            charge(bytes, identities, 8, limits.maximumWireBytes);
            charge(bytes, text, 1, limits.maximumWireBytes);
            const auto cornersOffset = bytes;
            charge(bytes, corners, sourceHintCornerBytes, limits.maximumWireBytes);
            const auto influencesOffset = bytes;
            charge(bytes, influences, sourceHintInfluenceBytes, limits.maximumWireBytes);
            uint64_t decoded = 0;
            charge(decoded, identities, sizeof(SourceHintIdentity) + 2, limits.maximumDecodedBytes);
            charge(decoded, text, 1, limits.maximumDecodedBytes);
            charge(decoded, corners, sizeof(SourceHintCorner), limits.maximumDecodedBytes);
            charge(decoded, influences, sizeof(SourceHintInfluence), limits.maximumDecodedBytes);
            if (bytes > std::numeric_limits<size_t>::max() || decoded > std::numeric_limits<size_t>::max())
                reject(SourceHintPayloadErrorCode::limitExceeded, "Worker source values cannot fit the address space.");
            return { static_cast<size_t>(bytes), static_cast<size_t>(cornersOffset), static_cast<size_t>(influencesOffset), decoded };
        }
        Plan validateOwned(const SourceHintPayload& value, const SourceHintPayloadLimits& limits)
        {
            validateSourceHintPayloadLimits(limits);
            if (value.identities.size() > limits.maximumIdentities || value.corners.size() > limits.maximumCorners || value.influences.size() > limits.maximumInfluences)
                reject(SourceHintPayloadErrorCode::limitExceeded, "Worker source counts exceed configured limits.");
            uint64_t text = 0;
            for (const auto& item : value.identities) {
                detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(item.node.data()), item.node.size(), limits.maximumIdentityBytes);
                detail::validateSourceHintIdentity(reinterpret_cast<const uint8_t*>(item.part.data()), item.part.size(), limits.maximumIdentityBytes);
                charge(text, item.node.size(), 1, limits.maximumTextBytes);
                charge(text, item.part.size(), 1, limits.maximumTextBytes);
            }
            const auto layout = plan(value.identities.size(), value.corners.size(), value.influences.size(), text, limits);
            for (const auto& item : value.influences)
                detail::validateSourceHintInfluence(item, value.identities.size());
            for (const auto& item : value.corners)
                detail::validateSourceHintCorner(item, value.influences.size(), [&](uint64_t index) { return value.influences.at(static_cast<size_t>(index)).kind; });
            return layout;
        }
        struct SourceWireView {
            Plan layout;
            uint64_t identities, corners, influences;
        };
        SourceWireView describeSourceHints(const uint8_t* bytes, size_t size, const SourceHintPayloadLimits& limits)
        {
            validateSourceHintPayloadLimits(limits);
            if (!bytes || size < sourceHintPrefixBytes)
                reject(SourceHintPayloadErrorCode::invalidInput, "Worker source prefix is unavailable.");
            if (detail::readUnsigned(bytes, 4) != 1)
                reject(SourceHintPayloadErrorCode::unsupportedVersion, "Worker source version is unsupported.");
            if (detail::readUnsigned(bytes + 4, 4))
                reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source reserved prefix is nonzero.");
            const auto identities = detail::readUnsigned(bytes + 8, 8);
            const auto corners = detail::readUnsigned(bytes + 16, 8);
            const auto influences = detail::readUnsigned(bytes + 24, 8);
            const auto layout = plan(identities, corners, influences, detail::readUnsigned(bytes + 32, 8), limits);
            if (size != layout.bytes)
                reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source span is truncated or has trailing bytes.");
            size_t cursor = sourceHintPrefixBytes;
            for (uint64_t index = 0; index < identities; ++index) {
                if (cursor > layout.cornersOffset || 8 > layout.cornersOffset - cursor)
                    reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source identity lengths exceed their span.");
                const auto node = detail::readUnsigned(bytes + cursor, 4);
                const auto part = detail::readUnsigned(bytes + cursor + 4, 4);
                cursor += 8;
                if (node > layout.cornersOffset - cursor || part > layout.cornersOffset - cursor - node)
                    reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source identity text exceeds its span.");
                detail::validateSourceHintIdentity(bytes + cursor, static_cast<size_t>(node), limits.maximumIdentityBytes);
                cursor += static_cast<size_t>(node);
                detail::validateSourceHintIdentity(bytes + cursor, static_cast<size_t>(part), limits.maximumIdentityBytes);
                cursor += static_cast<size_t>(part);
            }
            if (cursor != layout.cornersOffset)
                reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source identity text total is inconsistent.");
            for (uint64_t index = 0; index < influences; ++index) {
                const auto* item = bytes + layout.influencesOffset + static_cast<size_t>(index) * sourceHintInfluenceBytes;
                if (detail::readUnsigned(item + 4, 4) || detail::readUnsigned(item + 20, 4))
                    reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source influence reserved fields are nonzero.");
                detail::validateSourceHintInfluence(detail::readSourceHintInfluence(item), identities);
            }
            for (uint64_t index = 0; index < corners; ++index) {
                const auto* item = bytes + layout.cornersOffset + static_cast<size_t>(index) * sourceHintCornerBytes;
                if (detail::readUnsigned(item + 4, 4))
                    reject(SourceHintPayloadErrorCode::invalidLayout, "Worker source corner reserved field is nonzero.");
                detail::validateSourceHintCorner(detail::readSourceHintCorner(item), influences, [&](uint64_t influenceIndex) {
                    return static_cast<SourceHintKind>(detail::readUnsigned(bytes + layout.influencesOffset + static_cast<size_t>(influenceIndex) * sourceHintInfluenceBytes, 4));
                });
            }
            return { layout, identities, corners, influences };
        }

    }

    SourceHintPayloadError::SourceHintPayloadError(SourceHintPayloadErrorCode code, const char* cause)
        : std::runtime_error(cause)
        , m_code(code)
    {
    }

    void validateSourceHintPayloadLimits(const SourceHintPayloadLimits& limits)
    {
        if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
            || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
            || !limits.maximumIdentities || limits.maximumIdentities > 1000000
            || !limits.maximumCorners || limits.maximumCorners > maximumPayloadBytes / sourceHintCornerBytes
            || !limits.maximumInfluences || limits.maximumInfluences > maximumPayloadBytes / sourceHintInfluenceBytes
            || !limits.maximumTextBytes || limits.maximumTextBytes > maximumPayloadBytes
            || !limits.maximumIdentityBytes || limits.maximumIdentityBytes > 1024)
            reject(SourceHintPayloadErrorCode::invalidLimits, "Worker source limits are outside supported positive bounds.");
    }

    SourceHintPayloadSize measureSourceHintPayload(const SourceHintPayload& value, const SourceHintPayloadLimits& limits)
    {
        const auto layout = validateOwned(value, limits);
        return { layout.bytes, layout.decoded };
    }

    std::vector<uint8_t> encodeSourceHintPayload(const SourceHintPayload& value, const SourceHintPayloadLimits& limits)
    {
        const auto layout = validateOwned(value, limits);
        std::vector<uint8_t> bytes(layout.bytes, 0);
        detail::writeUnsigned(bytes.data(), 1, 4);
        detail::writeUnsigned(bytes.data() + 8, value.identities.size(), 8);
        detail::writeUnsigned(bytes.data() + 16, value.corners.size(), 8);
        detail::writeUnsigned(bytes.data() + 24, value.influences.size(), 8);
        detail::writeUnsigned(bytes.data() + 32, layout.cornersOffset - sourceHintPrefixBytes - value.identities.size() * 8, 8);
        size_t cursor = sourceHintPrefixBytes;
        for (const auto& item : value.identities) {
            detail::writeUnsigned(bytes.data() + cursor, item.node.size(), 4);
            detail::writeUnsigned(bytes.data() + cursor + 4, item.part.size(), 4);
            cursor += 8;
            std::memcpy(bytes.data() + cursor, item.node.data(), item.node.size());
            cursor += item.node.size();
            std::memcpy(bytes.data() + cursor, item.part.data(), item.part.size());
            cursor += item.part.size();
        }
        for (const auto& item : value.corners) {
            detail::writeUnsigned(bytes.data() + cursor, static_cast<uint32_t>(item.kind), 4);
            detail::writeUnsigned(bytes.data() + cursor + 8, item.firstInfluence, 8);
            detail::writeUnsigned(bytes.data() + cursor + 16, item.influenceCount, 8);
            cursor += sourceHintCornerBytes;
        }
        for (const auto& item : value.influences) {
            detail::writeUnsigned(bytes.data() + cursor, static_cast<uint32_t>(item.kind), 4);
            detail::writeUnsigned(bytes.data() + cursor + 8, item.identityIndex, 8);
            uint32_t bits = 0;
            std::memcpy(&bits, &item.weight, sizeof(bits));
            detail::writeUnsigned(bytes.data() + cursor + 16, bits, 4);
            cursor += sourceHintInfluenceBytes;
        }
        return bytes;
    }

    SourceHintPayloadInspection inspectSourceHintPayload(const uint8_t* bytes, size_t size, const SourceHintPayloadLimits& limits)
    {
        const auto view = describeSourceHints(bytes, size, limits);
        return { view.identities, view.corners, view.influences, view.layout.decoded };
    }

    SourceHintPayload decodeSourceHintPayload(const uint8_t* bytes, size_t size, const SourceHintPayloadLimits& limits)
    {
        const auto view = describeSourceHints(bytes, size, limits);
        const auto& layout = view.layout;
        const auto identities = view.identities, corners = view.corners, influences = view.influences;
        // Every quota/span/reference has been admitted before retaining output arrays or strings.
        SourceHintPayload result;
        result.identities.reserve(static_cast<size_t>(identities));
        result.corners.reserve(static_cast<size_t>(corners));
        result.influences.reserve(static_cast<size_t>(influences));
        size_t cursor = sourceHintPrefixBytes;
        for (uint64_t index = 0; index < identities; ++index) {
            const auto node = static_cast<size_t>(detail::readUnsigned(bytes + cursor, 4));
            const auto part = static_cast<size_t>(detail::readUnsigned(bytes + cursor + 4, 4));
            cursor += 8;
            result.identities.push_back({ std::string(reinterpret_cast<const char*>(bytes + cursor), node), std::string(reinterpret_cast<const char*>(bytes + cursor + node), part) });
            cursor += node + part;
        }
        for (uint64_t index = 0; index < corners; ++index)
            result.corners.push_back(detail::readSourceHintCorner(bytes + layout.cornersOffset + static_cast<size_t>(index) * sourceHintCornerBytes));
        for (uint64_t index = 0; index < influences; ++index)
            result.influences.push_back(detail::readSourceHintInfluence(bytes + layout.influencesOffset + static_cast<size_t>(index) * sourceHintInfluenceBytes));
        return result;
    }
}
}

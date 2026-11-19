// SPDX-License-Identifier: MIT
// Synthetic metadata only: no generation, asset decoding, writer or fault probes.
#include <dust3d/base/snapshot_identity.h>
#include <dust3d/base/uuid.h>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {
using Elements = std::map<std::string, std::map<std::string, std::string>>;
const std::string bare = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const std::string braced = "{" + bare + "}";
const std::string third = "{" + bare + "!";

void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

void computedUuidFindsAuthoredBareBracedAndInheritedOpaqueKeys()
{
    for (const auto& key : { bare, braced, std::string("qqqqqqqq-qqqq-4qqq-8qqq-qqqqqqqqqqqq") }) {
        const Elements elements { { key, { { "id", key }, { "future", "retained" } } } };
        const dust3d::SnapshotIdentityIndex index(elements);
        const auto result = index.find(elements, dust3d::Uuid(key));
        require(!result.ambiguous && result.element != elements.end(), "Computed UUID did not find its unique authored key");
        require(result.element->first == key && result.element->second.at("id") == key, "Lookup renamed an authored key or id attribute");
        require(result.element->second.at("future") == "retained", "Lookup changed opaque attributes");
    }
}

void exactMatchWinsWithoutCaseFoldingOrOpaqueIdentityCoercion()
{
    const std::string upper = "AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA";
    const Elements elements {
        { bare, { { "value", "bare" } } },
        { braced, { { "value", "braced" } } },
        { upper, { { "value", "upper" } } },
        { "legacy-owner", { { "value", "opaque" } } }
    };
    const auto original = elements;
    const dust3d::SnapshotIdentityIndex index(elements);
    for (const auto& key : { bare, braced, upper, std::string("legacy-owner") }) {
        const auto result = index.find(elements, key);
        require(!result.ambiguous && result.element != elements.end() && result.element->first == key, "Exact authored match lost precedence");
    }
    const auto computed = index.find(elements, dust3d::Uuid(bare));
    require(!computed.ambiguous && computed.element != elements.end() && computed.element->second.at("value") == "braced", "Computed canonical string must retain an exact live winner");
    const auto upperAlias = index.find(elements, "{" + upper + "}");
    require(!upperAlias.ambiguous && upperAlias.element != elements.end() && upperAlias.element->first == upper, "Case-distinct UUID was rebound to another identity");
    require(index.find(elements, std::string("LEGACY-OWNER")).element == elements.end(), "Opaque identity was case-folded");
    require(elements == original, "Identity lookup mutated the source collection");
}

void nonExactCanonicalAmbiguityNeverSelectsAnArbitraryOwner()
{
    const Elements elements { { bare, {} }, { braced, {} } };
    const dust3d::SnapshotIdentityIndex index(elements);
    const auto result = index.find(elements, third);
    require(result.ambiguous && result.element == elements.end(), "Nonexact UUID ambiguity must not choose a source owner");
    const Elements noCanonicalExact { { bare, {} }, { third, {} } };
    const dust3d::SnapshotIdentityIndex computedIndex(noCanonicalExact);
    const auto computed = computedIndex.find(noCanonicalExact, dust3d::Uuid(bare));
    require(computed.ambiguous && computed.element == noCanonicalExact.end(), "Computed UUID ambiguity must remain explicit without an exact canonical key");
    require(elements.size() == 2 && noCanonicalExact.size() == 2, "Ambiguous source identities were deduplicated");
}

void ownsAliasKeysBeyondTheOriginalCollectionAndChecksLiveExistence()
{
    const auto index = []() {
        const Elements original { { bare, { { "version", "old" } } } };
        return dust3d::SnapshotIdentityIndex(original);
    }();
    Elements live { { bare, { { "version", "live" } } } };
    const auto found = index.find(live, braced);
    require(!found.ambiguous && found.element != live.end() && found.element->second.at("version") == "live", "Index must own keys and return an iterator into the supplied live collection");
    static_assert(std::is_same_v<decltype(found.element), Elements::iterator>);
    found.element->second["version"] = "edited";
    require(live.at(bare).at("version") == "edited", "Mutable fallback iterator did not update the live authored record");
    const Elements& readOnly = live;
    const auto readOnlyFound = index.find(readOnly, braced);
    static_assert(std::is_same_v<decltype(readOnlyFound.element), Elements::const_iterator>);
    require(readOnlyFound.element != readOnly.end() && readOnlyFound.element->second.at("version") == "edited", "Const lookup did not observe the current live attributes");
    live.erase(bare);
    const auto erased = index.find(live, braced);
    require(!erased.ambiguous && erased.element == live.end(), "Stale alias key resolved an erased live entity");
    const auto computed = index.find(live, dust3d::Uuid(bare));
    require(!computed.ambiguous && computed.element == live.end(), "Computed UUID lookup must check live existence after erasure");
}

void newlyGeneratedExactKeysAndMissingOrEmptyLookupsRemainSafe()
{
    Elements live { { bare, {} } };
    const dust3d::SnapshotIdentityIndex index(live);
    live[braced] = { { "generated", "new" } };
    const auto exact = index.find(live, braced);
    require(!exact.ambiguous && exact.element != live.end() && exact.element->second.at("generated") == "new", "New generated canonical key must resolve through live exact lookup");
    const auto computed = index.find(live, dust3d::Uuid(bare));
    require(!computed.ambiguous && computed.element == exact.element, "Computed UUID must see a newly added canonical exact key");
    for (const auto& missing : { std::string(), std::string("missing"), std::string("ffffffff-ffff-4fff-8fff-ffffffffffff") }) {
        const auto result = index.find(live, missing);
        require(!result.ambiguous && result.element == live.end(), "Missing identity acquired an owner or ambiguity");
    }
    const Elements empty;
    const dust3d::SnapshotIdentityIndex emptyIndex(empty);
    require(emptyIndex.find(empty, dust3d::Uuid()).element == empty.end(), "Null UUID lookup on an empty source must remain missing");
}
}

int main()
{
    try {
        computedUuidFindsAuthoredBareBracedAndInheritedOpaqueKeys();
        exactMatchWinsWithoutCaseFoldingOrOpaqueIdentityCoercion();
        nonExactCanonicalAmbiguityNeverSelectsAnArbitraryOwner();
        ownsAliasKeysBeyondTheOriginalCollectionAndChecksLiveExistence();
        newlyGeneratedExactKeysAndMissingOrEmptyLookupsRemainSafe();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

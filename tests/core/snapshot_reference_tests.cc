// SPDX-License-Identifier: MIT
// Synthetic metadata only: no geometry, assets, writer, rig or animation execution.
#include "EngineInternal.h"
#include "SnapshotReferences.h"
#include <dust3d/base/snapshot_xml.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
const std::string part = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
const std::string first = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
const std::string second = "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
const std::string edge = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
const std::string component = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
const std::string absent = "ffffffff-ffff-4fff-8fff-ffffffffffff";

std::string braces(const std::string& value) { return "{" + value + "}"; }

void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

void invalid(const std::function<void()>& action, const std::string& cause = "")
{
    try {
        action();
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Reference failure lost its invalid-document status");
        require(cause.empty() || std::string(error.what()).find(cause) != std::string::npos, "Reference failure lost its field cause");
        return;
    }
    throw std::runtime_error("Invalid graph reference was accepted");
}

dust3d::Snapshot chain(bool bracedOwners = false)
{
    const auto owner = [&](const std::string& id) { return bracedOwners ? braces(id) : id; };
    const auto reference = [&](const std::string& id) { return bracedOwners ? id : braces(id); };
    dust3d::Snapshot snapshot;
    snapshot.parts[owner(part)] = { { "id", owner(part) }, { "future", "part" } };
    snapshot.nodes[owner(first)] = { { "id", owner(first) }, { "partId", reference(part) }, { "x", "0" }, { "y", "0" }, { "z", "0" }, { "radius", ".1" }, { "future", "node" } };
    snapshot.nodes[owner(second)] = { { "id", owner(second) }, { "partId", reference(part) }, { "x", "1" }, { "y", "0" }, { "z", "0" }, { "radius", ".1" } };
    snapshot.edges[owner(edge)] = { { "id", owner(edge) }, { "from", reference(first) }, { "to", reference(second) }, { "partId", reference(part) }, { "future", "edge" } };
    snapshot.components[owner(component)] = { { "id", owner(component) }, { "linkDataType", "partId" }, { "linkData", reference(part) }, { "future", "component" } };
    snapshot.rootComponent["children"] = reference(component);
    snapshot.canvas["future"] = "canvas";
    return snapshot;
}

void resolvesUniqueAliasesAndPreservesAuthoredIdentity()
{
    for (bool bracedOwners : { false, true }) {
        const auto key = [&](const std::string& id) { return bracedOwners ? braces(id) : id; };
        const auto source = chain(bracedOwners);
        const auto savedNodes = source.nodes;
        auto execution = source;
        dust3d::native::resolveSnapshotReferences(execution);
        require(source.nodes == savedNodes && source.nodes != execution.nodes, "Execution reference resolution changed the authored source copy");
        require(execution.nodes.size() == source.nodes.size() && execution.parts.size() == source.parts.size(), "Reference resolution renamed or merged source entities");
        require(execution.nodes.at(key(first)).at("id") == key(first) && execution.parts.at(key(part)).at("id") == key(part) && execution.edges.at(key(edge)).at("id") == key(edge) && execution.components.at(key(component)).at("id") == key(component), "Authored id attributes changed");
        require(execution.nodes.at(key(first)).at("partId") == key(part), "Node owner alias did not resolve to authored part key");
        require(execution.edges.at(key(edge)).at("from") == key(first) && execution.edges.at(key(edge)).at("to") == key(second), "Edge endpoint aliases did not resolve to authored node keys");
        require(execution.edges.at(key(edge)).at("partId") == key(part), "Edge owner alias did not resolve");
        require(execution.components.at(key(component)).at("linkData") == key(part), "Typed component part alias did not resolve");
        require(execution.nodes.at(key(first)).at("future") == "node" && execution.parts.at(key(part)).at("future") == "part" && execution.edges.at(key(edge)).at("future") == "edge" && execution.components.at(key(component)).at("future") == "component" && execution.canvas.at("future") == "canvas", "Unknown source metadata changed");
        dust3d::native::validateSnapshot(execution);
        require(execution.edges.at(key(edge)).at("from") == key(first) && execution.edges.at(key(edge)).at("to") == key(second), "Orientation did not retain the valid resolved chain");
    }
}

void retainsExactMatchesAndDuplicateCanonicalDefinitions()
{
    auto snapshot = chain();
    snapshot.parts[braces(part)] = { { "id", braces(part) }, { "future", "distinct" } };
    snapshot.nodes[first]["partId"] = part;
    snapshot.nodes[second]["partId"] = part;
    snapshot.edges[edge]["partId"] = part;
    snapshot.edges[edge]["from"] = first;
    snapshot.edges[edge]["to"] = second;
    snapshot.components[component]["linkData"] = braces(part);
    snapshot.nodes[braces(first)] = snapshot.nodes[first];
    snapshot.nodes[braces(first)]["id"] = braces(first);
    snapshot.nodes[braces(first)]["partId"] = braces(part);
    snapshot.nodes[braces(first)]["future"] = "distinct-node";
    const auto nodes = snapshot.nodes;
    const auto parts = snapshot.parts;
    dust3d::native::resolveSnapshotReferences(snapshot);
    require(snapshot.nodes == nodes && snapshot.parts == parts, "Exact matches must not deduplicate or rename canonical-alias definitions");
    require(snapshot.edges.at(edge).at("from") == first, "Exact node reference lost precedence to its brace alias");
    require(snapshot.components.at(component).at("linkData") == braces(part), "Exact part reference lost precedence to its brace alias");
    // SourceProvenance retains both node keys and reports their shared UUID as ambiguous.
}

void preservesCaseAndExistingMissingReferenceValidation()
{
    for (const auto& missing : { absent, std::string("AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA") }) {
        auto snapshot = chain();
        snapshot.nodes[first]["partId"] = missing;
        dust3d::native::resolveSnapshotReferences(snapshot);
        require(snapshot.nodes.at(first).at("partId") == missing, "Missing or case-distinct reference was silently rebound");
        invalid([&]() { dust3d::native::validateSnapshot(snapshot); });
    }
    auto missingEndpoint = chain();
    missingEndpoint.edges[edge]["from"] = absent;
    dust3d::native::resolveSnapshotReferences(missingEndpoint);
    invalid([&]() { dust3d::native::validateSnapshot(missingEndpoint); });
}

void rejectsSelfEdgesAfterAliasResolution()
{
    auto snapshot = chain();
    snapshot.edges[edge]["from"] = first;
    snapshot.edges[edge]["to"] = braces(first);
    dust3d::native::resolveSnapshotReferences(snapshot);
    require(snapshot.edges.at(edge).at("from") == snapshot.edges.at(edge).at("to"), "Equivalent endpoint aliases were not resolved consistently");
    invalid([&]() { dust3d::native::validateSnapshot(snapshot); });
}

void resolvesOnlyExistingTypedOptionalReferences()
{
    auto snapshot = chain();
    const std::string unresolved = "99999999-9999-4999-8999-999999999999";
    snapshot.nodes[first]["cutFace"] = braces(part);
    snapshot.nodes[second]["cutFace"] = "Quad";
    snapshot.parts[part]["cutFace"] = braces(part);
    snapshot.components[component]["children"] = braces(component) + "," + braces(unresolved);
    snapshot.components[component]["wrap"] = "Cloth";
    snapshot.components[component]["wrapBindTo"] = braces(component);
    snapshot.rootComponent["children"] = braces(component) + "," + braces(unresolved);
    snapshot.components[absent] = { { "id", absent }, { "linkDataType", "future" }, { "linkData", braces(part) }, { "future", braces(part) } };
    snapshot.parts[braces(absent)] = { { "id", braces(absent) }, { "cutFace", braces(unresolved) } };
    dust3d::native::resolveSnapshotReferences(snapshot);
    require(snapshot.nodes.at(first).at("cutFace") == part && snapshot.parts.at(part).at("cutFace") == part, "Existing UUID profile aliases did not resolve");
    require(snapshot.nodes.at(second).at("cutFace") == "Quad" && snapshot.parts.at(braces(absent)).at("cutFace") == braces(unresolved), "Preset or missing profile intent changed");
    require(snapshot.components.at(component).at("children") == component + "," + braces(unresolved) && snapshot.rootComponent.at("children") == component + "," + braces(unresolved), "Ordered component/root child references did not resolve");
    require(snapshot.components.at(component).at("wrapBindTo") == component, "Existing wrap binding alias did not resolve");
    require(snapshot.components.at(absent).at("linkData") == braces(part) && snapshot.components.at(absent).at("future") == braces(part), "Non-part link or opaque metadata acquired graph-reference semantics");
    snapshot.components[component]["wrapBindTo"] = braces(unresolved);
    dust3d::native::resolveSnapshotReferences(snapshot);
    require(snapshot.components.at(component).at("wrapBindTo") == braces(unresolved), "Missing wrap binding was silently replaced");
}

dust3d::Snapshot nativeSnapshot(const std::string& source)
{
    std::vector<char> bytes(source.begin(), source.end());
    bytes.push_back(0);
    dust3d::Snapshot snapshot;
    dust3d::loadSnapshotFromXmlString(&snapshot, bytes.data(), dust3d::SnapshotXmlMode::NativeExecution);
    dust3d::native::resolveSnapshotReferences(snapshot);
    return snapshot;
}

void nativeFlatHierarchyKeepsOrderDuplicatesAndStableCollisionFreeKeys()
{
    const std::string reserved = "00000000-0000-8000-8000-000000000001";
    const std::string firstLeaf = "{00000000-0000-8000-8000-000000000002}";
    const std::string secondLeaf = "{00000000-0000-8000-8000-000000000003}";
    const std::string thirdLeaf = "{00000000-0000-8000-8000-000000000004}";
    const auto source = "<canvas><parts><part id='" + braces(part) + "'/><part id='" + reserved
        + "'/></parts><partIdList><part id='" + part + "'/><part id='" + braces(reserved)
        + "'/><part id='" + braces(part) + "'/></partIdList></canvas>";
    const auto snapshot = nativeSnapshot(source);
    const auto repeated = nativeSnapshot(source);
    require(snapshot.components == repeated.components && snapshot.rootComponent == repeated.rootComponent,
        "Repeated reads must synthesize the same legacy hierarchy");
    require(snapshot.components.size() == 3 && !snapshot.components.count(braces(reserved)),
        "Synthetic keys must skip a reserved UUID alias and retain each flat entry");
    require(snapshot.rootComponent.at("children") == firstLeaf + "," + secondLeaf + "," + thirdLeaf,
        "Legacy entries must retain authored order including repeats");
    require(snapshot.components.at(firstLeaf).at("linkData") == braces(part)
            && snapshot.components.at(secondLeaf).at("linkData") == reserved
            && snapshot.components.at(thirdLeaf).at("linkData") == braces(part),
        "Each flat component must resolve to its actual authored part key");
}

void nativeModernHierarchySuppressesStaleLegacyEntries()
{
    const auto prefix = "<canvas><parts><part id='" + part + "'/></parts><partIdList><part id='"
        + absent + "'/></partIdList>";
    const auto empty = nativeSnapshot(prefix + "<components/></canvas>");
    const auto emptyChildren = empty.rootComponent.find("children");
    require(empty.components.empty() && (emptyChildren == empty.rootComponent.end() || emptyChildren->second.empty()),
        "An explicit empty hierarchy must suppress stale flat entries");
    const auto modern = nativeSnapshot(prefix + "<components><component id='" + component
        + "' linkDataType='partId' linkData='" + braces(part) + "'/></components></canvas>");
    require(modern.components.size() == 1 && modern.rootComponent.at("children") == component,
        "Modern hierarchy must exclude stale synthesized leaves");
    require(modern.components.at(component).at("linkData") == part, "Modern typed owner alias must still resolve");
}

void nativeLegacyInversionUsesEachExactAuthoredOwner()
{
    for (bool bareInverted : { false, true }) {
        const auto source = "<canvas><parts><part id='" + part + "' inverse='" + (bareInverted ? "true" : "false")
            + "'/><part id='" + braces(part) + "' inverse='" + (bareInverted ? "false" : "true")
            + "'/></parts><components><component id='" + component + "' linkDataType='partId' linkData='"
            + part + "' combineMode='Uncombined'/><component id='" + braces(component)
            + "' linkDataType='partId' linkData='" + braces(part) + "' combineMode='Uncombined'/></components></canvas>";
        const auto snapshot = nativeSnapshot(source);
        require(snapshot.parts.size() == 2 && snapshot.components.size() == 2, "Exact authored aliases must remain distinct");
        require(snapshot.components.at(component).at("combineMode") == (bareInverted ? "Inversion" : "Uncombined")
                && snapshot.components.at(braces(component)).at("combineMode") == (bareInverted ? "Uncombined" : "Inversion"),
            "Legacy inversion must use the exact linked owner instead of a canonical alias winner");
    }
}

void composesWithTheRealXmlReader()
{
    const auto source = "<canvas><nodes><node id='" + braces(first)
        + "' partId='" + part + "' x='0' y='0' z='0' radius='.1'/></nodes>"
                                "<parts><part id='"
        + braces(part) + "' inverse='True' future='keep'/></parts>"
                         "<components><component id='"
        + braces(component) + "' linkDataType='partId' linkData='" + part
        + "' combineMode='Uncombined'/></components></canvas>";
    std::vector<char> bytes(source.begin(), source.end());
    bytes.push_back(0);
    dust3d::Snapshot snapshot;
    dust3d::loadSnapshotFromXmlString(&snapshot, bytes.data());
    require(snapshot.components.at(braces(component)).at("combineMode") == "Uncombined", "The shared XML reader changed the authored combine mode");
    dust3d::native::resolveSnapshotReferences(snapshot);
    dust3d::native::validateSnapshot(snapshot);
    require(snapshot.nodes.at(braces(first)).at("partId") == braces(part), "XML node owner did not resolve before validation");
    require(snapshot.components.at(braces(component)).at("linkData") == braces(part), "XML component owner did not resolve before validation");
    require(snapshot.components.at(braces(component)).at("combineMode") == "Inversion", "Native reference resolution lost inherited inversion");
    require(snapshot.parts.at(braces(part)).at("inverse") == "True" && snapshot.parts.at(braces(part)).at("future") == "keep", "XML source attributes changed");
}

void rejectsNonExactAmbiguousAliasWithFieldCause()
{
    auto snapshot = chain();
    snapshot.parts[braces(part)] = { { "id", braces(part) } };
    // Inherited Uuid accepts either closing character here; this third spelling
    // has no exact match but names both authored canonical aliases.
    const auto thirdSpelling = "{" + part + "!";
    snapshot.nodes[first]["partId"] = thirdSpelling;
    invalid([&]() { dust3d::native::resolveSnapshotReferences(snapshot); }, "partId");
}
}

int main()
{
    try {
        resolvesUniqueAliasesAndPreservesAuthoredIdentity();
        retainsExactMatchesAndDuplicateCanonicalDefinitions();
        preservesCaseAndExistingMissingReferenceValidation();
        rejectsSelfEdgesAfterAliasResolution();
        resolvesOnlyExistingTypedOptionalReferences();
        rejectsNonExactAmbiguousAliasWithFieldCause();
        composesWithTheRealXmlReader();
        nativeFlatHierarchyKeepsOrderDuplicatesAndStableCollisionFreeKeys();
        nativeModernHierarchySuppressesStaleLegacyEntries();
        nativeLegacyInversionUsesEachExactAuthoredOwner();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

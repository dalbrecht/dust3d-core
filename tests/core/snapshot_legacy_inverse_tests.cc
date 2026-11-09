// SPDX-License-Identifier: MIT
// Synthetic metadata fixtures follow Qt Document::addFromSnapshot's legacy part inversion rule.
#include <dust3d/base/snapshot_xml.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

dust3d::Snapshot read(const std::string& source)
{
    std::vector<char> bytes(source.begin(), source.end());
    bytes.push_back(0);
    dust3d::Snapshot snapshot;
    dust3d::loadNativeSnapshotFromXmlString(&snapshot, bytes.data());
    return snapshot;
}

std::string fixture(const std::string& inverse, const std::string& mode)
{
    return "<canvas originX='0'><nodes><node id='n' future='node'/></nodes>"
           "<edges><edge id='e' future='edge'/></edges><parts><part id='p' "
        + inverse + " future='part'/><part id='q'/></parts>"
                    "<components><component id='group' name='Keep' combineMode='future'>"
                    "<component id='target' linkDataType='partId' linkData='p' future='component' "
        + mode + "/><component id='other' linkDataType='partId' linkData='q' combineMode='Uncombined'/>"
                 "</component></components><animations><animation id='a' name='Keep' type='future' extra='keep'/>"
                 "</animations></canvas>";
}

void ordinaryXmlReaderRetainsTheAuthoredMode()
{
    const auto source = fixture("inverse='true'", "combineMode='Uncombined'");
    std::vector<char> bytes(source.begin(), source.end());
    bytes.push_back(0);
    dust3d::Snapshot snapshot;
    dust3d::loadSnapshotFromXmlString(&snapshot, bytes.data());
    require(snapshot.components.at("target").at("combineMode") == "Uncombined",
        "The shared XML reader must keep the authored mode for existing Qt callers");
}

void forcesLegacyInversionWithoutChangingOtherMetadata()
{
    for (const auto* truth : { "true", "True", "1" }) {
        for (const auto* mode : { "", "combineMode='Normal'", "combineMode='Inversion'", "combineMode='Uncombined'", "combineMode='future'" }) {
            const auto snapshot = read(fixture(std::string("inverse='") + truth + "'", mode));
            const auto& target = snapshot.components.at("target");
            const auto modeIt = target.find("combineMode");
            require(modeIt != target.end() && modeIt->second == "Inversion", "Legacy inverse must force the owning component to Inversion");
            require(snapshot.parts.at("p").at("inverse") == truth, "Legacy source spelling must be retained");
            require(snapshot.parts.at("p").at("future") == "part", "Part metadata changed");
            require(snapshot.components.at("target").at("future") == "component", "Component metadata changed");
            require(snapshot.components.at("group").at("combineMode") == "future", "Parent mode changed");
            require(snapshot.components.at("group").at("children") == "target,other", "Hierarchy order changed");
            require(snapshot.components.at("other").at("combineMode") == "Uncombined", "Unrelated part mode changed");
            require(snapshot.nodes.at("n").at("future") == "node", "Node metadata changed");
            require(snapshot.edges.at("e").at("future") == "edge", "Edge metadata changed");
            require(snapshot.animations.at("a").at("extra") == "keep", "Animation metadata changed");
        }
    }
}

void preservesFalseMissingAndUnsupportedValues()
{
    for (const auto* inverse : { "", "inverse='false'", "inverse='0'", "inverse='TRUE'", "inverse='future'" }) {
        for (const auto* mode : { "", "combineMode='Normal'", "combineMode='Inversion'", "combineMode='Uncombined'", "combineMode='future'" }) {
            const auto snapshot = read(fixture(inverse, mode));
            const auto& component = snapshot.components.at("target");
            if (std::string(mode).empty())
                require(component.count("combineMode") == 0, "Inactive legacy flag must not introduce a mode");
            else {
                const auto text = std::string(mode);
                require(component.at("combineMode") == text.substr(13, text.size() - 14), "Inactive legacy flag changed the explicit mode");
            }
        }
    }
}

void onlyTransformsAnExplicitExistingPartLink()
{
    const auto snapshot = read("<canvas><parts><part id='p' inverse='true'/></parts><components>"
                               "<component id='unknown' linkDataType='future' linkData='p' combineMode='Normal'/>"
                               "<component id='missing' linkDataType='partId' linkData='absent' combineMode='Uncombined'/>"
                               "<component id='group' linkData='p'/></components></canvas>");
    require(snapshot.components.at("unknown").at("combineMode") == "Normal", "Unknown link type acquired part semantics");
    require(snapshot.components.at("missing").at("combineMode") == "Uncombined", "Missing owner changed the mode");
    require(snapshot.components.at("group").count("combineMode") == 0, "Untyped group acquired part semantics");
}

void supportsUuidBraceAliasesWithoutConflatingOtherOwners()
{
    const std::string bare = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    const std::string braced = "{" + bare + "}";
    for (const auto& owner : { bare, braced }) {
        const auto link = owner == bare ? braced : bare;
        const auto snapshot = read("<canvas><parts><part id='" + owner + "' inverse='true'/></parts>"
                                                                         "<components><component id='c' linkDataType='partId' linkData='"
            + link + "'/></components></canvas>");
        const auto& component = snapshot.components.at("c");
        const auto mode = component.find("combineMode");
        require(mode != component.end() && mode->second == "Inversion", "Braced and unbraced Qt UUID aliases must share inverse semantics");
        require(component.at("linkData") == link, "Compatibility must preserve original link spelling");
    }
    for (const auto& pair : { std::make_pair(braced, std::string("{AAAAAAAA-AAAA-4AAA-8AAA-AAAAAAAAAAAA}")),
             std::make_pair(std::string("bad-owner"), std::string("other-owner")) }) {
        const auto snapshot = read("<canvas><parts><part id='" + pair.first + "' inverse='true'/></parts>"
                                                                              "<components><component id='c' linkDataType='partId' linkData='"
            + pair.second + "'/></components></canvas>");
        require(snapshot.components.at("c").count("combineMode") == 0, "Case-distinct or invalid identities must not acquire another owner's inverse flag");
    }
}

void supportsLegacyFlatPartLists()
{
    const auto snapshot = read("<canvas><parts><part id='p' inverse='true'/></parts>"
                               "<partIdList><part id='p'/></partIdList></canvas>");
    require(snapshot.components.size() == 1, "Legacy flat part list must create one component");
    const auto& component = snapshot.components.begin()->second;
    require(component.at("linkData") == "p", "Legacy flat part link changed");
    require(component.at("combineMode") == "Inversion", "Generated legacy component lost inverse semantics");
}
}

int main()
{
    try {
        ordinaryXmlReaderRetainsTheAuthoredMode();
        forcesLegacyInversionWithoutChangingOtherMetadata();
        preservesFalseMissingAndUnsupportedValues();
        onlyTransformsAnExplicitExistingPartLink();
        supportsLegacyFlatPartLists();
        supportsUuidBraceAliasesWithoutConflatingOtherOwners();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

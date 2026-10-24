// SPDX-License-Identifier: MIT
#include <dust3d/base/snapshot_xml.h>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
dust3d::Snapshot makeOrdinarySnapshot()
{
    dust3d::Snapshot snapshot;
    snapshot.canvas["name"] = "A&B";
    snapshot.rootComponent["children"] = "group";
    snapshot.components["group"] = { { "id", "group" }, { "children", "leaf" }, { "__cache", "omit" } };
    snapshot.components["leaf"] = { { "id", "leaf" }, { "name", "<leaf>\"" } };
    return snapshot;
}
void requireValidationRejection(const dust3d::Snapshot& snapshot, dust3d::SnapshotXmlWriteErrorCode code,
    dust3d::SnapshotXmlWriteLimits limits = {})
{
    // Invalid metadata is passed ONLY to the non-serializing validator (#83).
    try {
        dust3d::validateSnapshotForXmlWrite(snapshot, limits);
    } catch (const dust3d::SnapshotXmlWriteError& error) {
        require(error.code() == code && error.what()[0], "Typed writer validation cause required");
        return;
    }
    throw std::runtime_error("Invalid write metadata admitted");
}
void verifyOrdinaryOutput()
{
    const auto snapshot = makeOrdinarySnapshot();
    dust3d::validateSnapshotForXmlWrite(snapshot);
    std::string output = "retained-prefix\n";
    dust3d::saveSnapshotToXmlString(snapshot, output);
    const std::string expected = "retained-prefix\n<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                                 "<canvas name=\"A&amp;B\">\n <nodes>\n </nodes>\n <edges>\n </edges>\n <parts>\n </parts>\n"
                                 " <components>\n  <component id=\"group\">\n   <component id=\"leaf\" name=\"&lt;leaf&gt;&quot;\">\n"
                                 "   </component>\n  </component>\n </components>\n <animations>\n </animations>\n</canvas>\n";
    require(output == expected, "Ordinary writer bytes, ordering, escaping and append semantics retained");
    std::string again;
    dust3d::saveSnapshotToXmlString(snapshot, again, {});
    require(output == "retained-prefix\n" + again, "Explicit default limits preserve legacy call output");
}
void verifyOrdinaryFieldsAndExactBudget()
{
    dust3d::Snapshot snapshot;
    snapshot.nodes["n"] = { { "id", "n" }, { "future", "x&<>\"" } };
    snapshot.edges["e"] = { { "id", "e" } };
    snapshot.parts["p"] = { { "id", "p" }, { "__cache", "omit" } };
    snapshot.animations["a"] = { { "id", "a" } };
    std::string output;
    dust3d::saveSnapshotToXmlString(snapshot, output);
    require(output.find("future=\"x&amp;&lt;&gt;&quot;\"") != std::string::npos, "All inherited attribute escapes retained");
    require(output.find("__cache") == std::string::npos && output.find("<components>") == std::string::npos, "Internal part attributes and absent root section retained");
    require(output.find("<edge id=\"e\"/>") != std::string::npos && output.find("<animation id=\"a\"/>") != std::string::npos, "Flat sections retained");
    dust3d::SnapshotXmlWriteLimits limits;
    limits.maximumXmlBytes = output.size();
    dust3d::validateSnapshotForXmlWrite(snapshot, limits);
    std::string exact;
    dust3d::saveSnapshotToXmlString(snapshot, exact, limits);
    require(exact == output, "Exact successful byte boundary retained");
    auto emptyRoot = snapshot;
    emptyRoot.rootComponent["children"] = ",,";
    dust3d::validateSnapshotForXmlWrite(emptyRoot);
    std::string withRoot;
    dust3d::saveSnapshotToXmlString(emptyRoot, withRoot);
    require(withRoot.find(" <components>\n </components>\n") != std::string::npos, "Empty tokens and explicitly empty root section retained");
}

void verifyPureValidation()
{
    using WriteErrorCode = dust3d::SnapshotXmlWriteErrorCode;
    const auto source = makeOrdinarySnapshot();
    auto limits = dust3d::SnapshotXmlWriteLimits {};
    limits.maximumDepth = 1;
    requireValidationRejection(source, WriteErrorCode::limitExceeded, limits);
    limits = {};
    limits.maximumComponents = 1;
    requireValidationRejection(source, WriteErrorCode::limitExceeded, limits);
    limits = {};
    limits.maximumXmlBytes = 16;
    requireValidationRejection(source, WriteErrorCode::limitExceeded, limits);
    limits.maximumXmlBytes = 0;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    limits = {};
    limits.maximumDepth = 127;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    limits.maximumDepth = 0;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    limits = {};
    limits.maximumComponents = 0;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    limits.maximumComponents = 65537;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    limits = {};
    limits.maximumXmlBytes = 16 * 1024 * 1024 + 1;
    requireValidationRejection(source, WriteErrorCode::invalidLimits, limits);
    auto missing = source;
    missing.rootComponent["children"] = "absent";
    requireValidationRejection(missing, WriteErrorCode::invalidHierarchy);
    auto duplicate = source;
    duplicate.rootComponent["children"] = "group,group";
    requireValidationRejection(duplicate, WriteErrorCode::invalidHierarchy);
    auto detached = source;
    detached.components["detached"] = { { "id", "detached" } };
    requireValidationRejection(detached, WriteErrorCode::invalidHierarchy);
    auto metadata = source;
    metadata.components["leaf"]["children"] = "group";
    requireValidationRejection(metadata, WriteErrorCode::invalidHierarchy);
    require(source.components.at("leaf").count("children") == 0, "Validation does not mutate source metadata");
}
}
int main()
{
    try {
        verifyOrdinaryOutput();
        verifyOrdinaryFieldsAndExactBudget();
        verifyPureValidation();
        std::cout << "Snapshot XML write contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

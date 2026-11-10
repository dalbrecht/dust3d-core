#include "EngineInternal.h"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void rejectNumber(const std::string& value, const std::string& originalCause)
{
    dust3d::Snapshot snapshot;
    snapshot.canvas["originX"] = value;
    try {
        dust3d::native::validateSnapshot(snapshot, false, true);
    } catch (const dust3d::native::DocumentError& error) {
        require(error.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Numeric rejection lost its document status");
        const std::string message = error.what();
        require(message.find("originX") != std::string::npos, "Numeric rejection lost its field name");
        require(message.find(originalCause) != std::string::npos, "Numeric rejection lost its original cause");
        return;
    }
    throw std::runtime_error("Invalid numeric metadata was accepted");
}
void validateEdgeDirections()
{
    const std::string part = "{10000000-0000-4000-8000-000000000001}";
    const std::string low = "{20000000-0000-4000-8000-000000000001}";
    const std::string middle = "{20000000-0000-4000-8000-000000000002}";
    const std::string high = "{20000000-0000-4000-8000-000000000003}";
    const std::string first = "{30000000-0000-4000-8000-000000000001}";
    const std::string second = "{30000000-0000-4000-8000-000000000002}";
    const std::string closing = "{30000000-0000-4000-8000-000000000003}";
    dust3d::Snapshot chain;
    chain.parts[part] = {};
    for (const auto& node : { low, middle, high })
        chain.nodes[node] = { { "partId", part }, { "x", "0" }, { "y", "0" }, { "z", "0" }, { "radius", "1" } };
    chain.edges[first] = { { "partId", part }, { "from", high }, { "to", middle } };
    chain.edges[second] = { { "partId", part }, { "from", middle }, { "to", low } };
    const auto authoredChain = chain.edges;
    dust3d::native::validateSnapshot(chain, false, false);
    require(chain.edges == authoredChain, "Validation reversed an authored chain");

    auto cycle = chain;
    cycle.edges[closing] = { { "partId", part }, { "from", low }, { "to", high } };
    const auto authoredCycle = cycle.edges;
    dust3d::native::validateSnapshot(cycle, false, false);
    require(cycle.edges == authoredCycle, "Validation reversed an authored cycle");

    auto legacy = chain;
    std::swap(legacy.edges[first]["from"], legacy.edges[first]["to"]);
    dust3d::native::validateSnapshot(legacy, false, false);
    require(legacy.edges.at(first).at("from") == middle && legacy.edges.at(first).at("to") == high
            && legacy.edges.at(second).at("from") == low && legacy.edges.at(second).at("to") == middle,
        "Validation stopped repairing inconsistent legacy directions");
    const std::string fourth = "{20000000-0000-4000-8000-000000000004}";
    auto inconsistent = chain;
    inconsistent.nodes[fourth] = inconsistent.nodes.at(high);
    inconsistent.edges[first] = { { "partId", part }, { "from", fourth }, { "to", low } };
    inconsistent.edges[second] = { { "partId", part }, { "from", middle }, { "to", low } };
    inconsistent.edges[closing] = { { "partId", part }, { "from", middle }, { "to", high } };
    dust3d::native::validateSnapshot(inconsistent, false, false);
    require(inconsistent.edges.at(first).at("from") == fourth && inconsistent.edges.at(first).at("to") == low
            && inconsistent.edges.at(second).at("from") == low && inconsistent.edges.at(second).at("to") == middle
            && inconsistent.edges.at(closing).at("from") == middle && inconsistent.edges.at(closing).at("to") == high,
        "Legacy repair ignored the sole outgoing endpoint");
}
}

int main()
{
    try {
        // Capture this standard library's diagnostic rather than assume its wording.
        for (const auto* value : { "bad", "1e309", "1e-999" }) {
            std::string originalCause;
            try {
                std::stod(value);
            } catch (const std::invalid_argument& error) {
                originalCause = error.what();
            } catch (const std::out_of_range& error) {
                originalCause = error.what();
            }
            require(!originalCause.empty(), "Fixture must exercise numeric conversion rejection");
            rejectNumber(value, originalCause);
        }
        for (const auto* value : { "", "0", "0.25", "0,25", "-0.25", "1000000" }) {
            dust3d::Snapshot snapshot;
            snapshot.canvas["originX"] = value;
            dust3d::native::validateSnapshot(snapshot, false, true);
            require(snapshot.canvas.at("originX") == value, "Validation must preserve source spelling");
        }
        validateEdgeDirections();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

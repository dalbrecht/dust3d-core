// SPDX-License-Identifier: MIT
#include "ExportReceipt.h"
#include <json.hpp>
#include <stdexcept>

namespace {
void require(bool condition)
{
    if (!condition)
        throw std::logic_error("Export receipt contract failed");
}
}

int main()
{
    using namespace dust3d::export_cli;
    const auto record = serializeReceipt({ 2, 3, "source\n\".ds3", "file\t.fbx", "fbx",
        "geometry", true, 42, "warning\nsecond line" });
    require(record.back() == '\n' && record.find('\n') == record.size() - 1);
    const auto decoded = nlohmann::json::parse(record);
    require(decoded.at("version") == 1);
    require(decoded.at("jobIndex") == 2 && decoded.at("jobCount") == 3);
    require(decoded.at("source") == "source\n\".ds3" && decoded.at("destination") == "file\t.fbx");
    require(decoded.at("format") == "fbx" && decoded.at("mode") == "geometry");
    require(decoded.at("committed") == true && decoded.at("byteCount") == 42);
    require(decoded.at("diagnostic") == "warning\nsecond line");

    bool committed = false;
    const auto publish = [&] {
        committed = true;
        return 42;
    };
    const auto undelivered = publishWithReceipt(record, publish, [&](const std::string& line) {
        require(committed && line == record);
        return false;
    });
    require(undelivered.publication == 42 && !undelivered.delivered);

    bool deliveryAttempted = false;
    try {
        publishWithReceipt(record, []() -> int { throw std::runtime_error("rename failed"); }, [&](const std::string&) {
                deliveryAttempted = true;
                return true; });
        require(false);
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()) == "rename failed");
    }
    require(!deliveryAttempted);
}

// SPDX-License-Identifier: MIT
#include "ExportReceipt.h"
#include <cassert>
#include <json.hpp>

int main()
{
    using namespace dust3d::export_cli;
    const auto record = serializeReceipt({ 2, 3, "source\n\".ds3", "file\t.fbx", "fbx",
        "geometry", true, 42, "warning\nsecond line" });
    assert(record.back() == '\n' && record.find('\n') == record.size() - 1);
    const auto decoded = nlohmann::json::parse(record);
    assert(decoded.at("version") == 1);
    assert(decoded.at("jobIndex") == 2 && decoded.at("jobCount") == 3);
    assert(decoded.at("source") == "source\n\".ds3" && decoded.at("destination") == "file\t.fbx");
    assert(decoded.at("format") == "fbx" && decoded.at("mode") == "geometry");
    assert(decoded.at("committed") == true && decoded.at("byteCount") == 42);
    assert(decoded.at("diagnostic") == "warning\nsecond line");
}

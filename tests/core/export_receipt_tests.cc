// SPDX-License-Identifier: MIT
#include "ExportCancellation.h"
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

    ReceiptDeliveryState delivery;
    int publishedJobs = 0;
    int attemptedWrites = 0;
    for (int job = 0; job < 2; ++job) {
        const auto result = publishWithReceipt(record, [&] { return ++publishedJobs; }, [&](const std::string&) {
                if (!delivery.enabled())
                    return false;
                ++attemptedWrites;
                return false; });
        delivery.record(result.delivered);
    }
    require(publishedJobs == 2 && attemptedWrites == 1);
    require(delivery.batchExitCode(5, 6) == 6);
    require(delivery.singleExitCode(3, 6) == 3);
    require(delivery.singleExitCode(0, 6) == 6);

    bool destinationReplaced = false;
    bool receiptWritten = false;
    try {
        publishWithReceipt(record, [&] {
            commitUnlessCancelled([] { return true; }, [&] { destinationReplaced = true; });
            return 0; }, [&](const std::string&) {
            receiptWritten = true;
            return true; });
        require(false);
    } catch (const ExportCancelled&) {
    }
    require(!destinationReplaced && !receiptWritten);
    commitUnlessCancelled([] { return false; }, [&] { destinationReplaced = true; });
    require(destinationReplaced);
}

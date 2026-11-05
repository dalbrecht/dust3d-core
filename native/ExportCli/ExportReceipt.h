// SPDX-License-Identifier: MIT
#ifndef DUST3D_EXPORT_CLI_EXPORT_RECEIPT_H
#define DUST3D_EXPORT_CLI_EXPORT_RECEIPT_H

#include <cstddef>
#include <json.hpp>
#include <string>

namespace dust3d::export_cli {

struct ExportReceipt {
    size_t jobIndex;
    size_t jobCount;
    std::string source;
    std::string destination;
    std::string format;
    std::string mode;
    bool committed;
    size_t byteCount;
    std::string diagnostic;
};

inline std::string serializeReceipt(const ExportReceipt& receipt)
{
    const nlohmann::json value {
        { "version", 1 },
        { "jobIndex", receipt.jobIndex },
        { "jobCount", receipt.jobCount },
        { "source", receipt.source },
        { "destination", receipt.destination },
        { "format", receipt.format },
        { "mode", receipt.mode },
        { "committed", receipt.committed },
        { "byteCount", receipt.byteCount },
        { "diagnostic", receipt.diagnostic },
    };
    return value.dump() + '\n';
}

class ReceiptDeliveryState {
public:
    bool enabled() const noexcept { return m_delivered; }
    void record(bool delivered) noexcept { m_delivered = m_delivered && delivered; }

    int batchExitCode(int exportExitCode, int receiptExitCode) const noexcept
    {
        return m_delivered ? exportExitCode : receiptExitCode;
    }

    int singleExitCode(int exportExitCode, int receiptExitCode) const noexcept
    {
        return exportExitCode != 0 ? exportExitCode : batchExitCode(0, receiptExitCode);
    }

private:
    bool m_delivered = true;
};

template <typename Publish, typename Deliver>
auto publishWithReceipt(const std::string& record, Publish publish, Deliver deliver)
{
    struct Result {
        decltype(publish()) publication;
        bool delivered;
    };
    auto publication = publish();
    return Result { publication, record.empty() || deliver(record) };
}

}
#endif

// SPDX-License-Identifier: MIT
#include <algorithm>
#include <iostream>
#include <native/Engine/WorkerReplyCollector.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(ProtocolErrorCode code, Action action)
{
    try {
        action();
    } catch (const ProtocolError& cause) {
        require(cause.code() == code && cause.what()[0], "Original framing cause retained");
        return;
    }
    throw std::runtime_error("Invalid reply collector transition accepted");
}
Header request()
{
    Header result;
    result.identity.document[0] = 1;
    result.identity.request = 42;
    result.identity.revision = 3;
    result.payloadBytes = 3;
    return result;
}
std::vector<uint8_t> wire()
{
    auto header = request();
    header.kind = FrameKind::response;
    header.payloadBytes = 4;
    return encodeFrame({ header, { 0, 255, 42, 7 } }, {});
}
void fragmented()
{
    auto source = wire();
    std::vector<uint8_t> stored;
    WorkerReplyCollector collector(request(), {}, stored);
    require(collector.nextReadBytes(65536) == 64, "Read fixed header before body");
    for (size_t i = 0; i < source.size(); ++i)
        collector.append(source.data() + i, 1);
    require(stored == source && collector.nextReadBytes(65536) == 1, "Full frame permits only EOF/trailing-byte observation");
    collector.finish();
    collector.finish();
    collector.append(nullptr, 0);
    rejects(ProtocolErrorCode::invalidInput, [&] { collector.nextReadBytes(1); });
    source.assign(source.size(), 19);
    require(stored[64] == 0 && stored[65] == 255, "Collector owns retained bytes");
    const uint8_t extra = 1;
    rejects(ProtocolErrorCode::trailingData, [&] { collector.append(&extra, 1); });
    rejects(ProtocolErrorCode::invalidInput, [&] { collector.finish(); });
    source = wire();
    stored.clear();
    WorkerReplyCollector combined(request(), {}, stored);
    combined.append(source.data(), source.size());
    combined.finish();
    require(stored == source, "A supplied complete span validates header before copying body");
}
void rejectedHeaders()
{
    for (int field = 0; field < 5; ++field) {
        auto changed = request();
        changed.kind = FrameKind::response;
        changed.payloadBytes = 4;
        if (field == 0)
            ++changed.identity.request;
        if (field == 1)
            ++changed.identity.revision;
        if (field == 2)
            ++changed.identity.document[0];
        if (field == 3)
            changed.operation = Operation::generateUvMesh;
        if (field == 4)
            changed.kind = FrameKind::request;
        auto bytes = encodeFrame({ changed, { 1, 2, 3, 4 } }, {});
        std::vector<uint8_t> stored;
        WorkerReplyCollector collector(request(), {}, stored);
        rejects(field == 4 ? ProtocolErrorCode::unexpectedResponse : ProtocolErrorCode::staleResponse, [&] { collector.append(bytes.data(), bytes.size()); });
        require(stored.size() == headerBytes, "Rejected identity/kind never retains body bytes");
        rejects(ProtocolErrorCode::invalidInput, [&] { collector.append(bytes.data(), 1); });
    }
    auto changed = request();
    changed.kind = FrameKind::response;
    changed.payloadBytes = 9;
    const auto header = encodeHeader(changed, {});
    std::vector<uint8_t> stored;
    WorkerReplyCollector limited(request(), { 8, 8, 8 }, stored);
    rejects(ProtocolErrorCode::limitExceeded, [&] { limited.append(header.data(), header.size()); });
    require(stored.size() == 64, "Over-budget declaration rejects at header boundary");
}
void emptyFailureAndBorrowedStorage()
{
    auto header = request();
    header.kind = FrameKind::failure;
    header.payloadBytes = 0;
    auto bytes = encodeHeader(header, {});
    std::vector<uint8_t> stored;
    WorkerReplyCollector empty(request(), {}, stored);
    empty.append(bytes.data(), bytes.size());
    require(empty.nextReadBytes(7) == 1, "Zero-body failure still requires EOF observation");
    empty.finish();
    require(stored.size() == headerBytes, "Framing does not interpret failure payload semantics");

    std::vector<uint8_t> limitedStorage;
    WorkerReplyCollector limited(request(), { 8, 8, 2 }, limitedStorage);
    header.payloadBytes = 3;
    bytes = encodeHeader(header, {});
    rejects(ProtocolErrorCode::limitExceeded, [&] { limited.append(bytes.data(), bytes.size()); });
    require(limitedStorage.size() == headerBytes, "Failure budget is independently enforced before body retention");

    std::vector<uint8_t> mutatedStorage;
    WorkerReplyCollector mutated(request(), {}, mutatedStorage);
    mutatedStorage.push_back(1);
    rejects(ProtocolErrorCode::invalidInput, [&] { mutated.nextReadBytes(1); });
    rejects(ProtocolErrorCode::invalidInput, [&] { mutated.append(nullptr, 0); });
    mutatedStorage.clear();
    rejects(ProtocolErrorCode::invalidInput, [&] { mutated.finish(); });

    std::vector<uint8_t> chunkedStorage;
    WorkerReplyCollector chunked(request(), {}, chunkedStorage);
    const auto source = wire();
    require(chunked.nextReadBytes(7) == 7, "Caller chunk cap applies before header");
    chunked.append(source.data(), 60);
    require(chunked.nextReadBytes(7) == 4, "Read stops precisely at the header boundary");
    chunked.append(source.data() + 60, 4);
    require(chunked.nextReadBytes(2) == 2, "Caller chunk cap applies after header");
    chunked.append(source.data() + 64, 4);
    chunked.finish();
}
void incompleteAndConfiguration()
{
    const auto bytes = wire();
    for (size_t length : { size_t(0), size_t(63), size_t(65) }) {
        std::vector<uint8_t> stored;
        WorkerReplyCollector collector(request(), {}, stored);
        collector.append(bytes.data(), length);
        rejects(ProtocolErrorCode::truncated, [&] { collector.finish(); });
        rejects(ProtocolErrorCode::invalidInput, [&] { collector.append(bytes.data(), 1); });
    }
    std::vector<uint8_t> stored;
    WorkerReplyCollector collector(request(), {}, stored);
    rejects(ProtocolErrorCode::invalidLimits, [&] { collector.nextReadBytes(0); });
    rejects(ProtocolErrorCode::invalidLimits, [&] { collector.nextReadBytes(65537); });
    rejects(ProtocolErrorCode::invalidInput, [&] { collector.append(nullptr, 1); });
    rejects(ProtocolErrorCode::invalidInput, [&] { collector.finish(); });
    stored = { 1 };
    rejects(ProtocolErrorCode::invalidInput, [&] { WorkerReplyCollector bad(request(), {}, stored); });
    stored.clear();
    rejects(ProtocolErrorCode::invalidLimits, [&] { WorkerReplyCollector bad(request(), { 0, 1, 1 }, stored); });
    auto wrong = request();
    wrong.kind = FrameKind::response;
    rejects(ProtocolErrorCode::unexpectedResponse, [&] { WorkerReplyCollector bad(wrong, {}, stored); });
    auto trailing = wire();
    trailing.push_back(1);
    WorkerReplyCollector excess(request(), {}, stored);
    rejects(ProtocolErrorCode::trailingData, [&] { excess.append(trailing.data(), trailing.size()); });
    require(stored.size() == 64, "Overlong body span is rejected before retaining its body");
}
}
int main()
{
    try {
        fragmented();
        rejectedHeaders();
        incompleteAndConfiguration();
        emptyFailureAndBorrowedStorage();
        std::cout << "Bounded reply framing passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

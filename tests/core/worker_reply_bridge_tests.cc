// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerProtocol.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
void rejected(const Dust3DWorkerReplyResult& result, ProtocolErrorCode code)
{
    require(result.status == DUST3D_WORKER_REPLY_PROTOCOL_REJECTED
            && result.detail_source == DUST3D_WORKER_DETAIL_FRAME
            && result.detail_code == static_cast<int32_t>(code) && result.message[0]
            && !result.message[511] && !result.message_truncated,
        "Original bounded protocol cause crosses C");
}
Header request()
{
    Header result;
    result.identity.document[0] = 1;
    result.identity.request = 42;
    result.identity.revision = 7;
    result.payloadBytes = 3;
    return result;
}
using Owner = std::unique_ptr<Dust3DWorkerReplyCollector, decltype(&dust3d_worker_reply_destroy)>;
Owner create()
{
    const auto header = encodeHeader(request(), {});
    Dust3DWorkerReplyResult receipt {};
    Owner owner(dust3d_worker_reply_create(header.data(), header.size(), { 8, 8, 8 }, &receipt), dust3d_worker_reply_destroy);
    require(owner && receipt.status == DUST3D_WORKER_REPLY_READY, "Expected request copied into owner");
    return owner;
}
void transitions()
{
    auto owner = create();
    auto header = request();
    header.kind = FrameKind::response;
    header.payloadBytes = 4;
    auto frame = encodeFrame({ header, { 0, 255, 42, 7 } }, {});
    for (size_t i = 0; i < frame.size(); ++i) {
        size_t count = 0;
        require(dust3d_worker_reply_next_read(owner.get(), 7, &count).status == DUST3D_WORKER_REPLY_READY && count > 0 && count <= 7, "Bounded read size crosses C");
        require(dust3d_worker_reply_append(owner.get(), frame.data() + i, 1).status == DUST3D_WORKER_REPLY_READY, "Fragmented bytes accepted");
    }
    size_t count = 100;
    require(!dust3d_worker_reply_bytes(owner.get(), &count) && count == 0, "Complete bytes hidden until EOF");
    require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Actual EOF frames one reply");
    require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Repeated successful EOF");
    require(dust3d_worker_reply_append(owner.get(), nullptr, 0).status == DUST3D_WORKER_REPLY_FRAMED, "Empty append preserves framing");
    const auto* view = dust3d_worker_reply_bytes(owner.get(), &count);
    require(view && count == frame.size(), "Borrowed framed view returned");
    const std::vector<uint8_t> copied(view, view + count);
    owner.reset();
    frame.assign(frame.size(), 19);
    require(copied[64] == 0 && copied[65] == 255, "Copied bytes survive owner destruction and input mutation");
}
void errors()
{
    size_t count = 123;
    rejected(dust3d_worker_reply_next_read(nullptr, 7, &count), ProtocolErrorCode::invalidInput);
    require(count == 0 && !dust3d_worker_reply_bytes(nullptr, &count) && !count, "Null owner clears outputs");
    require(!dust3d_worker_reply_bytes(nullptr, nullptr), "Null output rejected without dereference");
    rejected(dust3d_worker_reply_append(nullptr, nullptr, 0), ProtocolErrorCode::invalidInput);
    auto owner = create();
    require(!dust3d_worker_reply_bytes(owner.get(), nullptr), "Live owner null view output");
    rejected(dust3d_worker_reply_next_read(owner.get(), 7, nullptr), ProtocolErrorCode::invalidInput);
    rejected(dust3d_worker_reply_finish(owner.get()), ProtocolErrorCode::invalidInput);
    owner = create();
    rejected(dust3d_worker_reply_next_read(owner.get(), 0, &count), ProtocolErrorCode::invalidLimits);
    rejected(dust3d_worker_reply_append(owner.get(), nullptr, 0), ProtocolErrorCode::invalidInput);
    owner = create();
    rejected(dust3d_worker_reply_append(owner.get(), nullptr, 1), ProtocolErrorCode::invalidInput);
    owner = create();
    rejected(dust3d_worker_reply_finish(owner.get()), ProtocolErrorCode::truncated);
    require(!dust3d_worker_reply_bytes(owner.get(), &count) && !count, "Rejected frame never exposed");
    for (int change = 0; change < 3; ++change) {
        owner = create();
        auto header = request();
        header.kind = FrameKind::response;
        header.payloadBytes = 4;
        if (change == 0)
            ++header.identity.revision;
        if (change == 1)
            header.payloadBytes = 9;
        auto frame = encodeFrame({ header, std::vector<uint8_t>(header.payloadBytes, 1) }, {});
        if (change == 2)
            frame.push_back(1);
        rejected(dust3d_worker_reply_append(owner.get(), frame.data(), frame.size()), change == 0 ? ProtocolErrorCode::staleResponse : change == 1 ? ProtocolErrorCode::limitExceeded
                                                                                                                                                   : ProtocolErrorCode::trailingData);
        require(!dust3d_worker_reply_bytes(owner.get(), &count) && !count, "Rejected body cannot be copied");
    }
    owner = create();
    auto header = request();
    header.kind = FrameKind::response;
    header.payloadBytes = 0;
    const auto frame = encodeHeader(header, {});
    require(dust3d_worker_reply_append(owner.get(), frame.data(), frame.size()).status == DUST3D_WORKER_REPLY_READY, "Empty response framed separately from semantics");
    require(dust3d_worker_reply_finish(owner.get()).status == DUST3D_WORKER_REPLY_FRAMED, "Empty response EOF");
    rejected(dust3d_worker_reply_append(owner.get(), frame.data(), 1), ProtocolErrorCode::trailingData);
    require(!dust3d_worker_reply_bytes(owner.get(), &count) && !count, "Later rejection hides previously framed view");
}
void creationErrors()
{
    auto header = encodeHeader(request(), {});
    Dust3DWorkerReplyResult result {};
    require(!dust3d_worker_reply_create(header.data(), 64, { 8, 8, 8 }, nullptr), "Missing result never creates owner");
    require(!dust3d_worker_reply_create(nullptr, 64, { 8, 8, 8 }, &result), "Null expected bytes reject");
    rejected(result, ProtocolErrorCode::invalidInput);
    const auto copied = result;
    require(!dust3d_worker_reply_create(header.data(), 63, { 8, 8, 8 }, &result), "Short expected header rejects");
    rejected(result, ProtocolErrorCode::truncated);
    require(copied.detail_code == static_cast<int32_t>(ProtocolErrorCode::invalidInput) && copied.message[0], "Original result self-owned across calls");
    require(!dust3d_worker_reply_create(header.data(), 64, { 0, 8, 8 }, &result), "Invalid explicit limits reject");
    rejected(result, ProtocolErrorCode::invalidLimits);
    header[17] = 2;
    require(!dust3d_worker_reply_create(header.data(), 64, { 8, 8, 8 }, &result), "Response cannot stand in for expected request");
    rejected(result, ProtocolErrorCode::unexpectedResponse);
}
}
int main()
{
    try {
        transitions();
        errors();
        creationErrors();
        std::cout << "Parent reply C ownership and diagnostics passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

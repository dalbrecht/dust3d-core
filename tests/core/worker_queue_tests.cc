// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerQueue.h>
#include <stdexcept>

namespace {
using namespace dust3d::worker;
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Error, typename Action>
void rejects(Action action)
{
    try {
        action();
    } catch (const Error&) {
        return;
    }
    throw std::runtime_error("Expected typed queue rejection");
}
template <typename Action>
void rejectsQueue(QueueErrorCode expected, Action action)
{
    try {
        action();
    } catch (const QueueError& cause) {
        require(cause.code() == expected && cause.what()[0], "Queue failure retains its typed cause");
        return;
    }
    throw std::runtime_error("Expected typed queue failure");
}
Frame request(uint64_t number)
{
    Frame frame;
    frame.header.identity.document[0] = 1;
    frame.header.identity.request = number;
    frame.header.identity.revision = number;
    frame.payload = { 'd', 's', '3' };
    frame.header.payloadBytes = frame.payload.size();
    return frame;
}
std::vector<uint8_t> failure(const Header& request)
{
    auto header = request;
    header.kind = FrameKind::failure;
    auto body = encodeFailurePayload({ FailureStatus::invalidDocument, { 'x', 0, 255 } }, {});
    header.payloadBytes = body.size();
    return encodeFrame({ header, body }, {});
}
void scheduling()
{
    RequestQueue queue;
    require(!queue.begin(), "Empty queue has no work");
    queue.submit(request(1));
    auto first = queue.begin();
    require(first && first->payload == request(1).payload, "Begin transfers owned request");
    queue.submit(request(2));
    auto replaced = queue.submit(request(3));
    require(replaced && replaced->request == 2, "Latest replaces waiting archive");
    require(!queue.begin(), "Active collection prevents overlap");
    auto wire = failure(first->header);
    rejects<AdmissionError>([&] { queue.complete(first->header.identity, { true, 0 }, wire.data(), wire.size()); });
    auto latest = queue.begin();
    require(latest && latest->header.identity.request == 3, "Stale completion releases slot for latest");
    wire = failure(latest->header);
    auto admitted = queue.complete(latest->header.identity, { true, 0 }, wire.data(), wire.size());
    require(std::get<FailurePayload>(admitted).cause == std::vector<uint8_t>({ 'x', 0, 255 }), "Admission preserves original cause");
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.complete(latest->header.identity, { true, 0 }, wire.data(), wire.size()); });
    require(!queue.begin(), "Completed queue is empty");
}
void cancellationAndValidation()
{
    RequestQueue queue;
    queue.submit(request(1));
    auto first = queue.begin();
    queue.submit(request(2));
    queue.cancel(request(2).header.identity);
    rejects<AdmissionError>([&] { queue.complete(first->header.identity, { true, 0 }, nullptr, 0); });
    require(!queue.begin(), "Cancelling latest never revives obsolete active request");
    queue.submit(request(3));
    auto active = queue.begin();
    auto invalid = request(4);
    invalid.header.payloadBytes++;
    rejectsQueue(QueueErrorCode::invalidRequest, [&] { queue.submit(invalid); });
    rejectsQueue(QueueErrorCode::reusedRequest, [&] { queue.submit(request(3)); });
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.complete(request(9).header.identity, { true, 0 }, nullptr, 0); });
    auto wire = failure(active->header);
    require(std::holds_alternative<FailurePayload>(queue.complete(active->header.identity, { true, 0 }, wire.data(), wire.size())), "Invalid submission or foreign completion preserves active work");
    queue.submit(request(4));
    active = queue.begin();
    queue.cancel(active->header.identity);
    rejects<AdmissionError>([&] { queue.complete(active->header.identity, { true, 0 }, nullptr, 0); });
    queue.submit(request(5));
    active = queue.begin();
    queue.submit(request(6));
    queue.close();
    queue.close();
    rejectsQueue(QueueErrorCode::closed, [&] { queue.submit(request(7)); });
    rejects<AdmissionError>([&] { queue.complete(active->header.identity, { true, 0 }, nullptr, 0); });
    require(!queue.begin(), "Close discards waiting work while allowing active collection to retire");
}
void failedAdmissionRecovery()
{
    RequestQueue queue;
    queue.submit(request(1));
    auto first = queue.begin();
    rejects<ProtocolError>([&] { queue.complete(first->header.identity, { true, 0 }, nullptr, 0); });
    queue.submit(request(2));
    first = queue.begin();
    require(first && first->header.identity.request == 2, "Malformed completion releases slot without publication");
    queue.cancel(request(99).header.identity);
    auto wire = failure(first->header);
    require(std::holds_alternative<FailurePayload>(queue.complete(first->header.identity, { true, 0 }, wire.data(), wire.size())), "Unrelated cancellation preserves current work");
    auto invalid = request(3);
    invalid.header.kind = FrameKind::response;
    rejectsQueue(QueueErrorCode::invalidRequest, [&] { queue.submit(invalid); });
    invalid = request(3);
    invalid.header.identity.request = 0;
    rejects<ProtocolError>([&] { queue.submit(invalid); });
    Limits limits;
    limits.maximumInputBytes = 2;
    RequestQueue limited(limits);
    rejects<ProtocolError>([&] { limited.submit(request(1)); });
    queue.submit(request(3));
    queue.cancel(request(3).header.identity);
    require(!queue.begin(), "Waiting-only cancellation removes archive");
    queue.submit(request(4));
    first = queue.begin();
    queue.submit(request(5));
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.abandon(request(99).header.identity); });
    queue.abandon(first->header.identity);
    first = queue.begin();
    require(first && first->header.identity.request == 5, "Transport failure releases active slot and preserves latest");
    queue.abandon(first->header.identity);
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.abandon(first->header.identity); });
    require(!queue.begin(), "Abandoned work cannot publish or replay");
}
}
int main()
{
    try {
        scheduling();
        cancellationAndValidation();
        failedAdmissionRecovery();
        std::cout << "Worker queue contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

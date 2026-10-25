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
ServiceReceipt serviceReceipt(const Header& request)
{
    ServiceReceipt result;
    result.runtimeStatus = result.admissionStatus = 0;
    result.operation = request.operation;
    result.request = request.identity.request;
    result.process = 101;
    return result;
}
void serviceScheduling()
{
    RequestQueue queue;
    queue.submit(request(1));
    rejectsQueue(QueueErrorCode::invalidServiceSession, [&] { queue.beginService(0); });
    auto first = queue.beginService(9);
    require(first && first->header.identity.request == 1, "Invalid generation never consumes waiting work");
    require(!queue.beginService(10), "Active service work prevents a second begin");
    const ServiceObservation observed { 9, 101, true };
    auto receipt = serviceReceipt(first->header);
    auto wire = failure(first->header);
    rejectsQueue(QueueErrorCode::wrongCompletionMode, [&] { queue.complete(first->header.identity, { true, 0 }, wire.data(), wire.size()); });
    auto foreign = first->header.identity;
    ++foreign.request;
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.completeService(foreign, observed, receipt, nullptr, 0); });
    queue.submit(request(2));
    queue.submit(request(3));
    rejects<AdmissionError>([&] { queue.completeService(first->header.identity, observed, receipt, wire.data(), wire.size()); });
    auto latest = queue.beginService(10);
    require(latest && latest->header.identity.request == 3, "Stale service completion releases slot for latest source");
    receipt = serviceReceipt(latest->header);
    wire = failure(latest->header);
    rejects<AdmissionError>([&] { queue.completeService(latest->header.identity, observed, receipt, wire.data(), wire.size()); });
    queue.submit(request(4));
    auto current = queue.beginService(10);
    receipt = serviceReceipt(current->header);
    wire = failure(current->header);
    auto admitted = queue.completeService(current->header.identity, { 10, 101, true }, receipt, wire.data(), wire.size());
    wire.assign(wire.size(), 0);
    require(std::get<FailurePayload>(admitted).cause == std::vector<uint8_t>({ 'x', 0, 255 }), "Queued service result owns original failure cause");
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.completeService(current->header.identity, { 10, 101, true }, receipt, nullptr, 0); });
    queue.submit(request(5));
    auto child = queue.begin();
    receipt = serviceReceipt(child->header);
    wire = failure(child->header);
    rejectsQueue(QueueErrorCode::wrongCompletionMode, [&] { queue.completeService(child->header.identity, { 10, 101, true }, receipt, wire.data(), wire.size()); });
    require(std::holds_alternative<FailurePayload>(queue.complete(child->header.identity, { true, 0 }, wire.data(), wire.size())), "Wrong service mode does not retire child work");
}
void serviceGenerationFreeze()
{
    RequestQueue queue;
    require(!queue.beginService(9), "Empty service queue starts no work");
    queue.submit(request(1));
    auto active = queue.beginService(9);
    require(!queue.beginService(10), "Busy begin returns no second request");
    auto wire = failure(active->header);
    require(std::holds_alternative<FailurePayload>(queue.completeService(active->header.identity, { 9, 101, true }, serviceReceipt(active->header), wire.data(), wire.size())), "Busy begin cannot replace captured generation");
    queue.submit(request(2));
    active = queue.beginService(10);
    queue.abandon(active->header.identity);
    rejectsQueue(QueueErrorCode::unexpectedCompletion, [&] { queue.completeService(active->header.identity, { 10, 101, true }, serviceReceipt(active->header), nullptr, 0); });
    queue.submit(request(3));
    auto child = queue.begin();
    wire = failure(child->header);
    require(std::holds_alternative<FailurePayload>(queue.complete(child->header.identity, { true, 0 }, wire.data(), wire.size())), "Abandoning service work permits fresh child admission");
}
void unmatchedServiceRetirement()
{
    RequestQueue queue;
    queue.cancel(request(99).header.identity);
    queue.submit(request(1));
    auto active = queue.beginService(9);
    queue.submit(request(2));
    queue.cancel(request(99).header.identity);
    queue.cancel(active->header.identity);
    queue.abandon(active->header.identity);
    auto latest = queue.beginService(10);
    require(latest && latest->header.identity.request == 2, "Foreign/old cancellation and abandonment preserve newer source");
    queue.cancel(latest->header.identity);
    queue.abandon(latest->header.identity);
    require(!queue.beginService(11), "Cancelled then abandoned work leaves no pending source");
}
void serviceRetirement()
{
    for (const bool close : { false, true }) {
        RequestQueue queue;
        queue.submit(request(1));
        auto active = queue.beginService(9);
        if (close)
            queue.close();
        else
            queue.cancel(active->header.identity);
        rejects<AdmissionError>([&] { queue.completeService(active->header.identity, { 9, 101, true }, serviceReceipt(active->header), nullptr, 0); });
        if (close)
            rejectsQueue(QueueErrorCode::closed, [&] { queue.submit(request(2)); });
        else {
            queue.submit(request(2));
            auto next = queue.beginService(10);
            require(next && next->header.identity.request == 2, "Cancelled service admission releases capacity");
            queue.abandon(next->header.identity);
        }
    }
    RequestQueue queue;
    queue.submit(request(1));
    auto active = queue.beginService(9);
    rejects<ProtocolError>([&] { queue.completeService(active->header.identity, { 9, 101, true }, serviceReceipt(active->header), nullptr, 0); });
    queue.submit(request(2));
    auto next = queue.beginService(10);
    require(next && next->header.identity.request == 2, "Malformed current completion releases active slot");
}

}
int main()
{
    try {
        serviceScheduling();
        serviceRetirement();
        serviceGenerationFreeze();
        unmatchedServiceRetirement();
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

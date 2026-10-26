// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerQueue.h>
#include <native/Engine/WorkerSessionOutput.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session admission contract");
}
Frame request(uint64_t identifier)
{
    Header header;
    header.operation = Operation::generateSessionIndexedSourcedMesh;
    header.identity.document[0] = 1;
    header.identity.request = identifier;
    header.identity.revision = 7;
    header.payloadBytes = 1;
    return { header, { 1 } };
}
std::vector<uint8_t> response(const Header& request)
{
    auto header = request;
    header.kind = FrameKind::response;
    auto body = encodeSessionOutput(SessionOutput {}, {});
    header.payloadBytes = body.size();
    return encodeFrame({ header, std::move(body) }, {});
}
ServiceReceipt receipt(const Header& request)
{
    ServiceReceipt result;
    result.runtimeStatus = 0;
    result.admissionStatus = 0;
    result.operation = request.operation;
    result.request = request.identity.request;
    result.process = 1234;
    return result;
}
int main()
{
    try {
        const auto first = request(1);
        const auto wire = response(first.header);
        const ServiceObservation observed { 9, 1234, true }; // Synthetic metadata only; no OS or native work.
        PendingServiceReply pending(first.header, 9);
        const auto output = pending.completeSession(first.header.identity, observed, receipt(first.header), wire.data(), wire.size());
        require(std::holds_alternative<std::monostate>(output.outcome) && pending.isRetired());
        bool rejected = false;
        try {
            (void)pending.completeSession(first.header.identity, observed, receipt(first.header), wire.data(), wire.size());
        } catch (const AdmissionError& cause) {
            rejected = cause.code() == AdmissionErrorCode::retired;
        }
        require(rejected);
        PendingServiceReply wrongPeer(first.header, 9);
        auto foreign = observed;
        foreign.connectionGeneration = 10;
        rejected = false;
        try {
            (void)wrongPeer.completeSession(first.header.identity, foreign, receipt(first.header), wire.data(), wire.size());
        } catch (const AdmissionError& cause) {
            rejected = cause.code() == AdmissionErrorCode::unexpectedServicePeer;
        }
        require(rejected && wrongPeer.isRetired());
        for (int mode = 0; mode < 5; ++mode) {
            PendingServiceReply rejectedGate(first.header, 9);
            auto active = first.header.identity;
            auto observation = observed;
            auto control = receipt(first.header);
            auto expected = AdmissionErrorCode::unexpectedServicePeer;
            if (!mode) {
                ++active.revision;
                expected = AdmissionErrorCode::staleRequest;
            }
            if (mode == 1) {
                observation.reachedEndOfStream = false;
                expected = AdmissionErrorCode::incompleteStream;
            }
            if (mode == 2) {
                ++control.request;
                expected = AdmissionErrorCode::mismatchedServiceReceipt;
            }
            if (mode == 3) {
                control.runtimeStatus = 1;
                expected = AdmissionErrorCode::unsuccessfulService;
            }
            if (mode == 4)
                control.process = 42;
            rejected = false;
            try {
                (void)rejectedGate.completeSession(active, observation, control, wire.data(), wire.size());
            } catch (const AdmissionError& cause) {
                rejected = cause.code() == expected;
            }
            require(rejected && rejectedGate.isRetired());
        }
        for (int mode = 0; mode < 3; ++mode) {
            auto expected = first.header;
            auto frame = decodeFrame(wire.data(), wire.size(), {});
            auto code = ProtocolErrorCode::unexpectedResponse;
            if (!mode) {
                expected.operation = Operation::generateMesh;
                frame.header.operation = expected.operation;
                code = ProtocolErrorCode::unsupportedOperation;
            }
            if (mode == 1) {
                frame.header.kind = FrameKind::failure;
                frame.payload = encodeFailurePayload(FailurePayload {}, {});
            }
            if (mode == 2) {
                ++frame.header.identity.revision;
                code = ProtocolErrorCode::staleResponse;
            }
            frame.header.payloadBytes = frame.payload.size();
            const auto encoded = encodeFrame(frame, {});
            PendingServiceReply gate(expected, 9);
            rejected = false;
            try {
                (void)gate.completeSession(expected.identity, observed, receipt(expected), encoded.data(), encoded.size());
            } catch (const ProtocolError& cause) {
                rejected = cause.code() == code;
            }
            require(rejected && gate.isRetired());
        }
        SessionOutput failed;
        failed.outcome = FailurePayload { FailureStatus::generationFailed, { 0, 255 } };
        failed.sourceIssue = FailurePayload { FailureStatus::limitExceeded, { 42 } };
        failed.statistics = GenerationStatistics {};
        auto failureFrame = first;
        failureFrame.header.kind = FrameKind::response;
        failureFrame.payload = encodeSessionOutput(failed, {});
        failureFrame.header.payloadBytes = failureFrame.payload.size();
        const auto failureWire = encodeFrame(failureFrame, {});
        PendingServiceReply failureGate(first.header, 9);
        const auto original = failureGate.completeSession(first.header.identity, observed, receipt(first.header), failureWire.data(), failureWire.size());
        require(std::get<FailurePayload>(original.outcome).cause == std::vector<uint8_t> { 0, 255 } && original.sourceIssue->cause[0] == 42 && original.statistics);
        RequestQueue queue;
        queue.submit(first);
        require(queue.beginService(9).has_value());
        auto foreignIdentity = first.header.identity;
        ++foreignIdentity.request;
        rejected = false;
        try {
            (void)queue.completeSessionService(foreignIdentity, observed, receipt(first.header), wire.data(), wire.size());
        } catch (const QueueError& cause) {
            rejected = cause.code() == QueueErrorCode::unexpectedCompletion;
        }
        require(rejected);
        const auto queued = queue.completeSessionService(first.header.identity, observed, receipt(first.header), wire.data(), wire.size());
        require(std::holds_alternative<std::monostate>(queued.outcome));
        rejected = false;
        try {
            (void)queue.completeSessionService(first.header.identity, observed, receipt(first.header), wire.data(), wire.size());
        } catch (const QueueError& cause) {
            rejected = cause.code() == QueueErrorCode::unexpectedCompletion;
        }
        require(rejected);
        queue.submit(request(2));
        require(queue.beginService(9).has_value());
        queue.submit(request(3));
        const auto second = request(2);
        const auto staleWire = response(second.header);
        rejected = false;
        try {
            (void)queue.completeSessionService(second.header.identity, observed, receipt(second.header), staleWire.data(), staleWire.size());
        } catch (const AdmissionError& cause) {
            rejected = cause.code() == AdmissionErrorCode::staleRequest;
        }
        const auto next = queue.beginService(10);
        require(rejected && next && next->header.identity.request == 3);
        queue.cancel(next->header.identity);
        const auto retiredWire = response(next->header);
        rejected = false;
        try {
            (void)queue.completeSessionService(next->header.identity, { 10, 1234, true }, receipt(next->header), retiredWire.data(), retiredWire.size());
        } catch (const AdmissionError& cause) {
            rejected = cause.code() == AdmissionErrorCode::retired;
        }
        require(rejected && !queue.beginService(10));
        RequestQueue child;
        child.submit(first);
        require(child.begin().has_value());
        rejected = false;
        try {
            (void)child.completeSessionService(first.header.identity, observed, receipt(first.header), wire.data(), wire.size());
        } catch (const QueueError& cause) {
            rejected = cause.code() == QueueErrorCode::wrongCompletionMode;
        }
        require(rejected);
        child.abandon(first.header.identity);

    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

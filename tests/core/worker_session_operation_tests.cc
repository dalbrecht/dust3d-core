// SPDX-License-Identifier: MIT
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerAdmission.h>
#include <native/Engine/WorkerSessionOperation.h>
#include <native/Engine/WorkerSourceRequest.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Session operation contract");
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2);
        std::ifstream input(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(static_cast<bool>(input));
        std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        Frame request;
        request.header.operation = Operation::generateSessionIndexedSourcedMesh;
        request.header.identity.document[0] = 1;
        request.header.identity.request = 1;
        request.header.identity.revision = 42;
        request.payload = encodeSourceRequest({ archive, 0 }, {});
        request.header.payloadBytes = request.payload.size();
        ConnectionGenerationSession session(request.header.identity.document, 7, 64ULL * 1024 * 1024);
        auto invalid = request;
        invalid.header.operation = Operation::generateMesh;
        bool rejected = false;
        try {
            (void)generateSessionReply(invalid, session, 7, {});
        } catch (const ProtocolError& cause) {
            rejected = cause.code() == ProtocolErrorCode::unsupportedOperation;
        }
        require(rejected);
        invalid = request;
        invalid.header.kind = FrameKind::response;
        rejected = false;
        try {
            (void)generateSessionReply(invalid, session, 7, {});
        } catch (const ProtocolError& cause) {
            rejected = cause.code() == ProtocolErrorCode::unexpectedResponse;
        }
        require(rejected);
        invalid = request;
        ++invalid.header.payloadBytes;
        rejected = false;
        try {
            (void)generateSessionReply(invalid, session, 7, {});
        } catch (const ProtocolError& cause) {
            rejected = cause.code() == ProtocolErrorCode::invalidInput;
        }
        require(rejected);
        SessionOutputLimits tiny;
        tiny.maximumWireBytes = 128;
        rejected = false;
        try {
            (void)generateSessionReply(request, session, 7, tiny);
        } catch (const SessionOutputError& cause) {
            rejected = cause.code() == SessionOutputErrorCode::limitExceeded;
        }
        require(rejected && session.statistics(7).retainedPayloadBytes == 0);
        for (int mode = 0; mode < 3; ++mode) {
            SessionOutputLimits unsupported;
            if (!mode)
                unsupported.maximumDecodedBytes = 1;
            else if (mode == 1)
                unsupported.maximumWireBytes = sessionOutputPrefixBytes + unsupported.failure.maximumWireBytes + generationStatisticsPrefixBytes + unsupported.failure.maximumWireBytes + unsupported.failure.maximumWireBytes - 1;
            else
                unsupported.maximumDecodedBytes = unsupported.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes + std::max(unsupported.statistics.maximumCauseBytes, unsupported.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes) + unsupported.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes - 1;
            rejected = false;
            try {
                (void)generateSessionReply(request, session, 7, unsupported);
            } catch (const SessionOutputError& cause) {
                rejected = cause.code() == SessionOutputErrorCode::limitExceeded;
            }
            require(rejected && session.statistics(7).retainedPayloadBytes == 0);
        }
        auto first = generateSessionReply(request, session, 7, {});
        validateResponseIdentity(first.header, request.header);
        require(first.header.kind == FrameKind::response && first.header.payloadBytes == first.payload.size());
        ++request.header.identity.request;
        request.payload = encodeSourceRequest({ archive, 32ULL * 1024 * 1024 }, {});
        request.header.payloadBytes = request.payload.size();
        auto second = generateSessionReply(request, session, 7, {});
        validateResponseIdentity(second.header, request.header);
        session.close();
        const auto result = decodeSessionOutput(second.payload.data(), second.payload.size(), {});
        require(std::holds_alternative<IndexedSourcesPayload>(result.outcome)
            && result.statistics && result.statistics->reusedComponents > 0
            && !result.sourceIssue && !result.statisticsIssue);
        const auto wire = encodeFrame(second, {});
        for (auto kind : { FrameKind::response, FrameKind::failure }) {
            auto unsupported = second;
            unsupported.header.kind = kind;
            if (kind == FrameKind::failure)
                unsupported.payload = encodeFailurePayload(FailurePayload {}, {});
            unsupported.header.payloadBytes = unsupported.payload.size();
            const auto framed = encodeFrame(unsupported, {});
            PendingReply pending(request.header);
            bool rejected = false;
            try {
                (void)pending.complete(request.header.identity, { true, 0 }, framed.data(), framed.size());
            } catch (const ProtocolError& cause) {
                rejected = cause.code() == ProtocolErrorCode::unsupportedOperation;
            }
            require(rejected && pending.isRetired());
        }
        const auto copied = decodeFrame(wire.data(), wire.size(), {});
        require(copied.header.identity == request.header.identity && copied.payload == second.payload);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

// SPDX-License-Identifier: MIT
#include "WorkerSessionOperation.h"
#include "WorkerSessionOutputCopy.h"
#include "WorkerSourceRequest.h"
#include <algorithm>
namespace dust3d {
namespace worker {
    namespace {
        uint64_t remaining(uint64_t maximum, uint64_t reservation)
        {
            if (maximum <= reservation)
                throw SessionOutputError(SessionOutputErrorCode::limitExceeded, "Session output policy leaves no generation allowance after diagnostic reservation.");
            return maximum - reservation;
        }
        NativeSessionOptions generationPolicy(const SessionOutputLimits& limits)
        {
            // Validate all inner/aggregate policies and the detailed diagnostic prefix
            // before native work. Reservations are logical allowances, not RSS caps.
            (void)encodeSessionOutput(SessionOutput {}, limits);
            (void)encodeFailurePayload(FailurePayload { FailureStatus::generationFailed, {}, FailureDetail { FailureDomain::standard, 0, 0, 0 } }, limits.failure);
            const auto diagnosticWire = limits.failure.maximumWireBytes + std::max(limits.statistics.maximumWireBytes, generationStatisticsPrefixBytes + limits.failure.maximumWireBytes);
            const auto diagnosticDecoded = limits.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes + std::max(limits.statistics.maximumCauseBytes, limits.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes);
            const auto wire = remaining(limits.maximumWireBytes, sessionOutputPrefixBytes + diagnosticWire);
            const auto decoded = remaining(limits.maximumDecodedBytes, diagnosticDecoded);
            if (wire < limits.failure.maximumWireBytes || decoded < limits.failure.maximumCauseBytes + maximumFailureResourceIdentityBytes)
                throw SessionOutputError(SessionOutputErrorCode::limitExceeded, "Session output policy cannot retain an original primary failure alongside diagnostics.");
            NativeSessionOptions result;
            result.output = limits.sourced;
            result.output.maximumWireBytes = std::min(result.output.maximumWireBytes, wire);
            result.output.maximumDecodedBytes = std::min(result.output.maximumDecodedBytes, decoded);
            result.ordinary = limits.ordinary;
            result.ordinary.maximumWireBytes = std::min(result.ordinary.maximumWireBytes, wire);
            result.ordinary.maximumDecodedBytes = std::min(result.ordinary.maximumDecodedBytes, decoded);
            result.statistics = limits.statistics;
            return result;
        }
    }
    Frame generateSessionReply(const Frame& request, ConnectionGenerationSession& session, uint64_t connectionGeneration, const SessionOutputLimits& limits)
    {
        (void)encodeHeader(request.header, {});
        if (request.header.operation != Operation::generateSessionIndexedSourcedMesh)
            throw ProtocolError(ProtocolErrorCode::unsupportedOperation, "Session generation requires its exact operation.");
        if (request.header.kind != FrameKind::request)
            throw ProtocolError(ProtocolErrorCode::unexpectedResponse, "Session generation requires a request frame.");
        if (request.header.payloadBytes != request.payload.size())
            throw ProtocolError(ProtocolErrorCode::invalidInput, "Session request body length disagrees with its header.");
        auto policy = generationPolicy(limits);
        const auto decoded = decodeSourceRequest(request.payload.data(), request.payload.size(), {});
        const auto requested = decoded.maximumSourceBytes ? decoded.maximumSourceBytes : DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES;
        policy.output.sources.maximumDecodedBytes = std::min(policy.output.sources.maximumDecodedBytes, requested);
        auto generated = session.generateSourcedIndexed(decoded.archive, request.header.identity, connectionGeneration, policy);
        auto output = copySessionOutput(std::move(generated), limits.failure);
        auto payload = encodeSessionOutput(output, limits);
        auto header = request.header;
        header.kind = FrameKind::response;
        header.payloadBytes = payload.size();
        return { header, std::move(payload) };
    }
}
}

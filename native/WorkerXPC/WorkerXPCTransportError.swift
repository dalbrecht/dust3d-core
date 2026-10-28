// SPDX-License-Identifier: MIT

/// Local transport lifecycle failures, independent of worker preview/legacy receipt domains.
public enum WorkerXPCTransportError: Error {
    case retired, busy, invalidGeneration, generationMismatch, sequenceExhausted
    case invalidServiceIdentifier, emptySigningRequirement
    case signingRequirement(status: Int32)
    case missingProxy, connectionInvalidated, connectionInterrupted, invalidChannels, invalidRequest
    case diagnosticExceeded(actual: Int)
    case remote(any Error)
    case missingPeer
    case peerChanged(expected: Int32, observed: Int32)
}

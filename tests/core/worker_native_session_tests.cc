// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerNativeSession.h>
using namespace dust3d::worker;
int main(int argc, char** argv)
{
    try {
        if (argc != 2)
            throw std::runtime_error("Existing licensed fixture directory required");
        std::ifstream input(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        if (!input)
            throw std::runtime_error("Ordinary Capsule fixture opens");
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        RequestIdentity identity;
        identity.document[0] = 1;
        identity.request = 1;
        NativeGenerationSession session(identity.document, 64ULL * 1024 * 1024);
        const auto first = session.generateSourcedIndexed(archive, identity, {});
        ++identity.request;
        const PreviewArchiveView borrowed { { archive.data(), archive.size(), 0 }, 0 };
        const auto second = session.generatePreview(borrowed, identity);
        if (!first.mesh || !second.mesh || !second.statistics || second.statistics->reusedComponents == 0
            || first.mesh->geometry.vertices.empty() || second.mesh->geometry.vertices.empty())
            throw std::runtime_error("Worker-owned session must retain actual cache reuse and return independent indexed/source values");
        if (encodeIndexedSourcesPayload(*first.mesh, {}) != encodeIndexedSourcesPayload(*second.mesh, {}))
            throw std::runtime_error("Native-budget preview must preserve the original same-fixture pair");
        Dust3DError nativeError {};
        std::unique_ptr<Dust3DGenerationSession, decltype(&dust3d_generation_session_destroy)> ordinaryOwner(
            dust3d_generation_session_create(0, &nativeError), dust3d_generation_session_destroy);
        if (!ordinaryOwner || nativeError.status != DUST3D_STATUS_OK)
            throw std::runtime_error("Healthy ordinary fixture session opens");
        auto ordinary = generateSessionPreviewIndexedMesh(ordinaryOwner.get(), borrowed);
        ordinaryOwner.reset();
        if (!std::holds_alternative<IndexedMeshPayload>(ordinary)
            || encodeIndexedMeshPayload(std::get<IndexedMeshPayload>(ordinary), {}) != encodeIndexedMeshPayload(first.mesh->geometry, {}))
            throw std::runtime_error("Healthy ordinary fallback adapter preserves independent original geometry");
        try {
            (void)generateSessionPreviewIndexedMesh(nullptr, borrowed);
            throw std::runtime_error("Missing ordinary session accepted");
        } catch (const ProtocolError& cause) {
            if (cause.code() != ProtocolErrorCode::invalidInput)
                throw;
        }
        const auto before = session.statistics();
        auto otherDocument = identity;
        otherDocument.document[0] = 2;
        bool wrongDocumentRejected = false;
        try {
            (void)session.generatePreview(borrowed, otherDocument);
        } catch (const ProtocolError& cause) {
            wrongDocumentRejected = cause.code() == ProtocolErrorCode::invalidIdentity;
        }
        const auto after = session.statistics();
        if (!wrongDocumentRejected || before.retainedPayloadBytes != after.retainedPayloadBytes
            || before.reusedComponents != after.reusedComponents)
            throw std::runtime_error("Wrong-document admission must reject before native generation and preserve cache observations");
        bool invalidOwnerRejected = false;
        try {
            NativeGenerationSession invalid({}, 64);
        } catch (const ProtocolError&) {
            invalidOwnerRejected = true;
        }
        if (!invalidOwnerRejected)
            throw std::runtime_error("Empty host document scope rejects before owner creation");
        try {
            (void)session.generatePreview({ { nullptr, archive.size(), 0 }, 0 }, identity);
            throw std::runtime_error("Missing borrowed archive reached native work");
        } catch (const ProtocolError& cause) {
            if (cause.code() != ProtocolErrorCode::invalidInput)
                throw;
        }
        auto missingRequest = identity;
        missingRequest.request = 0;
        try {
            (void)session.generatePreview(borrowed, missingRequest);
            throw std::runtime_error("Missing preview request identity accepted");
        } catch (const ProtocolError& cause) {
            if (cause.code() != ProtocolErrorCode::invalidIdentity)
                throw;
        }
        try {
            (void)session.generatePreview({ { archive.data(), archive.size(), 257ULL * 1024 * 1024 }, 0 }, identity);
            throw std::runtime_error("Invalid captured source policy reached native work");
        } catch (const PreviewChunkError& cause) {
            if (cause.code() != PreviewChunkErrorCode::invalidLimits)
                throw;
        }
        try {
            (void)session.generatePreview(borrowed, identity, { 0, 0 });
            throw std::runtime_error("Invalid statistics policy reached native work");
        } catch (const GenerationStatisticsError& cause) {
            if (cause.code() != GenerationStatisticsErrorCode::invalidLimits)
                throw;
        }
        const auto afterArguments = session.statistics();
        if (afterArguments.retainedPayloadBytes != before.retainedPayloadBytes || afterArguments.reusedComponents != before.reusedComponents)
            throw std::runtime_error("Rejected preview controls must leave actual cache observations unchanged");
        session.reset();
        if (session.statistics().retainedPayloadBytes != 0 || first.mesh->geometry.vertices.empty())
            throw std::runtime_error("Reset clears native retention without invalidating previously copied output");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

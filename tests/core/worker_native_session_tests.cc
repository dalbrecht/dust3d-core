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
        const auto second = session.generateSourcedIndexed(archive, identity, {});
        if (!first.mesh || !second.mesh || !second.statistics || second.statistics->reusedComponents == 0
            || first.mesh->geometry.vertices.empty() || second.mesh->geometry.vertices.empty())
            throw std::runtime_error("Worker-owned session must retain actual cache reuse and return independent indexed/source values");
        const auto before = session.statistics();
        auto otherDocument = identity;
        otherDocument.document[0] = 2;
        bool wrongDocumentRejected = false;
        try {
            (void)session.generateSourcedIndexed(archive, otherDocument, {});
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
        session.reset();
        if (session.statistics().retainedPayloadBytes != 0 || first.mesh->geometry.vertices.empty())
            throw std::runtime_error("Reset clears native retention without invalidating previously copied output");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerLegacyOperation.h>
#include <native/Engine/WorkerLegacyRequest.h>
#include <stdexcept>
using namespace dust3d::worker;
int main(int argc, char** argv)
{
    try {
        if (argc != 2)
            throw std::runtime_error("Positive fixture directory required");
        std::ifstream input(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        if (!input)
            throw std::runtime_error("Existing ordinary fixture opens");
        LegacyRequest value { { std::istreambuf_iterator<char>(input), {} }, {} };
        value.options.atlasTextureSize = value.options.colorSize = value.options.normalSize = value.options.materialSize = 8;
        Frame request;
        request.header.operation = Operation::generateLegacyBaked;
        request.header.identity.document[0] = 1;
        request.header.identity.request = 7;
        request.payload = encodeLegacyRequest(value, {});
        request.header.payloadBytes = request.payload.size();
        const auto reply = generateLegacyBakedReply(request, nullptr, {});
        validateResponseIdentity(reply.header, request.header);
        if (reply.header.kind != FrameKind::response || decodeLegacyBakedPayload(reply.payload.data(), reply.payload.size(), {}).geometry.vertices.empty())
            throw std::runtime_error("Correlated legacy operation preserves complete positive rendering without unnecessary image decoding");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

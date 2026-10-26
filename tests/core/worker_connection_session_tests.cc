// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerConnectionSession.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Connection session contract");
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2);
        std::ifstream input(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(static_cast<bool>(input));
        std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
        RequestIdentity request;
        request.document[0] = 1;
        request.request = 1;
        ConnectionGenerationSession session(request.document, 7, 64ULL * 1024 * 1024);
        auto first = session.generateSourcedIndexed(archive, request, 7, {});
        request.request = 2;
        auto second = session.generateSourcedIndexed(archive, request, 7, {});
        require(first.mesh && second.mesh && second.statistics && second.statistics->reusedComponents > 0);
        session.reset(7);
        require(session.statistics(7).retainedPayloadBytes == 0);
        bool rejected = false;
        try {
            (void)session.generateSourcedIndexed(archive, request, 7, {});
        } catch (const SessionScopeError& cause) {
            rejected = cause.code() == SessionScopeErrorCode::reusedRequest;
        }
        require(rejected);
        request.request = 3;
        auto afterReset = session.generateSourcedIndexed(archive, request, 7, {});
        require(afterReset.mesh && afterReset.statistics && afterReset.statistics->reusedComponents == 0);
        session.close();
        session.close();
        rejected = false;
        try {
            (void)session.statistics(7);
        } catch (const SessionScopeError& cause) {
            rejected = cause.code() == SessionScopeErrorCode::closed;
        }
        require(rejected && !first.mesh->geometry.vertices.empty());
        ConnectionGenerationSession independent(request.document, 8, 0);
        request.request = 1;
        auto disabled = independent.generateSourcedIndexed(archive, request, 8, {});
        require(disabled.mesh && disabled.statistics && disabled.statistics->maximumCachePayloadBytes == 0 && disabled.statistics->retainedPayloadBytes == 0);
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

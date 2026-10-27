// SPDX-License-Identifier: MIT
#include <cstdio>
#include <iostream>
#include <memory>
#include <native/Engine/WorkerJobIO.h>
#include <unistd.h>
using namespace dust3d::worker;
void require(bool value)
{
    if (!value)
        throw std::runtime_error("Host request expectation contract");
}
using File = std::unique_ptr<FILE, decltype(&std::fclose)>;
int main()
{
    try {
        Frame request;
        request.header.operation = Operation::generateSessionIndexedSourcedMesh;
        request.header.identity.document[0] = 1;
        request.header.identity.request = 7;
        request.header.identity.revision = 3;
        // Tiny protocol values only; never passed to native generation.
        request.payload = { 1, 2, 3 };
        request.header.payloadBytes = request.payload.size();
        const auto wire = encodeFrame(request, {});
        for (int mode = 0; mode < 6; ++mode) {
            File input(std::tmpfile(), std::fclose), output(std::tmpfile(), std::fclose);
            require(input && output);
            require(std::fwrite(wire.data(), 1, wire.size(), input.get()) == wire.size());
            require(!std::fflush(input.get()) && !std::fseek(input.get(), 0, SEEK_SET));
            WorkerJobExpectation expected { request.header.operation, request.header.identity };
            if (mode == 1)
                expected.identity.document[0] = 2;
            else if (mode == 2)
                ++expected.identity.request;
            else if (mode == 3)
                ++expected.identity.revision;
            else if (mode == 4)
                expected.operation = Operation::generateMesh;
            else if (mode == 5)
                expected.identity.request = 0;
            WorkerJobContext context;
            bool rejected = false;
            try {
                const auto copied = readWorkerRequest(fileno(input.get()), fileno(output.get()), context, &expected);
                require(!mode && copied.payload == request.payload);
            } catch (const ProtocolError& cause) {
                rejected = cause.code() == ProtocolErrorCode::invalidIdentity;
            }
            require(rejected == (mode != 0));
            require(context.requestIdentity == request.header.identity.request && context.operation == request.header.operation);
            require(::lseek(fileno(input.get()), 0, SEEK_CUR) == static_cast<off_t>(mode ? headerBytes : wire.size()));
            require(::lseek(fileno(output.get()), 0, SEEK_CUR) == 0);
        }
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

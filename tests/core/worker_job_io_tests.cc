// SPDX-License-Identifier: MIT
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerJobIO.h>
#include <native/Engine/WorkerUvMesh.h>
#include <native/Engine/WorkerUvRequest.h>
#include <stdexcept>
#include <unistd.h>

using namespace dust3d::worker;
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
class TemporaryChannel {
public:
    explicit TemporaryChannel(const std::string& root)
    {
        std::string pattern = root + "/worker-job-XXXXXX";
        std::vector<char> name(pattern.begin(), pattern.end());
        name.push_back(0);
        m_descriptor = ::mkstemp(name.data());
        require(m_descriptor >= 0, "Owned temporary job channel opens");
        ::unlink(name.data());
    }
    ~TemporaryChannel()
    {
        if (m_descriptor >= 0)
            ::close(m_descriptor);
    }
    TemporaryChannel(const TemporaryChannel&) = delete;
    TemporaryChannel& operator=(const TemporaryChannel&) = delete;
    int descriptor() const { return m_descriptor; }
    void rewind() { require(::lseek(m_descriptor, 0, SEEK_SET) == 0, "Channel rewinds"); }

private:
    int m_descriptor = -1;
};
void job(const std::vector<uint8_t>& archive, const std::string& scratch, Operation operation)
{
    TemporaryChannel input(scratch), output(scratch);
    Header header;
    header.operation = operation;
    header.identity.document[0] = 1;
    header.identity.request = 42;
    std::vector<uint8_t> body = archive;
    if (operation == Operation::generateUvMesh) {
        UvRequest request;
        request.archive = archive;
        request.textureSize = 64;
        body = encodeUvRequest(request, {});
    }
    header.payloadBytes = body.size();
    const auto wire = encodeFrame({ header, body }, {});
    require(writeWorkerBytes(input.descriptor(), wire.data(), wire.size()).complete, "Ordinary request writes");
    input.rewind();
    WorkerJobContext context;
    serveWorkerJob(input.descriptor(), output.descriptor(), context);
    require(context.requestIdentity == 42 && context.operation == operation, "Job context preserves actual request and operation");
    require(::fcntl(input.descriptor(), F_GETFD) >= 0 && ::fcntl(output.descriptor(), F_GETFD) >= 0, "Job borrows both channels without closing them");
    output.rewind();
    std::vector<uint8_t> reply;
    uint8_t buffer[4096];
    ssize_t count;
    while ((count = ::read(output.descriptor(), buffer, sizeof(buffer))) > 0)
        reply.insert(reply.end(), buffer, buffer + count);
    require(count == 0 && reply.size() < 1024 * 1024, "Small ordinary reply reaches EOF");
    const auto response = decodeFrame(reply.data(), reply.size(), {});
    validateResponseIdentity(response.header, header);
    require(response.header.kind == FrameKind::response, "Ordinary descriptor job succeeds");
    if (operation == Operation::generateUvMesh) {
        const auto pair = decodeUvMeshPayload(response.payload.data(), response.payload.size(), {});
        require(pair.geometry.vertices.size() == 240 && pair.uv.triangles.size() == 80, "Reusable runtime retains paired geometry/UV order");
    } else
        require(decodeMeshPayload(response.payload.data(), response.payload.size(), {}).vertices.size() == 240, "Reusable runtime retains base geometry");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Licensed Capsule and shared scratch root required");
        require(std::filesystem::file_size(argv[1]) < 1024 * 1024, "Licensed fixture stays small before copying");
        std::ifstream source(argv[1], std::ios::binary);
        require(bool(source), "Capsule opens");
        const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(source), {} };
        require(archive.size() < 1024 * 1024, "Licensed input stays small");
        require(std::string(workerOperationName(std::nullopt)) == "unknown", "No observed header cannot invent operation");
        require(std::string(workerOperationName(Operation::generateMesh)) == "generateMesh", "Base operation name exact");
        require(std::string(workerOperationName(Operation::generateUvMesh)) == "generateUvMesh", "UV operation name exact");
        require(std::string(workerOperationName(static_cast<Operation>(99))) == "unknown", "Unsupported diagnostic observations remain unknown");
        const WorkerJobIOError original(WorkerJobStage::input, EACCES);
        require(original.stage() == WorkerJobStage::input && original.systemError() == EACCES && original.what()[0], "I/O error preserves original synthetic cause without OS actions");
        const WorkerJobIOError outputError(WorkerJobStage::output, EIO);
        require(outputError.what()[0], "Output error has a stable diagnostic");
        require(writeWorkerBytes(-1, nullptr, 0).complete, "Empty span performs no I/O");
        require(!writeWorkerBytes(-1, nullptr, 1).complete, "Invalid negative descriptor rejects before any syscall");
        TemporaryChannel valid(argv[2]);
        require(writeWorkerBytes(valid.descriptor(), nullptr, 1).error == EINVAL, "Missing span rejects before any syscall");
        WorkerJobContext rejected;
        rejected.requestIdentity = 99;
        rejected.operation = Operation::generateUvMesh;
        try {
            serveWorkerJob(-1, valid.descriptor(), rejected);
            throw std::runtime_error("Invalid channel admitted");
        } catch (const ProtocolError& cause) {
            require(cause.code() == ProtocolErrorCode::invalidInput && rejected.requestIdentity == 0 && !rejected.operation, "Invalid job resets stale context before I/O");
        }
        job(archive, argv[2], Operation::generateMesh);
        job(archive, argv[2], Operation::generateUvMesh);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <Dust3DWorkerRuntime.h>
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
    const auto result = dust3d_worker_serve(input.descriptor(), output.descriptor());
    require(result.status == DUST3D_WORKER_JOB_COMPLETE && result.request_identity == 42 && result.system_error == 0
            && result.detail_source == DUST3D_WORKER_DETAIL_NONE && result.message[0] == 0,
        "C runtime returns an independent complete-frame result");
    require(result.operation == (operation == Operation::generateMesh ? DUST3D_WORKER_OPERATION_MESH : DUST3D_WORKER_OPERATION_UV_MESH), "C operation identity copied exactly");
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
        require(dust3d_worker_runtime_abi_version() == 1, "C ABI layout version is explicit");
        struct DirectoryDescriptor {
            int value;
            ~DirectoryDescriptor()
            {
                if (value >= 0)
                    ::close(value);
            }
        } directory { ::open(argv[2], O_RDONLY | O_DIRECTORY | O_CLOEXEC) };
        require(directory.value >= 0, "Owned scratch directory opens for ordinary I/O rejection");
        TemporaryChannel directoryOutput(argv[2]);
        const auto ioError = dust3d_worker_serve(directory.value, directoryOutput.descriptor());
        require(ioError.status == DUST3D_WORKER_JOB_IO_FAILED && ioError.system_error == EISDIR
                && ioError.io_stage == DUST3D_WORKER_IO_INPUT && ioError.detail_source == DUST3D_WORKER_DETAIL_SYSTEM
                && ioError.operation == DUST3D_WORKER_OPERATION_UNKNOWN && ioError.request_identity == 0 && ioError.message[0],
            "Ordinary directory-read rejection retains original errno/stage/context across C");
        require(::fcntl(directory.value, F_GETFD) >= 0 && ::lseek(directoryOutput.descriptor(), 0, SEEK_END) == 0,
            "Failed input leaves borrowed channel ownership and output unchanged");
        TemporaryChannel malformedInput(argv[2]), malformedOutput(argv[2]);
        Header malformedHeader;
        malformedHeader.operation = Operation::generateUvMesh;
        malformedHeader.identity.document[0] = 1;
        malformedHeader.identity.request = 43;
        malformedHeader.payloadBytes = 3;
        const auto malformedWire = encodeFrame({ malformedHeader, { 1, 2, 3 } }, {});
        require(writeWorkerBytes(malformedInput.descriptor(), malformedWire.data(), malformedWire.size()).complete, "Pure malformed UV codec fixture writes");
        malformedInput.rewind();
        const auto rejectedCodec = dust3d_worker_serve(malformedInput.descriptor(), malformedOutput.descriptor());
        require(rejectedCodec.status == DUST3D_WORKER_JOB_CODEC_REJECTED && rejectedCodec.detail_source == DUST3D_WORKER_DETAIL_UV_REQUEST
                && rejectedCodec.detail_code == static_cast<int32_t>(UvRequestErrorCode::truncated) && rejectedCodec.request_identity == 43 && rejectedCodec.operation == DUST3D_WORKER_OPERATION_UV_MESH && rejectedCodec.message[0],
            "Pre-generation codec rejection preserves original source/context across C");
        job(archive, argv[2], Operation::generateMesh);
        job(archive, argv[2], Operation::generateUvMesh);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

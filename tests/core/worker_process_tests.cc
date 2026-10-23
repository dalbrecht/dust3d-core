// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <native/Engine/WorkerFailure.h>
#include <native/Engine/WorkerMesh.h>
#include <stdexcept>
#include <string>

namespace {
using namespace dust3d::worker;
const std::vector<std::string> cases { "capsule", "sword", "seam", "badArchive", "emptyArchive", "emptyStream", "shortHeader", "shortBody", "trailing", "wrongKind", "version", "quota", "identity", "inputDirectory", "usage" };
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
std::vector<uint8_t> readFile(const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size(path);
    require(size <= 1024 * 1024, "Worker process fixture/output must remain bounded");
    std::ifstream file(path, std::ios::binary);
    require(bool(file), "Cannot open worker process fixture/output");
    std::vector<uint8_t> result(static_cast<size_t>(size));
    if (!result.empty())
        file.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size()));
    require(!file.bad() && static_cast<size_t>(file.gcount()) == size && file.peek() == EOF, "Worker fixture/output changed or was truncated");
    return result;
}
void writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    require(bool(file), "Cannot create bounded worker input fixture");
    if (!bytes.empty())
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(bool(file), "Cannot write bounded worker input fixture");
}
Header request(const std::string& name)
{
    const auto found = std::find(cases.begin(), cases.end(), name);
    require(found != cases.end(), "Unknown worker subprocess case");
    Header header;
    header.identity.document[0] = 1;
    header.identity.request = static_cast<uint64_t>(std::distance(cases.begin(), found)) + 1;
    header.identity.revision = 3;
    return header;
}
void prepare(const std::filesystem::path& fixtures, const std::filesystem::path& scratch)
{
    std::filesystem::create_directories(scratch);
    const auto capsule = readFile(fixtures / "Capsule.ds3");
    const auto sword = readFile(fixtures / "Sword.ds3");
    for (const auto& name : cases) {
        if (name == "inputDirectory") {
            std::filesystem::create_directories(scratch / (name + ".request"));
            continue;
        }
        const std::vector<uint8_t> body = name == "capsule" || name == "seam" || name == "usage" ? capsule : name == "sword" ? sword
            : name == "emptyArchive"                                                                                         ? std::vector<uint8_t> {}
                                                                                                                             : std::vector<uint8_t> { 1, 2, 3 };
        auto header = request(name);
        header.payloadBytes = body.size();
        auto bytes = encodeFrame({ header, body }, {});
        if (name == "emptyStream")
            bytes.clear();
        else if (name == "shortHeader")
            bytes.resize(3);
        else if (name == "shortBody")
            bytes.pop_back();
        else if (name == "trailing")
            bytes.push_back(255);
        else if (name == "wrongKind") {
            header.kind = FrameKind::response;
            bytes = encodeFrame({ header, body }, {});
        } else if (name == "version")
            bytes[9] = 2;
        else if (name == "quota") {
            header.payloadBytes = Limits {}.maximumInputBytes + 1;
            Limits allowed;
            allowed.maximumInputBytes = maximumPayloadBytes;
            const auto advertised = encodeHeader(header, allowed);
            bytes.assign(advertised.begin(), advertised.end());
        } else if (name == "identity")
            std::fill(bytes.begin() + 40, bytes.begin() + 56, 0);
        writeFile(scratch / (name + ".request"), bytes);
    }
}
void verify(const std::filesystem::path& fixtures, const std::filesystem::path& scratch, const std::string& name)
{
    auto wire = readFile(scratch / (name + ".reply"));
    const auto diagnostic = readFile(scratch / (name + ".error"));
    const bool isMesh = name == "capsule" || name == "seam" || name == "sword";
    const bool isNativeFailure = name == "badArchive" || name == "emptyArchive";
    if (!isMesh && !isNativeFailure) {
        require(wire.empty(), "Rejected request must not publish a partial/forged response");
        const std::string text(diagnostic.begin(), diagnostic.end());
        require(!text.empty() && text.size() <= 1024 && text.find(name == "usage" ? "stage=usage" : name == "inputDirectory" ? "stage=input"
                                                                                                                             : "stage=protocol")
                    != std::string::npos,
            "Controlled protocol rejection preserves a bounded transport cause");
        require(std::all_of(text.begin(), text.end(), [](unsigned char byte) { return byte == '\n' || (byte >= 32 && byte <= 126); }), "Transport diagnostics contain static printable descriptions only");
        return;
    }
    require(diagnostic.empty(), "Engine/native diagnostic content must not leak to stderr");
    auto frame = decodeFrame(wire.data(), wire.size(), {});
    validateResponseIdentity(frame.header, request(name));
    if (isMesh) {
        require(frame.header.kind == FrameKind::response, "Valid archive must produce a mesh response");
        const auto mesh = decodeMeshPayload(frame.payload.data(), frame.payload.size(), {});
        wire.clear();
        frame.payload.clear();
        const auto archive = readFile(fixtures / (name == "sword" ? "Sword.ds3" : "Capsule.ds3"));
        Dust3DError error {};
        std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> expected(dust3d_mesh_load(archive.data(), archive.size(), &error), dust3d_mesh_destroy);
        require(expected != nullptr, error.message);
        require((name == "sword" || mesh.vertices.size() == 240) && mesh.vertices.size() == dust3d_mesh_vertex_count(expected.get())
                && mesh.nodeCount == dust3d_mesh_node_count(expected.get()) && mesh.partCount == dust3d_mesh_part_count(expected.get())
                && mesh.warning == dust3d_mesh_warning(expected.get()),
            "Subprocess result must match actual native counts/warning");
        const auto* vertices = dust3d_mesh_vertices(expected.get());
        for (size_t index = 0; index < mesh.vertices.size(); ++index)
            require(std::memcmp(mesh.vertices[index].position.data(), vertices[index].position, 3 * sizeof(float)) == 0
                    && std::memcmp(mesh.vertices[index].normal.data(), vertices[index].normal, 3 * sizeof(float)) == 0
                    && std::memcmp(mesh.vertices[index].color.data(), vertices[index].color, 4 * sizeof(float)) == 0,
                "Subprocess geometry must preserve every admitted native float bit");
    } else {
        require(frame.header.kind == FrameKind::failure, "Native input error must remain a typed operation outcome");
        const auto failure = decodeFailurePayload(frame.payload.data(), frame.payload.size(), {});
        const auto archive = name == "emptyArchive" ? std::vector<uint8_t> {} : std::vector<uint8_t> { 1, 2, 3 };
        Dust3DError error {};
        std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)> expected(dust3d_mesh_load(archive.data(), archive.size(), &error), dust3d_mesh_destroy);
        require(!expected && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Reference invalid archive must fail normally");
        const auto end = std::find(std::begin(error.message), std::end(error.message), '\0');
        require(failure.status == FailureStatus::invalidDocument && failure.cause == std::vector<uint8_t>(error.message, end),
            "Native cause must survive the actual process boundary without replacement");
    }
}
}
int main(int argc, char** argv)
{
    try {
        require(argc >= 4, "Expected prepare/verify, fixtures and scratch paths");
        const std::string mode = argv[1];
        if (mode == "prepare" && argc == 4)
            prepare(argv[2], argv[3]);
        else if (mode == "verify" && argc == 5)
            verify(argv[2], argv[3], argv[4]);
        else
            throw std::runtime_error("Unknown worker subprocess test invocation");
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

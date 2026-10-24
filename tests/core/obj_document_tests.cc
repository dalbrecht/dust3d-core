// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxGeometrySelection.h"
#include "GenerationPipeline.h"
#include "ObjDocument.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>
using namespace dust3d;
using namespace dust3d::native;
static_assert(!std::is_copy_constructible_v<OwnedObjDocument> && std::is_nothrow_move_constructible_v<OwnedObjDocument>);
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "OBJ rejection lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid OBJ source accepted");
}
Object fixture()
{
    Object object;
    object.vertices = { { -1.25, 0, 0 }, { 2.5, 0, 0 }, { 2.5, 3, 0 }, { -1.25, 3, 0 }, { 0, 0, 4 } };
    object.triangleAndQuads = { { 0, 1, 2, 3 }, { 0, 3, 4 } };
    object.triangles = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 3, 4 } };
    return object;
}
void reimport(const Object& object, const OwnedObjDocument& document)
{
    std::istringstream input(std::string(document.bytes().begin(), document.bytes().end()));
    std::vector<Vector3> vertices;
    std::vector<std::vector<size_t>> faces;
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream record(line);
        record.imbue(std::locale::classic());
        std::string kind;
        record >> kind;
        if (kind == "v") {
            double x = 0, y = 0, z = 0;
            require(bool(record >> x >> y >> z), "Invalid independent OBJ position");
            vertices.emplace_back(x, y, z);
        } else if (kind == "f") {
            std::vector<size_t> face;
            size_t index = 0;
            while (record >> index) {
                require(index && index <= vertices.size(), "Invalid one-based OBJ index");
                face.push_back(index - 1);
            }
            require(face.size() == 3 || face.size() == 4, "OBJ lost original polygon arity");
            faces.push_back(std::move(face));
        } else {
            require(kind == "#", "OBJ gained non-reference attribute records");
        }
    }
    require(faces == object.triangleAndQuads && vertices.size() == object.vertices.size(), "OBJ polygon indices/order changed");
    for (size_t index = 0; index < vertices.size(); ++index) {
        for (size_t axis = 0; axis < 3; ++axis)
            require(vertices[index][axis] == object.vertices[index][axis], "OBJ double coordinate did not round-trip exactly");
    }
}
class DecimalComma : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
};
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2 || argc == 3, "Ordinary fixture directory required; optional controlled OBJ output");
        auto source = fixture();
        auto output = encodeObjDocument(source);
        reimport(source, output);
        const auto payload = output.payloadBytes();
        reimport(source, encodeObjDocument(source, payload));
        rejects([&] { encodeObjDocument(source, payload - 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        source.vertices[0][0] = 1000000.123456789;
        const auto previousLocale = std::locale();
        std::locale::global(std::locale(previousLocale, new DecimalComma));
        const auto precise = encodeObjDocument(source);
        std::locale::global(previousLocale);
        reimport(source, precise);
        require(planObjGeometry(1000000, 5000000).minimumPayloadBytes < 67108864, "Numeric-only source bounds allocated/rejected");
        rejects([] { planObjGeometry(std::numeric_limits<size_t>::max(), 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planObjGeometry(1, 5000001); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planObjGeometry(1, 0); }, DUST3D_STATUS_NO_GEOMETRY);
        rejects([] { planObjGeometry(0, 1); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planObjGeometry(1, 1, 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planObjGeometry(1, 1, 64); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planObjGeometry(1, 1, 268435457); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        auto invalid = fixture();
        invalid.triangleAndQuads[0].resize(5);
        rejects([&] { encodeObjDocument(invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
        invalid = fixture();
        invalid.triangleAndQuads[0][0] = 5;
        rejects([&] { encodeObjDocument(invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
        invalid = fixture();
        invalid.vertices[0][1] = std::numeric_limits<double>::infinity();
        rejects([&] { encodeObjDocument(invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
        invalid = fixture();
        invalid.vertices[0][1] = 4000001;
        rejects([&] { encodeObjDocument(invalid); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        std::ifstream file(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(bool(file), "Capsule fixture missing");
        const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        std::string warning;
        auto snapshot = readDocument(bytes.data(), bytes.size(), warning);
        std::unique_ptr<OwnedObjDocument> generated;
        std::unique_ptr<FbxGeometrySelection> fbx;
        GenerationOutputs outputs;
        outputs.objDocument = &generated;
        outputs.fbxGeometry = &fbx;
        std::lock_guard<std::mutex> lock(engineMutex());
        const auto preview = generateMesh(std::move(snapshot), warning, nullptr, nullptr, nullptr, outputs);
        require(generated && fbx && !generated->bytes().empty() && preview->vertices.size() == 240, "Original generated polygon OBJ missing");
        const auto* previousObj = generated.get();
        const auto* previousFbx = fbx.get();
        outputs.maximumFbxGeometryBytes = planFbxGeometrySelection(1, 1, 1, false, false).stagingBytes;
        rejects([&] { generateMesh(readDocument(bytes.data(), bytes.size(), warning), warning, nullptr, nullptr, nullptr, outputs); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        require(generated.get() == previousObj && fbx.get() == previousFbx, "Failed companion output partially replaced another export owner");
        if (argc == 3) {
            const auto simple = encodeObjDocument(fixture());
            std::ofstream exported(argv[2], std::ios::binary | std::ios::trunc);
            exported.exceptions(std::ios::badbit | std::ios::failbit);
            exported.write(reinterpret_cast<const char*>(simple.bytes().data()), static_cast<std::streamsize>(simple.bytes().size()));
            exported.close();
        }
        std::cout << "Original double/polygon OBJ contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

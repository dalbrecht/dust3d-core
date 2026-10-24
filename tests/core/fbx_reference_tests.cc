// SPDX-License-Identifier: MIT
#include "FbxDocument.h"
#include <algorithm>
#include <application/third_party/fbx/src/fbxdocument.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
using namespace dust3d::native;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
class TemporaryDirectory {
public:
    explicit TemporaryDirectory(const std::filesystem::path& parent)
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (size_t index = 0; index < 100; ++index) {
            auto candidate = parent / ("fbx-reference-" + std::to_string(stamp) + "-" + std::to_string(index));
            if (std::filesystem::create_directory(candidate)) {
                path = std::move(candidate);
                return;
            }
        }
        throw std::runtime_error("Cannot create isolated FBX reference fixture directory");
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    ~TemporaryDirectory()
    {
        std::error_code cause;
        std::filesystem::remove_all(path, cause);
        if (cause)
            std::cerr << "FBX reference cleanup failed: " << cause.message() << '\n';
    }
    std::filesystem::path path;
};
std::vector<uint8_t> readBytes(const std::filesystem::path& path)
{
    require(std::filesystem::file_size(path) < 65536, "Reference fixture unexpectedly large");
    std::ifstream input(path, std::ios::binary);
    input.exceptions(std::ios::badbit);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}
std::string describe(fbx::FBXDocument& document)
{
    std::ostringstream capture;
    struct Restore {
        std::streambuf* original;
        ~Restore() { std::cout.rdbuf(original); }
    } restore { std::cout.rdbuf(capture.rdbuf()) };
    document.print();
    return capture.str();
}
void verify(const std::filesystem::path& parent)
{
    FbxPropertyBlock properties;
    fbx::FBXNode referenceRoot("Root");
    properties.appendInt16(-32768);
    referenceRoot.addProperty(int16_t(-32768));
    properties.appendBoolean(true);
    referenceRoot.addProperty(true);
    properties.appendInt32(-7);
    referenceRoot.addProperty(int32_t(-7));
    properties.appendFloat(-0.0F);
    referenceRoot.addProperty(-0.0F);
    properties.appendDouble(-2);
    referenceRoot.addProperty(-2.0);
    properties.appendInt64(INT64_MIN);
    referenceRoot.addProperty(int64_t(INT64_MIN));
    const std::string name("Mesh\0\1Geometry", 14);
    properties.appendString(name);
    referenceRoot.addProperty(name);
    const std::vector<uint8_t> raw { 0, 255, 17 };
    properties.appendRaw(raw);
    referenceRoot.addProperty(raw, 'R');
    const std::vector<int32_t> ints { 1, -2 };
    properties.appendInt32Array(ints);
    referenceRoot.addProperty(ints);
    const std::vector<int64_t> longs { -1, 46186158000LL };
    properties.appendInt64Array(longs);
    referenceRoot.addProperty(longs);
    const std::vector<float> floats { 1, -0.0F };
    properties.appendFloatArray(floats);
    referenceRoot.addProperty(floats);
    const std::vector<double> doubles { -2, 0.5 };
    properties.appendDoubleArray(doubles);
    referenceRoot.addProperty(doubles);
    const std::vector<bool> booleans { true, false, true };
    properties.appendBooleanArray(booleans);
    referenceRoot.addProperty(booleans);
    std::vector<FbxNode> children;
    children.emplace_back("Child");
    referenceRoot.addChild(fbx::FBXNode("Child"));
    referenceRoot.addChild(fbx::FBXNode());
    std::vector<FbxNode> roots;
    roots.emplace_back("Root", std::move(properties), std::move(children));
    roots.emplace_back("Sibling");
    const auto output = encodeFbxDocument(roots);
    require(output.bytes().size() < 4096, "Reference output unexpectedly large");
    fbx::FBXDocument reference;
    reference.nodes.push_back(std::move(referenceRoot));
    reference.nodes.emplace_back("Sibling");
    TemporaryDirectory directory(parent);
    const auto referencePath = directory.path / "reference.fbx";
    {
        std::ofstream file(referencePath, std::ios::binary);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        reference.write(file);
        file.flush();
        file.close();
    }
    require(readBytes(referencePath) == output.bytes(), "Native FBX bytes differ from untouched reference writer");
    const auto nativePath = directory.path / "native.fbx";
    {
        std::ofstream file(nativePath, std::ios::binary);
        file.exceptions(std::ios::badbit | std::ios::failbit);
        file.write(reinterpret_cast<const char*>(output.bytes().data()), static_cast<std::streamsize>(output.bytes().size()));
        file.close();
    }
    // Only byte-equal, small, known-valid reference output reaches the inherited reader.
    fbx::FBXDocument decoded;
    // Preserve stream failures instead of letting the inherited reader continue after a failed extraction.
    std::ifstream input;
    input.exceptions(std::ios::badbit | std::ios::failbit);
    input.open(nativePath, std::ios::binary);
    decoded.read(input);
    require(decoded.getVersion() == 7400 && decoded.nodes.size() == 2 && decoded.nodes.front().getName() == "Root", "Reference reader lost native node structure");
    require(describe(decoded) == describe(reference), "Reference reader changed encoded property values/types");
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Pass a test-build parent directory");
        verify(argv[1]);
        std::cout << "FBX reference codec interoperability passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
    } catch (const std::string& cause) {
        std::cerr << cause << '\n';
    } catch (...) {
        std::cerr << "Unexpected reference codec failure\n";
    }
    return 1;
}

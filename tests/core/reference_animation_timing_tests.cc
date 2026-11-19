// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxMeshScene.h"
#include "FishIdleEvaluation.h"
#include "GroundedFishIdleSound.h"
#include <application/third_party/fbx/src/fbxdocument.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& error) {
        require(error.status() == status, "Timing rejection lost its typed cause");
        return;
    }
    throw std::runtime_error("Invalid/over-budget timing request was admitted");
}

RigStructure fishRig()
{
    // Same canonical seven required names as application/resources/rig_fish.xml;
    // synthetic grounded rest positions. This fixture has no mesh generation or spring accessories.
    return { "Fish", "", {
                             { "Root", "", 0, 0, 0.75f, 0, 0.25f, 0.75f },
                             { "Head", "Root", 0, 0, 0.75f, 0, 0, 0.50f },
                             { "BodyFront", "Head", 0, 0, 0.50f, 0, 0, 0.25f },
                             { "BodyMid", "BodyFront", 0, 0, 0.25f, 0, 0, 0 },
                             { "BodyRear", "BodyMid", 0, 0, 0, 0, 0, -0.25f },
                             { "TailStart", "BodyRear", 0, 0, -0.25f, 0, 0, -0.50f },
                             { "TailEnd", "TailStart", 0, 0, -0.50f, 0, 0, -0.75f },
                         } };
}

AnimationParams timing(const std::string& seconds, const std::string& count = "1")
{
    return { { { "durationSeconds", seconds }, { "frameCount", count } } };
}

std::map<std::string, Matrix4x4> inverseFor(const RigStructure& rig)
{
    std::map<std::string, Matrix4x4> inverse;
    require(RigGenerator().computeBoneInverseBindMatrices(rig, inverse), "Fish inverse binds failed");
    return inverse;
}

RigAnimationClip reference(const RigStructure& rig, const std::map<std::string, Matrix4x4>& inverse,
    const std::string& type, const AnimationParams& authored)
{
    RigAnimationClip result;
    require(AnimationGenerator::generate(rig, inverse, result, type, authored), "Shared reference rejected " + type);
    return result;
}

void checkPoseTiming()
{
    const auto rig = fishRig();
    const auto inverse = inverseFor(rig);
    for (const std::string type : { "FishIdle", "FishAttack" }) {
        for (const std::string seconds : { "1", "0.001", "0.01", "59.99", "60", "600" }) {
            for (const std::string count : { "1", "2", "4" }) {
                const auto authored = timing(seconds, count);
                const auto original = authored.values;
                const auto generated = reference(rig, inverse, type, authored);
                const auto expected = OwnedEvaluatedSkeletonClip::copy(generated, rig, 1024 * 1024);
                const auto actual = [&] {
                    try {
                        return evaluateFishIdle(rig, inverse, authored, 1024 * 1024, nullptr, {}, type);
                    } catch (const DocumentError& error) {
                        throw std::runtime_error(type + " seconds=" + seconds + " frameCount=" + count + ": " + error.what());
                    }
                }();
                require(authored.values == original, "Evaluation changed authored timing text");
                const size_t evaluatedCount = count == "4" ? 4 : 2;
                require(actual.sampleTimes().size() == evaluatedCount + (actual.loops() ? 1 : 0), "Raw frameCount was used instead of normalized count");
                require(actual.animationType() == type && actual.loops() == generated.loop, "Selected timing identity changed");
                require(actual.durationSeconds() == static_cast<float>(std::stod(seconds)), "Reference Float duration changed");
                require(actual.sampleTimes() == expected.sampleTimes(), "Reference sampling times changed");
                require(actual.modelFromJoint() == expected.modelFromJoint(), "Reference timing matrices changed");
                for (const auto& matrix : actual.modelFromJoint()) {
                    for (const auto value : matrix)
                        require(std::isfinite(value), "Timing returned nonfinite matrices");
                }
                require(actual.sampleTimes().back() == actual.durationSeconds(), "Terminal pose was lost");
                rejects([&] { (void)evaluateFishIdle(rig, inverse, authored, actual.logicalPayloadBytes() - 1, nullptr, {}, type); }, DUST3D_STATUS_LIMIT_EXCEEDED);
            }
        }
    }
    for (const std::string seconds : { "0", "-1", "0.000999", "600.000001", "nan", "inf", "1e999" })
        rejects([&] { (void)evaluateFishIdle(rig, inverse, timing(seconds), 1024 * 1024); }, DUST3D_STATUS_INVALID_DOCUMENT);
    for (const std::string count : { "0", "-1", "1.5", "5000", "nan", "1e999" })
        rejects([&] { (void)evaluateFishIdle(rig, inverse, timing("1", count), 1024 * 1024); }, DUST3D_STATUS_INVALID_DOCUMENT);
}

std::string description(fbx::FBXNode node)
{
    std::ostringstream output;
    struct Restore {
        std::streambuf* buffer;
        ~Restore() { std::cout.rdbuf(buffer); }
    } restore { std::cout.rdbuf(output.rdbuf()) };
    node.print();
    return output.str();
}

void inspect(fbx::FBXNode node, size_t& stacks, size_t& stops, size_t& keyArrays)
{
    const auto name = node.getName();
    const auto text = description(node);
    if (name == "AnimationStack") {
        ++stacks;
        require(text.find("Controlled 600") != std::string::npos, "Selected stack name changed");
    }
    if (name == "P" && text.find("LocalStop") != std::string::npos) {
        ++stops;
        require(text.find("27711694800000") != std::string::npos, "Independent reader lost600second LocalStop");
    }
    if (name == "KeyTime") {
        ++keyArrays;
        require(text.find("27711694800000") != std::string::npos, "Independent reader lost terminal key time");
        require(text.find("[0,") != std::string::npos, "Independent reader lost initial key time");
    }
    for (auto child : node.getChildren())
        inspect(std::move(child), stacks, stops, keyArrays);
}

void checkFbxTiming(const std::filesystem::path& parent)
{
    const auto rig = fishRig();
    const auto inverse = inverseFor(rig);
    Object mesh;
    mesh.vertices = { { 0, 0, 0 }, { 0.1, 0, 0 }, { 0, 0.1, 0 } };
    mesh.triangles = { { 0, 1, 2 } };
    const std::vector<GlbSkinWeights> weights(3, { { 3, 0, 0, 0 }, { 1, 0, 0, 0 } });
    for (const std::string type : { "FishIdle", "FishAttack" }) {
        RigAnimationClip clip;
        const auto evaluated = evaluateFishIdle(rig, inverse, timing("600", "4"), 1024 * 1024, nullptr, [&](const RigAnimationClip& generated, size_t) {
                clip = generated;
                return size_t { 0 }; }, type);
        require(evaluated.durationSeconds() == 600 && clip.animationType == type, "Native handoff lost selected600second clip");
        clip.name = "Controlled 600 " + type;
        const auto encoded = encodeFbxRestMeshScene(mesh, nullptr, rig, weights, { 1024 * 1024, 4096 }, { clip });
        require(encoded.payloadBytes() <= 1024 * 1024, "Controlled export exceeded its byte budget");
        rejects([&] { (void)encodeFbxRestMeshScene(mesh, nullptr, rig, weights, { 64, 4096 }, { clip }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([&] { (void)encodeFbxRestMeshScene(mesh, nullptr, rig, weights, { 1024 * 1024, 1 }, { clip }); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        const auto path = parent / ("timing-" + type + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".fbx");
        struct Remove {
            std::filesystem::path path;
            ~Remove()
            {
                std::error_code ignored;
                std::filesystem::remove(path, ignored);
            }
        } remove { path };
        {
            std::ofstream stream(path, std::ios::binary);
            stream.exceptions(std::ios::badbit | std::ios::failbit);
            stream.write(reinterpret_cast<const char*>(encoded.bytes().data()), encoded.bytes().size());
        }
        fbx::FBXDocument decoded;
        std::ifstream stream(path, std::ios::binary);
        decoded.read(stream);
        size_t stacks = 0, stops = 0, keys = 0;
        for (auto node : decoded.nodes)
            inspect(std::move(node), stacks, stops, keys);
        require(stacks == 1 && stops == 1 && keys > 0, "Controlled export lost stack/timing/animated curves");
    }
}

void checkPcmLimits()
{
    const auto rig = fishRig();
    const auto inverse = inverseFor(rig);
    const auto longClip = reference(rig, inverse, "FishIdle", timing("31", "4"));
    rejects([&] { (void)renderFishIdleSound(longClip, {}, SurfaceMaterial::Water, 1024 * 1024, nullptr); }, DUST3D_STATUS_INVALID_DOCUMENT);
    rejects([&] { (void)renderFishIdleSoundSurfaces(longClip, {}, 1024 * 1024, nullptr); }, DUST3D_STATUS_INVALID_DOCUMENT);
    const auto manyFrames = reference(rig, inverse, "FishIdle", timing("1", "129"));
    rejects([&] { (void)renderFishIdleSound(manyFrames, {}, SurfaceMaterial::Water, 1024 * 1024, nullptr); }, DUST3D_STATUS_INVALID_DOCUMENT);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 3, "Expected mode and owned build directory");
        const std::string mode = argv[1];
        if (mode == "pose")
            checkPoseTiming();
        else if (mode == "fbx")
            checkFbxTiming(argv[2]);
        else if (mode == "pcm")
            checkPcmLimits();
        else
            throw std::runtime_error("Unknown timing contract mode");
        std::cout << "Reference timing " << mode << " contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}

// SPDX-License-Identifier: MIT
// Unregistered source contract. Compile only under the current runtime quarantine.
#include "FishIdleEvaluation.h"
#include "GroundedFishIdleFbx.h"
#include <cassert>
#include <string_view>

static_assert(dust3d::native::supportsFishLoop("FishForward"));
static_assert(!dust3d::native::supportsFishLoop("fishForward"));
static_assert(dust3d::native::supportsFishClip("FishAttack"));
static_assert(dust3d::native::supportsFishClip("FishHurt"));
static_assert(dust3d::native::supportsFishClip("FishDie"));
static_assert(!dust3d::native::supportsFishLoop("FishAttack"));
static_assert(!dust3d::native::supportsFishClip("FishDash"));

void checkGroundedFishIdleFbx(const dust3d::native::GeneratedRigInput& input, const dust3d::RigStructure& rig)
{
    using namespace dust3d::native;
    const std::vector<FishIdleFbxClipInput> clips {
        { "Authored Drift", { { { "durationSeconds", "1" }, { "frameCount", "32" }, { "driftFactor", "3" } } } },
        { "Authored Swim", { { { "durationSeconds", "1.25" }, { "frameCount", "24" }, { "swimSpeedFactor", "1" } } }, "FishSwim" },
        { "Legacy Forward", { { { "durationSeconds", "1" }, { "frameCount", "30" } } }, "FishForward" },
        { "Authored Attack", { { { "durationSeconds", "0.9" }, { "frameCount", "27" } } }, "FishAttack" },
        { "Authored Hurt", {}, "FishHurt" },
        { "Authored Die", {}, "FishDie" },
    };
    const auto output = encodeGroundedFishIdleFbx(input, rig, clips, nullptr, {});
    const std::string_view bytes(reinterpret_cast<const char*>(output.bytes().data()), output.bytes().size());
    const auto drift = bytes.find("Authored Drift");
    const auto rest = bytes.find("Authored Swim");
    const auto legacy = bytes.find("Legacy Forward");
    assert(drift != std::string_view::npos && rest != std::string_view::npos && legacy != std::string_view::npos);
    const auto attack = bytes.find("Authored Attack");
    const auto hurt = bytes.find("Authored Hurt");
    const auto die = bytes.find("Authored Die");
    assert(attack != std::string_view::npos && hurt != std::string_view::npos && die != std::string_view::npos);
    assert(drift < rest && rest < legacy && legacy < attack && attack < hurt && hurt < die);
    assert(bytes.find("BindPose") != std::string_view::npos);
    assert(bytes.find("AnimationStack") != std::string_view::npos);
}

void checkTexturedGroundedFishIdleFbx(const dust3d::native::GeneratedRigInput& input,
    const dust3d::RigStructure& rig, const dust3d::Snapshot& authored, Dust3DBakeOptions options)
{
    using namespace dust3d::native;
    const FishIdleFbxBakeInput bake { authored, options };
    const std::vector<FishIdleFbxClipInput> clips {
        { "Textured Drift", { { { "durationSeconds", "1" }, { "frameCount", "32" } } } },
    };
    const auto output = encodeGroundedFishIdleFbx(input, rig, clips, nullptr, {}, {}, &bake);
    const std::string_view bytes(reinterpret_cast<const char*>(output.bytes().data()), output.bytes().size());
    assert(bytes.find("LayerElementUV") != std::string_view::npos);
    assert(bytes.find("AnimationStack") != std::string_view::npos);
    assert(bytes.find("Material") != std::string_view::npos);
}

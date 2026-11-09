// SPDX-License-Identifier: MIT
// Unregistered source contract. Compile only under the current runtime quarantine.
#include "GroundedFishIdleFbx.h"
#include <cassert>
#include <string_view>

void checkGroundedFishIdleFbx(const dust3d::native::GeneratedRigInput& input, const dust3d::RigStructure& rig)
{
    using namespace dust3d::native;
    const std::vector<FishIdleFbxClipInput> clips {
        { "Authored Drift", { { { "durationSeconds", "1" }, { "frameCount", "32" }, { "driftFactor", "3" } } } },
        { "Authored Rest", { { { "durationSeconds", "1.25" }, { "frameCount", "24" } } } },
    };
    const auto output = encodeGroundedFishIdleFbx(input, rig, clips, nullptr, {});
    const std::string_view bytes(reinterpret_cast<const char*>(output.bytes().data()), output.bytes().size());
    const auto drift = bytes.find("Authored Drift");
    const auto rest = bytes.find("Authored Rest");
    assert(drift != std::string_view::npos && rest != std::string_view::npos && drift < rest);
    assert(bytes.find("BindPose") != std::string_view::npos);
    assert(bytes.find("AnimationStack") != std::string_view::npos);
}

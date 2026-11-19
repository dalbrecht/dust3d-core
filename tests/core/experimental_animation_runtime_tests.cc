// SPDX-License-Identifier: MIT
#include "ExperimentalFishIdle.h"
#include "fish_idle_pcm_fixture.h"
#include <cassert>
#include <cstdlib>

#include <iostream>
#include <iterator>
#include <memory>
#include <string_view>

void checkFishIdlePCMOwner();
void checkSelectedFishIdleFbxOwner();

static void checkAuthoredDecimalCompatibility()
{
    auto options = dust3d_experimental_fish_idle_default_options();
    const auto check = [&](std::string_view value, bool accepted) {
        const Dust3DExperimentalFishIdleParameter parameters[] {
            { "durationSeconds", 15, "1", 1 },
            { "frameCount", 10, "4", 1 },
            { "driftFactor", 11, value.data(), value.size() },
        };
        Dust3DError error {};
        std::unique_ptr<Dust3DExperimentalFishIdle, decltype(&dust3d_experimental_fish_idle_destroy)> owner(
            dust3d_experimental_fish_idle_load(fishArchive, std::size(fishArchive),
                fishBones, std::size(fishBones), parameters, std::size(parameters), &options, &error),
            dust3d_experimental_fish_idle_destroy);
        if (bool(owner) != accepted) {
            std::cerr << "Unexpected authored decimal admission: " << value << "\n";
            std::abort();
        }
        assert(error.status == (accepted ? DUST3D_STATUS_OK : DUST3D_STATUS_INVALID_DOCUMENT));
    };
    for (auto value : { "0", "+0.2", "0,2", "2e-1" })
        check(value, true);
    for (auto value : { "", " 1", "1 ", "+-1", "-+1", "nan", "inf", "1e999", "1x" })
        check(value, false);
    check(std::string_view("1\0", 2), false);
}

int main()
{
    checkAuthoredDecimalCompatibility();
    checkFishIdlePCMOwner();
    checkSelectedFishIdleFbxOwner();
    std::cout << "Private animation pose, PCM, and FBX ownership passed\n";
}

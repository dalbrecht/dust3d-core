// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/biped/clip_metadata.h>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    try {
        const auto& catalog = dust3d::animation::clipCatalog();
        require(catalog.size() == 180, "inherited catalog membership changed");
        std::map<std::string, size_t> visibleByRig;
        size_t aliases = 0;
        for (const auto& entry : catalog) {
            const auto& info = entry.second;
            require(!entry.first.empty() && !info.generator.empty(), "missing type/generator identity");
            require(std::isfinite(info.duration) && info.duration > 0 && info.samples > 0, "invalid timing metadata");
            require(catalog.count(info.generator) == 1, "generator identity is absent");
            for (const auto& parameter : info.parameters)
                require(!parameter.first.empty() && std::isfinite(parameter.second), "invalid default override");
            if (info.alias)
                ++aliases;
            else
                ++visibleByRig[info.rig];
        }
        const std::map<std::string, size_t> expected = {
            { "Biped", 85 }, { "Bird", 24 }, { "Fish", 5 }, { "Insect", 15 },
            { "Quadruped", 22 }, { "Snake", 5 }, { "Spider", 20 }
        };
        require(visibleByRig == expected && aliases == 4, "rig/alias inventory changed");
        const auto& sprint = catalog.at("BipedSprint");
        require(sprint.duration == 0.65 && sprint.samples == 32 && sprint.loop, "additional biped timing changed");
        const auto& idle = catalog.at("BipedIdle");
        require(idle.duration == 4 && idle.samples == 90 && idle.parameters.at("breathingAmplitudeFactor") == 0.65,
            "base clip overrides changed");
        const auto& backward = catalog.at("QuadrupedRunBackward");
        require(backward.generator == "QuadrupedRun" && backward.parameters.at("travelAngleDegrees") == 180
                && backward.parameters.at("stepLengthFactor") == 0.55,
            "directional gait identity or overrides changed");
        const auto& diagonal = catalog.at("BipedWalkForwardLeft");
        require(diagonal.parameters.at("travelAngleDegrees") == 45
                && diagonal.parameters.at("footRollFactor") == 0 && diagonal.parameters.at("leanForwardFactor") == 0,
            "biped direction overrides changed");
        const auto& legacy = catalog.at("BirdForward");
        require(legacy.alias && legacy.generator == "BirdFly" && legacy.duration == 1 && legacy.samples == 30,
            "legacy alias timing changed");
        const auto& metadata = dust3d::biped::additionalClipMetadata();
        require(metadata.size() == 48, "additional biped inventory changed");
        std::set<std::string> names;
        for (const auto& clip : metadata) {
            require(names.insert(clip.type).second && clip.entryPose && clip.exitPose, "duplicate or missing biped metadata");
            const auto& info = catalog.at(clip.type);
            require(info.duration == clip.duration && info.samples == clip.samples && info.loop == clip.loop,
                "additional metadata diverged from catalog");
        }
        require(dust3d::animation::standardBones("Biped").count("Root") == 1, "known rig bones changed");
        require(dust3d::animation::standardBones("Fish").count("Jaw") == 1, "optional fish jaw changed");
        require(dust3d::animation::standardBones("future").empty(), "unknown rig must have no inferred bones");
        require(&catalog == &dust3d::animation::clipCatalog(), "catalog storage must remain stable");
        std::cout << "Generator-free animation metadata contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

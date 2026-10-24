// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/animation_defaults.h>
#include <iostream>
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
        for (const auto& entry : catalog) {
            const auto timing = dust3d::animation::defaultTiming(entry.first);
            require(timing.first == entry.second.duration && timing.second == entry.second.samples,
                "Inherited default timing changed");
            auto parameters = dust3d::animation::defaultParameters(entry.first);
            require(parameters.getValue("durationSeconds", -1) == entry.second.duration
                    && parameters.getValue("frameCount", -1) == entry.second.samples,
                "Inherited common defaults changed");
            require(parameters.values.size() == entry.second.parameters.size() + 2, "Default membership changed");
            for (const auto& value : entry.second.parameters) {
                require(std::isfinite(parameters.getValue(value.first, -1))
                        && parameters.getValue(value.first, -1) == value.second,
                    "Inherited numeric/string override changed");
            }
            parameters.setValue("durationSeconds", 1.25);
            require(dust3d::animation::defaultParameters(entry.first).getValue("durationSeconds", -1) == entry.second.duration,
                "Returned default values must be independently owned");
        }
        const auto unknown = dust3d::animation::defaultTiming("Future.Type");
        require(unknown.first == 3 && unknown.second == 90, "Inherited unknown timing fallback changed");
        require(dust3d::animation::defaultParameters("Future.Type").values.empty(), "Unknown type must not invent parameters");
        const auto diagonal = dust3d::animation::defaultParameters("BipedWalkForwardLeft");
        require(diagonal.getValue("travelAngleDegrees", 0) == 45 && diagonal.getValue("stepLengthFactor", 0) == 0.7,
            "Directional defaults changed");
        require(diagonal.getValue("missing", 7) == 7 && !diagonal.getBool("missing", false), "Missing parameter fallback");
        auto flags = diagonal;
        flags.setBool("flag", true);
        require(flags.getBool("flag", false) && flags.values.at("flag") == "true", "Boolean text semantics");
        flags.setBool("flag", false);
        require(!flags.getBool("flag", true) && flags.values.at("flag") == "false", "False boolean text semantics");
        require(dust3d::animation::defaultParameters("BirdForward").values
                == dust3d::animation::defaultParameters("BirdFly").values,
            "Legacy alias defaults changed");
        std::cout << "Generator-free animation default contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

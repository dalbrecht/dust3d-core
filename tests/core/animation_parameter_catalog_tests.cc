// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/parameter_catalog.h>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
const dust3d::animation::ParameterDefinition& find(const std::string& type, const std::string& name)
{
    for (const auto& definition : dust3d::animation::parameterDefinitions(type))
        if (definition.parameterName == name)
            return definition;
    throw std::runtime_error("Expected reference parameter missing");
}
template <typename Action>
void rejects(Action action, dust3d::animation::ParameterConversionErrorCode expected)
{
    try {
        action();
        throw std::runtime_error("Invalid conversion was accepted");
    } catch (const dust3d::animation::ParameterConversionError& error) {
        require(error.code() == expected, "Original typed conversion cause changed");
    }
}
}
int main()
{
    using namespace dust3d::animation;
    try {
        size_t definitions = 0;
        size_t outOfRangeDefaults = 0;
        for (const auto& clip : clipCatalog()) {
            std::set<std::string> names;
            for (const auto& definition : parameterDefinitions(clip.first)) {
                ++definitions;
                require(!definition.parameterName.empty() && !definition.displayName.empty()
                        && names.insert(definition.parameterName).second,
                    "Missing or duplicate parameter identity/label");
                require(std::isfinite(definition.defaultParameterValue) && std::isfinite(definition.sliderDivisor)
                        && definition.sliderDivisor > 0 && std::isfinite(definition.parameterMultiplier) && definition.parameterMultiplier > 0 && definition.minimumSliderValue <= definition.maximumSliderValue,
                    "Invalid descriptor scalar metadata");
                require(std::isfinite(parameterValue(definition, definition.minimumSliderValue))
                        && std::isfinite(parameterValue(definition, definition.maximumSliderValue)),
                    "Finite endpoint conversion");
                if (definition.defaultSliderValue < definition.minimumSliderValue || definition.defaultSliderValue > definition.maximumSliderValue)
                    ++outOfRangeDefaults;
                else
                    require(sliderValue(definition, definition.defaultParameterValue) == definition.defaultSliderValue,
                        "Inherited default truncation changed");
            }
        }
        require(definitions == 1592 && outOfRangeDefaults == 0 && parameterDefinitions("Future.Type").empty(), "Reference schema inventory/default range changed");
        const auto& wing = find("BirdGlide", "wingSpreadAngle");
        require(wing.sliderDivisor == 667 && wing.minimumSliderValue == 0 && wing.maximumSliderValue == 300,
            "Nondecimal reference scaling changed");
        require(wing.defaultParameterValue == 0.15 && wing.defaultSliderValue == 100
                && sliderValue(wing, 0.15) == 100 && parameterValue(wing, 100) == 100.0 / 667.0,
            "Reference scaling/truncation behavior changed");
        const auto& hit = find("FishDie", "hitFrequency");
        require(hit.sliderDivisor == 100 && hit.parameterMultiplier == 8
                && parameterValue(hit, 137) == 137.0 / 100.0 * 8.0
                && sliderValue(hit, 8.0) == 100,
            "Custom hit-frequency operation order changed");
        const auto& decay = find("FishDie", "spinDecay");
        require(decay.sliderDivisor == 100 && decay.parameterMultiplier == 4
                && parameterValue(decay, 137) == 137.0 / 100.0 * 4.0,
            "Custom decay operation order changed");
        const auto& idle = find("BipedIdle", "breathingAmplitudeFactor");
        require(idle.defaultParameterValue == 0.65 && idle.defaultSliderValue == 65, "Preset override not applied");
        const auto& direction = find("QuadrupedRunBackward", "travelAngleDegrees");
        require(direction.defaultParameterValue == 180 && direction.minimumSliderValue == -180
                && direction.maximumSliderValue == 180 && sliderValue(direction, -45.9) == -45,
            "Directional range or truncation changed");
        const auto& speed = find("InsectWalk", "gaitSpeedFactor");
        require(speed.sliderDivisor == 1 && speed.minimumSliderValue == 1 && speed.maximumSliderValue == 8,
            "Direct integer control metadata changed");
        rejects([&] { sliderValue(wing, std::numeric_limits<double>::quiet_NaN()); }, ParameterConversionErrorCode::nonFiniteValue);
        rejects([&] { sliderValue(wing, std::numeric_limits<double>::infinity()); }, ParameterConversionErrorCode::nonFiniteValue);
        rejects([&] { sliderValue(wing, 1e20); }, ParameterConversionErrorCode::unrepresentableValue);
        rejects([&] { sliderValue(wing, -1); }, ParameterConversionErrorCode::outsideRange);
        rejects([&] { parameterValue(speed, 0); }, ParameterConversionErrorCode::outsideRange);
        auto invalid = wing;
        invalid.sliderDivisor = 0;
        rejects([&] { sliderValue(invalid, 1); }, ParameterConversionErrorCode::invalidDefinition);
        invalid = wing;
        invalid.minimumSliderValue = 301;
        rejects([&] { parameterValue(invalid, 100); }, ParameterConversionErrorCode::invalidDefinition);
        invalid = wing;
        invalid.parameterMultiplier = 0;
        rejects([&] { sliderValue(invalid, 1); }, ParameterConversionErrorCode::invalidDefinition);
        auto wide = wing;
        wide.sliderDivisor = 1;
        wide.parameterMultiplier = 1;
        wide.defaultParameterValue = 0;
        wide.defaultSliderValue = 0;
        wide.minimumSliderValue = std::numeric_limits<int>::lowest();
        wide.maximumSliderValue = std::numeric_limits<int>::max();
        const double maximum = static_cast<double>(wide.maximumSliderValue);
        const double minimum = static_cast<double>(wide.minimumSliderValue);
        require(sliderValue(wide, maximum) == wide.maximumSliderValue
                && sliderValue(wide, minimum) == wide.minimumSliderValue,
            "Exact integer endpoints rejected");
        require(sliderValue(wide, maximum + 0.75) == wide.maximumSliderValue
                && sliderValue(wide, minimum - 0.75) == wide.minimumSliderValue,
            "Representable truncated fractional endpoints rejected");
        rejects([&] { sliderValue(wide, maximum + 1.0); }, ParameterConversionErrorCode::unrepresentableValue);
        rejects([&] { sliderValue(wide, minimum - 1.0); }, ParameterConversionErrorCode::unrepresentableValue);
        std::cout << "Checked animation parameter catalog passed: " << definitions
                  << " descriptors, " << outOfRangeDefaults << " raw defaults outside widget range\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

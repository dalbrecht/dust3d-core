#ifndef DUST3D_APPLICATION_ANIMATION_PARAMETER_TABLE_H_
#define DUST3D_APPLICATION_ANIMATION_PARAMETER_TABLE_H_

#include <dust3d/animation/parameter_catalog.h>
#include <functional>
#include <string>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

struct AnimationParameterDef {
    std::string paramName;
    std::string displayName;
    int defaultSliderValue;
    int sliderMin;
    int sliderMax;
    double defaultParamValue;
    std::function<double(int)> toParam;
    std::function<int(double)> fromParam;
};

inline AnimationParameterDef makeDiv100Param(const std::string& paramName, const std::string& displayName,
    int defaultSlider, int sliderMin, int sliderMax, double defaultParam)
{
    return { paramName, displayName, defaultSlider, sliderMin, sliderMax, defaultParam,
        [](int v) { return v / 100.0; },
        [](double v) { return static_cast<int>(v * 100); } };
}

inline AnimationParameterDef makeLinearParam(const std::string& paramName, const std::string& displayName,
    int defaultSlider, int sliderMin, int sliderMax, double defaultParam,
    double sliderToParamFactor)
{
    return { paramName, displayName, defaultSlider, sliderMin, sliderMax, defaultParam,
        [sliderToParamFactor](int v) { return v / sliderToParamFactor; },
        [sliderToParamFactor](double v) { return static_cast<int>(v * sliderToParamFactor); } };
}

inline AnimationParameterDef makeDirectParam(const std::string& paramName, const std::string& displayName,
    int defaultSlider, int sliderMin, int sliderMax, double defaultParam)
{
    return { paramName, displayName, defaultSlider, sliderMin, sliderMax, defaultParam,
        [](int v) { return static_cast<double>(v); },
        [](double v) { return static_cast<int>(v); } };
}

inline const std::vector<AnimationParameterDef>& getAnimationParameterDefs(const std::string& animationType)
{
    static const std::vector<AnimationParameterDef> empty;
    const auto& definitions = dust3d::animation::parameterDefinitions(animationType);
    if (definitions.empty())
        return empty;
    // Only declared nonempty schemas are cached; unknown document types do not grow this cache.
    static std::mutex mutex;
    static std::map<std::string, std::vector<AnimationParameterDef>> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto found = cache.find(animationType);
    if (found != cache.end())
        return found->second;
    std::vector<AnimationParameterDef> converted;
    converted.reserve(definitions.size());
    for (const auto& definition : definitions) {
        if (definition.parameterMultiplier == 1.0) {
            converted.push_back(makeLinearParam(definition.parameterName, definition.displayName,
                definition.defaultSliderValue, definition.minimumSliderValue, definition.maximumSliderValue,
                definition.defaultParameterValue, definition.sliderDivisor));
        } else {
            const double divisor = definition.sliderDivisor;
            const double multiplier = definition.parameterMultiplier;
            // Retain the two legacy reference callback expressions; native callers use checked conversion instead.
            converted.push_back({ definition.parameterName, definition.displayName,
                definition.defaultSliderValue, definition.minimumSliderValue, definition.maximumSliderValue,
                definition.defaultParameterValue,
                [divisor, multiplier](int v) { return v / divisor * multiplier; },
                [divisor, multiplier](double v) { return static_cast<int>(v / multiplier * divisor); } });
        }
    }
    return cache.emplace(animationType, std::move(converted)).first->second;
}

#endif

// SPDX-License-Identifier: MIT
#ifndef DUST3D_ANIMATION_PARAMETER_CATALOG_H_
#define DUST3D_ANIMATION_PARAMETER_CATALOG_H_
#include <stdexcept>
#include <string>
#include <vector>
namespace dust3d {
namespace animation {
    /// Ordered reference control metadata. Both nominal default and raw truncated default index are retained.
    struct ParameterDefinition {
        std::string parameterName;
        std::string displayName;
        int defaultSliderValue;
        int minimumSliderValue;
        int maximumSliderValue;
        double defaultParameterValue;
        double sliderDivisor;
        double parameterMultiplier = 1.0;
    };
    enum class ParameterConversionErrorCode { invalidDefinition,
        nonFiniteValue,
        unrepresentableValue,
        outsideRange };
    /// Typed cause for checked metadata conversion; no native rig/animation evaluation occurs.
    class ParameterConversionError final : public std::runtime_error {
    public:
        ParameterConversionError(ParameterConversionErrorCode code, const char* message)
            : std::runtime_error(message)
            , m_code(code)
        {
        }
        ParameterConversionErrorCode code() const noexcept { return m_code; }

    private:
        ParameterConversionErrorCode m_code;
    };
    /// Common reference widget limits; display precision is not stored-value normalization.
    struct CommonTimingDefinition {
        double minimumDurationSeconds;
        double maximumDurationSeconds;
        int durationDecimalPlaces;
        double durationStepSeconds;
        int minimumFrameCount;
        int maximumFrameCount;
        int frameCountStep;
    };
    /// Validated scalar timing only; constructing this value does not allocate frames or evaluate animation.
    struct CheckedTiming {
        double durationSeconds;
        int frameCount;
    };
    /** Return common duration/frame-count widget metadata shared by all clip types.
     * @return Process-static limits copied from the reference widget; clip defaults remain in defaultTiming.
     * No input, allocation or runtime evaluation. Example: `commonTimingDefinition().maximumFrameCount`.
     */
    const CommonTimingDefinition& commonTimingDefinition() noexcept;
    /** Validate an explicit timing edit without rounding/clamping or invoking a sampler.
     * @param durationSeconds Finite duration inside the reference widget's inclusive limits.
     * @param frameCount Finite integral count inside its inclusive limits, checked before integer conversion.
     * @return Unchanged duration and exactly represented integer frame count.
     * @throws ParameterConversionError for nonfinite, out-of-range or fractional input.
     * This validates edits, not arbitrary stored documents; unknown/raw values must survive display/save.
     * Example: `auto timing = checkedTiming(3.0, 90.0)`.
     */
    CheckedTiming checkedTiming(double durationSeconds, double frameCount);
    /** Return original ordered control definitions, including inherited aliases/variants and preset overrides.
     * @param type Exact retained type identity; undeclared types share an empty result.
     * @return Immutable process-static definitions. No generator, Qt, or callback object is required.
     * Construction/typed metadata errors propagate to C++ callers; this is not a C ABI.
     * Example: `const auto& controls = dust3d::animation::parameterDefinitions("BirdGlide");`
     */
    const std::vector<ParameterDefinition>& parameterDefinitions(const std::string& type);
    /** Convert an in-range integer slider index using inherited division-then-multiplication order.
     * @param definition A valid descriptor with positive finite coefficients and representable endpoint values.
     * @param slider Index inside the inclusive declared range.
     * @return Finite stored numerical value; throws ParameterConversionError before invalid conversion.
     * Example: `parameterValue(definition, definition.defaultSliderValue)`.
     */
    double parameterValue(const ParameterDefinition& definition, int slider);
    /** Convert finite stored value with inherited truncation toward zero, then reject indices outside range.
     * @param definition Valid descriptor; raw defaults can be outside widget range and are not silently clamped.
     * @param value Numerical value to convert, not arbitrary stored text.
     * @return In-range index; typed definition/nonfinite/representation checks precede the cast.
     * Widget bounds are checked on the representable truncated index.
     * Quantization is not an exact numerical round trip. Example: `sliderValue(definition, 0.15)`.
     */
    int sliderValue(const ParameterDefinition& definition, double value);
}
}
#endif

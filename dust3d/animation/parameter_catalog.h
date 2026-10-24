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
     * @return In-range index; throws typed nonfinite/representation/range/definition cause before integer cast.
     * Quantization is not an exact numerical round trip. Example: `sliderValue(definition, 0.15)`.
     */
    int sliderValue(const ParameterDefinition& definition, double value);
}
}
#endif

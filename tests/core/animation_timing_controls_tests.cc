// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/parameter_catalog.h>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Action>
void rejects(Action action, dust3d::animation::ParameterConversionErrorCode expected)
{
    try {
        action();
        throw std::runtime_error("Invalid timing accepted");
    } catch (const dust3d::animation::ParameterConversionError& cause) {
        require(cause.code() == expected, "Timing rejection lost its typed cause");
    }
}
}
int main()
{
    using namespace dust3d::animation;
    try {
        const auto& definition = commonTimingDefinition();
        require(definition.minimumDurationSeconds == 0.01 && definition.maximumDurationSeconds == 59.99
                && definition.durationDecimalPlaces == 2 && definition.durationStepSeconds == 0.1
                && definition.minimumFrameCount == 1 && definition.maximumFrameCount == 4999
                && definition.frameCountStep == 5,
            "Reference timing widget metadata changed");
        for (const double seconds : { 0.01, 3.0, 59.99 }) {
            for (const double frames : { 1.0, 90.0, 4999.0 }) {
                const auto timing = checkedTiming(seconds, frames);
                require(timing.durationSeconds == seconds && timing.frameCount == frames,
                    "Checked timing silently normalized its input");
            }
        }
        require(checkedTiming(1.125, 2).durationSeconds == 1.125,
            "Display precision must not round stored duration");
        rejects([] { checkedTiming(0.0, 1); }, ParameterConversionErrorCode::outsideRange);
        rejects([] { checkedTiming(60.0, 1); }, ParameterConversionErrorCode::outsideRange);
        rejects([] { checkedTiming(1.0, 0); }, ParameterConversionErrorCode::outsideRange);
        rejects([] { checkedTiming(1.0, 4999.5); }, ParameterConversionErrorCode::outsideRange);
        rejects([] { checkedTiming(1.0, 1.5); }, ParameterConversionErrorCode::unrepresentableValue);
        rejects([] { checkedTiming(std::numeric_limits<double>::quiet_NaN(), 1); }, ParameterConversionErrorCode::nonFiniteValue);
        rejects([] { checkedTiming(1.0, std::numeric_limits<double>::infinity()); }, ParameterConversionErrorCode::nonFiniteValue);
        std::cout << "Common animation timing metadata contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

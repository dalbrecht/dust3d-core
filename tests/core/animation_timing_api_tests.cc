// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <iostream>
#include <limits>
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
        Dust3DAnimationTimingDefinition definition {};
        require(dust3d_animation_timing_definition(&definition) == 1
                && definition.minimum_duration_seconds == 0.01 && definition.maximum_duration_seconds == 59.99
                && definition.duration_decimal_places == 2 && definition.duration_step_seconds == 0.1
                && definition.minimum_frame_count == 1 && definition.maximum_frame_count == 4999
                && definition.frame_count_step == 5,
            "Timing metadata C copy changed");
        require(dust3d_animation_timing_definition(nullptr) == 0, "Null metadata destination accepted");
        Dust3DAnimationTiming output { 123, 123 };
        Dust3DError error {};
        require(!dust3d_animation_timing_check(1.0, 1.5, &output, &error)
                && output.duration_seconds == 0 && output.frame_count == 0
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT
                && std::string(error.message).find("integral") != std::string::npos,
            "Fractional timing did not preserve cause/clear output");
        require(!dust3d_animation_timing_check(std::numeric_limits<double>::quiet_NaN(), 1, &output, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT
                && std::string(error.message).find("finite") != std::string::npos,
            "Nonfinite timing rejection changed");
        require(!dust3d_animation_timing_check(60, 1, &output, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT
                && std::string(error.message).find("range") != std::string::npos,
            "Timing range rejection changed");
        require(!dust3d_animation_timing_check(1, 1, nullptr, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Null timing destination accepted");
        require(dust3d_animation_timing_check(1.125, 2, &output, &error)
                && output.duration_seconds == 1.125 && output.frame_count == 2
                && error.status == DUST3D_STATUS_OK && error.message[0] == 0,
            "Successful edit rounded data or retained prior error");
        require(dust3d_animation_timing_check(0.01, 1, &output, nullptr)
                && output.duration_seconds == 0.01 && output.frame_count == 1,
            "Minimum timing or optional diagnostic changed");
        require(dust3d_animation_timing_check(59.99, 4999, &output, &error)
                && output.duration_seconds == 59.99 && output.frame_count == 4999,
            "Maximum timing scalar changed");
        std::cout << "Common timing C API contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

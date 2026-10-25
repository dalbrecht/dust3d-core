// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/spring_step_admission.h>

namespace dust3d {
namespace animation {
    SpringStepAdmission admitDampedSpringStep(double stepSeconds, double stiffness, double damping) noexcept
    {
        if (!std::isfinite(stepSeconds) || stepSeconds <= 0
            || !std::isfinite(stiffness) || stiffness <= 0
            || !std::isfinite(damping) || damping <= 0)
            return SpringStepAdmission::invalidInput;

        // Jury conditions for v'=(1-c*h)*v-k*h*x; x'=x+h*v' require
        // k*h*h+2*c*h < 4 when k,c,h are positive. Avoid squaring large inputs
        // and avoid the cancellation in (sqrt(c*c+4*k)-c)/k.
        const double root = std::hypot(damping, 2.0 * std::sqrt(stiffness));
        const double limit = 2.0 / (0.5 * damping + 0.5 * root);
        return stepSeconds < limit ? SpringStepAdmission::accepted : SpringStepAdmission::unstableStep;
    }

    DampedSpringFramePlan planDampedSpringFrames(double durationSeconds, double frameCount, double stiffness, double damping) noexcept
    {
        if (!std::isfinite(frameCount) || frameCount < 2 || frameCount > 4999
            || std::trunc(frameCount) != frameCount || !std::isfinite(durationSeconds)
            || durationSeconds < 0.001 || durationSeconds > 600.0)
            return {};
        const int count = static_cast<int>(frameCount);
        const float duration = static_cast<float>(durationSeconds);
        const double step = duration / static_cast<double>(count);
        const auto admission = admitDampedSpringStep(step, stiffness, damping);
        if (admission != SpringStepAdmission::accepted)
            return { admission, 0, 0, 0 };
        return { admission, count, duration, step };
    }
}
}

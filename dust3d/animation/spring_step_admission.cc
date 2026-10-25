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
}
}

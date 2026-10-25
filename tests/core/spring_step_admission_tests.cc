// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/animation/spring_step_admission.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}

int main()
{
    using namespace dust3d::animation;
    try {
        for (const auto coefficients : { std::pair<double, double> { 120, 12 }, { 150, 14 } }) {
            require(admitDampedSpringStep(1.0 / 12.0, coefficients.first, coefficients.second) == SpringStepAdmission::accepted,
                "Established twelve-sample scalar step rejected");
            require(admitDampedSpringStep(1.0 / 6.0, coefficients.first, coefficients.second) == SpringStepAdmission::unstableStep,
                "Coarse scalar step outside linear stability region accepted");
        }
        // k=4, c=3 gives the exact Jury boundary h=0.5, without running an integrator.
        require(admitDampedSpringStep(0.5, 4, 3) == SpringStepAdmission::unstableStep,
            "Marginal linear stability boundary accepted");
        require(admitDampedSpringStep(std::nextafter(0.5, 0.0), 4, 3) == SpringStepAdmission::accepted,
            "Strictly interior scalar step rejected");
        require(admitDampedSpringStep(std::nextafter(0.5, 1.0), 4, 3) == SpringStepAdmission::unstableStep,
            "Exterior scalar step accepted");
        for (const double invalid : { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() }) {
            require(admitDampedSpringStep(invalid, 120, 12) == SpringStepAdmission::invalidInput
                    && admitDampedSpringStep(1.0 / 12.0, invalid, 12) == SpringStepAdmission::invalidInput
                    && admitDampedSpringStep(1.0 / 12.0, 120, invalid) == SpringStepAdmission::invalidInput,
                "Invalid scalar coefficient or step accepted");
        }
        const auto precise = planDampedSpringFrames(1.00000003, 12, 120, 12);
        require(precise.admission == SpringStepAdmission::accepted && precise.frameCount == 12
                && precise.durationSeconds == static_cast<float>(1.00000003)
                && precise.stepSeconds == static_cast<float>(1.00000003) / 12.0,
            "Plan must retain the reference float duration and interval");
        for (const auto timing : { std::pair<double, double> { 0.001, 2 }, { 600, 4999 } }) {
            require(planDampedSpringFrames(timing.first, timing.second, 4, 3).admission == SpringStepAdmission::accepted,
                "Legacy timing endpoints rejected before scalar stability admission");
        }
        auto rejectsPlan = [](double seconds, double frames, double stiffness, double damping, SpringStepAdmission expected) {
            const auto plan = planDampedSpringFrames(seconds, frames, stiffness, damping);
            require(plan.admission == expected && plan.frameCount == 0 && plan.durationSeconds == 0 && plan.stepSeconds == 0,
                "Rejected plan must preserve its cause and expose no usable simulation values");
        };
        for (const double seconds : { 0.0, -1.0, 0.0009, 600.01, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() })
            rejectsPlan(seconds, 12, 120, 12, SpringStepAdmission::invalidInput);
        for (const double frames : { 0.0, 1.0, 2.5, 5000.0, 1e20, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() })
            rejectsPlan(1, frames, 120, 12, SpringStepAdmission::invalidInput);
        rejectsPlan(1, 6, 120, 12, SpringStepAdmission::unstableStep);
        rejectsPlan(1, 12, 0, 12, SpringStepAdmission::invalidInput);
        std::cout << "Pure damped spring step admission contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

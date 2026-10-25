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
        std::cout << "Pure damped spring step admission contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

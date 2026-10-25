// SPDX-License-Identifier: MIT
#ifndef DUST3D_ANIMATION_SPRING_STEP_ADMISSION_H_
#define DUST3D_ANIMATION_SPRING_STEP_ADMISSION_H_

namespace dust3d {
namespace animation {
    /// Admission for one semi-implicit Euler step; accepted is not whole-clip safety.
    enum class SpringStepAdmission { accepted,
        invalidInput,
        unstableStep };

    /** Check the strict linear stability region of the reference damped spring update.
     * @param stepSeconds Positive finite simulation interval, not a frame allocation count.
     * @param stiffness Positive finite spring coefficient for unit mass.
     * @param damping Positive finite velocity damping coefficient for unit mass.
     * @return Typed invalid input or unstable step, otherwise accepted; no simulation runs.
     * No allocation or exception. Moving targets, rig transforms and final output bounds
     * still need independent validation. Example: `admitDampedSpringStep(1.0 / 12, 120, 12)`.
     */
    SpringStepAdmission admitDampedSpringStep(double stepSeconds, double stiffness, double damping) noexcept;
}
}
#endif

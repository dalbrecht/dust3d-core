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

    /// Scalar-only plan; rejected plans contain no usable timing or count.
    struct DampedSpringFramePlan {
        SpringStepAdmission admission = SpringStepAdmission::invalidInput;
        int frameCount = 0;
        float durationSeconds = 0;
        double stepSeconds = 0;
    };

    /** Validate raw reference timing before numeric casts and check its actual spring interval.
     * @param durationSeconds Finite duration inside inherited generator limits [0.001,600].
     * @param frameCount Finite integral frame count inside [2,4999], before conversion to int.
     * @param stiffness Positive finite unit-mass spring coefficient.
     * @param damping Positive finite unit-mass damping coefficient.
     * @return Accepted plan with the reference float duration and step, or a typed rejection
     * with cleared values. No allocation, frame evaluation, rounding of stored text or exception.
     * Example: `planDampedSpringFrames(1, 12, 120, 12)`.
     */
    DampedSpringFramePlan planDampedSpringFrames(double durationSeconds, double frameCount, double stiffness, double damping) noexcept;
}
}
#endif

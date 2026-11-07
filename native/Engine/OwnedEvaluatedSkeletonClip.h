// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_OWNED_EVALUATED_SKELETON_CLIP_H_
#define DUST3D_NATIVE_OWNED_EVALUATED_SKELETON_CLIP_H_

#include "RigMatrix.h"
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace dust3d {
struct RigAnimationClip;
struct RigStructure;

namespace native {
    class OwnedEvaluatedSkeletonClip {
    public:
        /// Copy an already evaluated clip for its matching checked source rig.
        /// Limits apply to logical copied bytes, including names, times and matrices.
        /// Throws DocumentError on invalid output, limit violation or cancellation.
        /// Example: auto owned = OwnedEvaluatedSkeletonClip::copy(clip, rig);
        static OwnedEvaluatedSkeletonClip copy(const RigAnimationClip& clip, const RigStructure& rig,
            size_t maximumPayloadBytes = 64 * 1024 * 1024, const std::atomic_bool* cancellation = nullptr);

        const std::string& name() const { return m_name; }
        const std::string& animationType() const { return m_animationType; }
        double durationSeconds() const { return m_durationSeconds; }
        bool loops() const { return m_loops; }
        size_t jointCount() const { return m_jointCount; }
        size_t logicalPayloadBytes() const
        {
            return m_name.size() + m_animationType.size() + m_sampleTimes.size() * sizeof(double)
                + m_modelFromJoint.size() * sizeof(RigMatrix);
        }
        const std::vector<double>& sampleTimes() const { return m_sampleTimes; }
        /// Frame-major matrices in rig.bones order, in the producer's model space.
        const std::vector<RigMatrix>& modelFromJoint() const { return m_modelFromJoint; }

    private:
        OwnedEvaluatedSkeletonClip() = default;

        std::string m_name;
        std::string m_animationType;
        double m_durationSeconds = 0;
        bool m_loops = false;
        size_t m_jointCount = 0;
        std::vector<double> m_sampleTimes;
        std::vector<RigMatrix> m_modelFromJoint;
    };
}
}
#endif

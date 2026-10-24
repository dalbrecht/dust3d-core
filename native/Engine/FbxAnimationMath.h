// SPDX-License-Identifier: MIT
// Copyright (c) 2018 Dust3D Project
#ifndef DUST3D_NATIVE_FBX_ANIMATION_MATH_H_
#define DUST3D_NATIVE_FBX_ANIMATION_MATH_H_
#include "RigMatrix.h"
#include <cstdint>
namespace dust3d {
namespace native {
    /// Convert finite nonnegative seconds to reference FBX ticks, truncating toward
    /// zero at 46,186,158,000 ticks/second. Accept at most 60 seconds, matching the
    /// owned animation boundary. Throws DocumentError INVALID_DOCUMENT for invalid
    /// values or LIMIT_EXCEEDED above 60. Pure, no engine globals or allocations on
    /// success. Example: fbxAnimationTime(1) == 46186158000.
    int64_t fbxAnimationTime(double seconds);
    struct FbxEulerAngles {
        double pitch = 0;
        double yaw = 0;
        double roll = 0;
    };
    /// Decompose a column-major rigid transform as XYZ degrees (Rz*Ry*Rx), using
    /// the reference roll=0 gimbal branch at |sin(yaw)|>1-1e-12. Validate via the
    /// world-input boundary: finite affine right-handed unit rotation and
    /// translation <=128M. Throws typed INVALID_DOCUMENT/LIMIT_EXCEEDED without
    /// modifying input or engine globals. Example: auto angles = fbxEulerAngles(world).
    FbxEulerAngles fbxEulerAngles(const RigMatrix& matrix);
    /// Normalize degrees into (-180, 180], preserving the reference -180 tie.
    /// Inputs must be finite with absolute value <=1M. Throws DocumentError
    /// INVALID_DOCUMENT for nonfinite values or LIMIT_EXCEEDED for bounds.
    /// Example: normalizeFbxAngle(-180) == 180. Pure; no global engine state.
    double normalizeFbxAngle(double degrees);
    /// Return the reference signed shortest difference in [-180, 180], preserving
    /// both signed half-turn ties. Same input limits/errors as normalizeFbxAngle.
    /// Example: shortestFbxAngleDifference(-179, 179) == 2.
    double shortestFbxAngleDifference(double degrees, double referenceDegrees);
    /// Choose an equivalent degree value with difference from previous in
    /// (-180, 180], without the inherited repeated +/-360 loops. Same finite/1M
    /// limits apply to both inputs and result. This accommodates the owned clip's
    /// 5,000-frame maximum (at most 900,000 accumulated degrees). Throws typed
    /// INVALID_DOCUMENT/LIMIT_EXCEEDED as above; no owner or global state.
    /// Example: unwrapFbxAngle(-179, 179) == 181.
    double unwrapFbxAngle(double degrees, double previousDegrees);
}
}
#endif

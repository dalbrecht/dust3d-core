// SPDX-License-Identifier: MIT
// Copyright (c) 2018 Dust3D Project
// Adapted from application/sources/fbx_file.cc; keep reference tie/truncation semantics.
#include "FbxAnimationMath.h"
#include "EngineInternal.h"
#include <cmath>
namespace dust3d {
namespace native {
    namespace {
        void validateAngle(double degrees)
        {
            if (!std::isfinite(degrees))
                throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX rotation contains a nonfinite angle.");
            if (std::abs(degrees) > 1000000)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX rotation exceeds the animation angle bound.");
        }
    }
    int64_t fbxAnimationTime(double seconds)
    {
        if (!std::isfinite(seconds) || seconds < 0)
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "FBX time must be finite and nonnegative.");
        if (seconds > 60)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX time exceeds the owned animation duration bound.");
        // The admitted maximum yields 2,771,169,480,000, safely representable by int64_t.
        return static_cast<int64_t>(seconds * 46186158000.0);
    }
    double normalizeFbxAngle(double degrees)
    {
        validateAngle(degrees);
        double wrapped = std::fmod(degrees, 360.0);
        if (wrapped <= -180.0)
            wrapped += 360.0;
        else if (wrapped > 180.0)
            wrapped -= 360.0;
        return wrapped;
    }
    double shortestFbxAngleDifference(double degrees, double referenceDegrees)
    {
        validateAngle(degrees);
        validateAngle(referenceDegrees);
        double difference = std::fmod(degrees - referenceDegrees, 360.0);
        if (difference < -180.0)
            difference += 360.0;
        else if (difference > 180.0)
            difference -= 360.0;
        return difference;
    }
    double unwrapFbxAngle(double degrees, double previousDegrees)
    {
        validateAngle(degrees);
        validateAngle(previousDegrees);
        // Bounded subtraction cannot overflow; normalization preserves the inherited positive half-turn tie.
        const double result = previousDegrees + normalizeFbxAngle(std::fmod(degrees - previousDegrees, 360.0));
        validateAngle(result);
        return result;
    }
}
}

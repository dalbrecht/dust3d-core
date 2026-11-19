// SPDX-License-Identifier: MIT
// Copyright (c) 2018 Dust3D Project
// Adapted from application/sources/fbx_file.cc; keep reference tie/truncation semantics.
#include "FbxAnimationMath.h"
#include "EngineInternal.h"
#include "RigTransformValidation.h"
#include <algorithm>
#include <cmath>
#include <limits>
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
        if (seconds > 600)
            throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "FBX time exceeds the owned animation duration bound.");
        // The generator-domain maximum is checked at compile time before truncation.
        static_assert(600ULL * 46186158000ULL <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
        return static_cast<int64_t>(seconds * 46186158000.0);
    }
    FbxEulerAngles fbxEulerAngles(const RigMatrix& matrix)
    {
        validateRigTransform(matrix, RigTransformUse::worldInput);
        constexpr double degreesPerRadian = 180.0 / 3.14159265358979323846;
        const double sineYaw = std::max(-1.0, std::min(1.0, -matrix[2]));
        FbxEulerAngles result;
        result.yaw = std::asin(sineYaw) * degreesPerRadian;
        if (std::abs(sineYaw) > 1.0 - 1e-12) {
            // Match the reference: roll and pitch are coupled here; absorb roll into pitch.
            result.pitch = std::atan2(sineYaw > 0 ? matrix[4] : -matrix[4], matrix[5]) * degreesPerRadian;
        } else {
            result.pitch = std::atan2(matrix[6], matrix[10]) * degreesPerRadian;
            result.roll = std::atan2(matrix[1], matrix[0]) * degreesPerRadian;
        }
        return result;
    }
    FbxEulerAngles continuousFbxEulerAngles(FbxEulerAngles current, FbxEulerAngles previous)
    {
        constexpr double radiansPerDegree = 3.14159265358979323846 / 180.0;
        if (std::abs(std::sin(current.yaw * radiansPerDegree)) > 1.0 - 1e-12) {
            // At the singularity, preserve the preceding roll and transfer the coupled angle into pitch.
            const double rollChange = previous.roll - current.roll;
            current.pitch += current.yaw >= 0 ? rollChange : -rollChange;
            current.roll = previous.roll;
        }
        const FbxEulerAngles alternate { current.pitch + 180, 180 - current.yaw, current.roll + 180 };
        const auto nearest = [&](FbxEulerAngles value) -> FbxEulerAngles {
            return { unwrapFbxAngle(value.pitch, previous.pitch), unwrapFbxAngle(value.yaw, previous.yaw),
                unwrapFbxAngle(value.roll, previous.roll) };
        };
        const auto score = [&](FbxEulerAngles value) {
            const double pitch = value.pitch - previous.pitch;
            const double yaw = value.yaw - previous.yaw;
            const double roll = value.roll - previous.roll;
            return pitch * pitch + yaw * yaw + roll * roll;
        };
        const auto direct = nearest(current);
        const auto flipped = nearest(alternate);
        return score(flipped) < score(direct) ? flipped : direct;
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

// SPDX-License-Identifier: MIT
#include "RigTransformValidation.h"
#include "EngineInternal.h"
#include <cmath>
namespace dust3d {
namespace native {
    namespace {
        struct Rules {
            double maximumTranslation;
            Dust3DStatus invalidStatus;
        };
        Rules rulesFor(RigTransformUse use)
        {
            switch (use) {
            case RigTransformUse::localInput:
                return { 1000000, DUST3D_STATUS_INVALID_DOCUMENT };
            case RigTransformUse::worldInput:
                return { 128000000, DUST3D_STATUS_INVALID_DOCUMENT };
            case RigTransformUse::generatedWorld:
                return { 128000000, DUST3D_STATUS_GENERATION_FAILED };
            case RigTransformUse::generatedSkin:
                return { 256000000, DUST3D_STATUS_GENERATION_FAILED };
            }
            throw DocumentError(DUST3D_STATUS_INVALID_DOCUMENT, "Unknown rigid transform validation context.");
        }
    }
    void validateRigTransform(const RigMatrix& value, RigTransformUse use)
    {
        const auto rules = rulesFor(use);
        for (double component : value)
            if (!std::isfinite(component))
                throw DocumentError(rules.invalidStatus, "Rig pose matrix contains nonfinite values.");
        for (size_t index : { size_t(12), size_t(13), size_t(14) })
            if (std::abs(value[index]) > rules.maximumTranslation)
                throw DocumentError(DUST3D_STATUS_LIMIT_EXCEEDED, "Rig pose translation exceeds coordinate bounds.");
        for (size_t index : { size_t(3), size_t(7), size_t(11) })
            if (std::abs(value[index]) > 1e-8)
                throw DocumentError(rules.invalidStatus, "Rig pose matrix is not affine.");
        if (std::abs(value[15] - 1) > 1e-8)
            throw DocumentError(rules.invalidStatus, "Rig pose matrix has an invalid homogeneous component.");
        for (size_t column = 0; column < 3; ++column)
            for (size_t row = 0; row < 3; ++row)
                if (std::abs(value[column * 4 + row]) > 1.000001)
                    throw DocumentError(rules.invalidStatus, "Rig pose rotation entry exceeds a unit rigid frame.");
        for (size_t first = 0; first < 3; ++first)
            for (size_t second = 0; second < 3; ++second) {
                double dot = 0;
                for (size_t axis = 0; axis < 3; ++axis)
                    dot += value[first * 4 + axis] * value[second * 4 + axis];
                // Finite rotation entries bounded above make these small dot products nonoverflowing.
                if (std::abs(dot - (first == second ? 1 : 0)) > 1e-6)
                    throw DocumentError(rules.invalidStatus, "Rig pose matrix is not a rigid frame.");
            }
        const double determinant = value[0] * (value[5] * value[10] - value[9] * value[6])
            - value[4] * (value[1] * value[10] - value[9] * value[2])
            + value[8] * (value[1] * value[6] - value[5] * value[2]);
        // The same bounded entries keep the six determinant products finite.
        if (std::abs(determinant - 1) > 1e-6)
            throw DocumentError(rules.invalidStatus, "Rig pose matrix reverses handedness.");
    }
}
}

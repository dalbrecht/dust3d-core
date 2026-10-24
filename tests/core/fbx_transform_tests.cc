// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxAnimationMath.h"
#include "RigMatrix.h"
#include "RigTransformValidation.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d::native;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
RigMatrix rotation(double pitch, double yaw, double roll)
{
    const double radians = std::acos(-1.0) / 180;
    const double sx = std::sin(pitch * radians), cx = std::cos(pitch * radians);
    const double sy = std::sin(yaw * radians), cy = std::cos(yaw * radians);
    const double sz = std::sin(roll * radians), cz = std::cos(roll * radians);
    return { cz * cy, sz * cy, -sy, 0,
        cz * sy * sx - sz * cx, sz * sy * sx + cz * cx, cy * sx, 0,
        cz * sy * cx + sz * sx, sz * sy * cx - cz * sx, cy * cx, 0,
        2, -3, 4, 1 };
}
void roundTrip(const RigMatrix& source)
{
    const auto original = source;
    const auto angles = fbxEulerAngles(source);
    const auto rebuilt = rotation(angles.pitch, angles.yaw, angles.roll);
    for (size_t column = 0; column < 3; ++column)
        for (size_t row = 0; row < 3; ++row)
            require(std::abs(source[column * 4 + row] - rebuilt[column * 4 + row]) < 1e-7, "FBX XYZ Euler reconstruction changed rotation");
    require(source == original, "FBX decomposition mutated input");
}
void rejects(RigMatrix source, Dust3DStatus expected = DUST3D_STATUS_INVALID_DOCUMENT)
{
    try {
        (void)fbxEulerAngles(source);
    } catch (const DocumentError& cause) {
        require(cause.status() == expected, "FBX matrix validation lost its typed cause");
        return;
    }
    throw std::runtime_error("Invalid FBX transform accepted");
}
}
int main()
{
    try {
        for (auto use : { RigTransformUse::localInput, RigTransformUse::worldInput, RigTransformUse::generatedWorld, RigTransformUse::generatedSkin }) {
            const auto status = use == RigTransformUse::localInput || use == RigTransformUse::worldInput
                ? DUST3D_STATUS_INVALID_DOCUMENT
                : DUST3D_STATUS_GENERATION_FAILED;
            const double bound = use == RigTransformUse::localInput ? 1000000 : use == RigTransformUse::generatedSkin ? 256000000
                                                                                                                      : 128000000;
            auto matrix = rotation(0, 0, 0);
            for (size_t axis : { size_t(12), size_t(13), size_t(14) })
                for (double sign : { -1.0, 1.0 }) {
                    matrix = rotation(0, 0, 0);
                    matrix[axis] = sign * bound;
                    validateRigTransform(matrix, use);
                    matrix[axis] = sign * (bound + 1);
                    try {
                        validateRigTransform(matrix, use);
                        throw std::runtime_error("Rigid translation bound accepted");
                    } catch (const DocumentError& cause) {
                        require(cause.status() == DUST3D_STATUS_LIMIT_EXCEEDED, "Coordinate status changed");
                    }
                }
            matrix = rotation(0, 0, 0);
            matrix[0] = std::numeric_limits<double>::quiet_NaN();
            try {
                validateRigTransform(matrix, use);
                throw std::runtime_error("Nonfinite rigid frame accepted");
            } catch (const DocumentError& cause) {
                require(cause.status() == status, "Input/generated cause classification changed");
            }
        }
        try {
            validateRigTransform(rotation(0, 0, 0), static_cast<RigTransformUse>(999));
            throw std::runtime_error("Unknown validation context accepted");
        } catch (const DocumentError& cause) {
            require(cause.status() == DUST3D_STATUS_INVALID_DOCUMENT, "Unknown context cause changed");
        }
        for (double yaw : { -90.0, 90.0 }) {
            auto noisy = rotation(0, yaw, 0);
            noisy[2] *= 1.0000001;
            const auto angles = fbxEulerAngles(noisy);
            require(std::abs(angles.yaw - yaw) < 1e-8 && angles.roll == 0, "Reference sine clamp changed");
        }
        for (double pitch : { -180.0, -90.0, -37.0, 0.0, 41.0, 90.0, 180.0 })
            for (double yaw : { -90.0, -89.9999999, -45.0, 0.0, 45.0, 89.9999999, 90.0 })
                for (double roll : { -180.0, -90.0, -21.0, 0.0, 63.0, 90.0, 180.0 })
                    roundTrip(rotation(pitch, yaw, roll));
        const auto positive = fbxEulerAngles(rotation(37, 90, 21));
        const auto negative = fbxEulerAngles(rotation(37, -90, 21));
        require(std::abs(positive.pitch - 16) < 1e-8 && std::abs(positive.yaw - 90) < 1e-8 && positive.roll == 0, "Positive gimbal convention changed");
        require(std::abs(negative.pitch - 58) < 1e-8 && std::abs(negative.yaw + 90) < 1e-8 && negative.roll == 0, "Negative gimbal convention changed");
        for (size_t index = 0; index < 16; ++index)
            for (double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
                auto matrix = rotation(0, 0, 0);
                matrix[index] = invalid;
                rejects(matrix);
            }
        for (size_t index : { size_t(3), size_t(7), size_t(11), size_t(15) }) {
            auto matrix = rotation(0, 0, 0);
            matrix[index] += 0.1;
            rejects(matrix);
        }
        for (size_t index : { size_t(0), size_t(5), size_t(10) }) {
            auto reflected = rotation(0, 0, 0);
            reflected[index] = -1;
            rejects(reflected);
            auto scaled = rotation(0, 0, 0);
            scaled[index] = 1.1;
            rejects(scaled);
        }
        auto sheared = rotation(0, 0, 0);
        sheared[4] = 0.1;
        rejects(sheared);
        for (size_t index : { size_t(12), size_t(13), size_t(14) }) {
            auto matrix = rotation(0, 0, 0);
            matrix[index] = 128000000;
            roundTrip(matrix);
            matrix[index] += 1;
            rejects(matrix, DUST3D_STATUS_LIMIT_EXCEEDED);
        }
        std::cout << "FBX transform conventions passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

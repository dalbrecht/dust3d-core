// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "FbxAnimationMath.h"
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
void near(double actual, double expected)
{
    require(std::isfinite(actual) && std::abs(actual - expected) < 1e-8, "FBX angle differs from reference");
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus expected)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == expected, "FBX math lost typed boundary cause");
        return;
    }
    throw std::runtime_error("Invalid FBX math input accepted");
}
}
int main()
{
    try {
        require(fbxAnimationTime(0) == 0 && fbxAnimationTime(1) == 46186158000LL, "FBX seconds/ticks convention changed");
        require(fbxAnimationTime(60) == 2771169480000LL, "FBX duration boundary changed");
        require(fbxAnimationTime(1.0 / 24) == static_cast<int64_t>((1.0 / 24) * 46186158000.0), "FBX reference truncation changed");
        for (double angle : { -1080.0, -540.0, -180.0, 0.0, 180.0, 540.0, 1080.0 }) {
            double expected = std::fmod(angle, 360.0);
            if (expected <= -180)
                expected += 360;
            else if (expected > 180)
                expected -= 360;
            near(normalizeFbxAngle(angle), expected);
        }
        near(shortestFbxAngleDifference(-180, 0), -180);
        near(shortestFbxAngleDifference(180, 0), 180);
        near(shortestFbxAngleDifference(-179, 179), 2);
        near(shortestFbxAngleDifference(179, -179), -2);
        near(unwrapFbxAngle(-179, 179), 181);
        near(unwrapFbxAngle(179, -179), -181);
        near(unwrapFbxAngle(-180, 0), 180);
        for (int previous = -1080; previous <= 1080; previous += 17) {
            for (int angle = -1080; angle <= 1080; angle += 31) {
                double expected = angle;
                while (expected - previous > 180)
                    expected -= 360;
                while (expected - previous <= -180)
                    expected += 360;
                near(unwrapFbxAngle(angle, previous), expected);
            }
        }
        near(unwrapFbxAngle(-900000, 900000), 900000);
        near(unwrapFbxAngle(900000, -900000), -900000);
        double previous = 0;
        for (size_t index = 0; index < 5000; ++index) {
            previous = unwrapFbxAngle(normalizeFbxAngle(static_cast<double>(index) * 179), previous);
            near(previous, static_cast<double>(index) * 179);
        }
        for (double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() }) {
            rejects([&] { (void)fbxAnimationTime(invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
            rejects([&] { (void)normalizeFbxAngle(invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
            rejects([&] { (void)shortestFbxAngleDifference(0, invalid); }, DUST3D_STATUS_INVALID_DOCUMENT);
            rejects([&] { (void)unwrapFbxAngle(invalid, 0); }, DUST3D_STATUS_INVALID_DOCUMENT);
        }
        rejects([] { (void)fbxAnimationTime(-0.001); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { (void)fbxAnimationTime(60.001); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { (void)normalizeFbxAngle(1000001); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { (void)shortestFbxAngleDifference(-1000001, 0); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { (void)unwrapFbxAngle(0, 1000001); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { (void)unwrapFbxAngle(100, 1000000); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        std::cout << "FBX animation conventions passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

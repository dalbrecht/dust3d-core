// SPDX-License-Identifier: MIT
#include <cmath>
#include <dust3d/base/position_key.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void testPositionQuantizationBounds()
{
    const double exclusiveLimit = std::ldexp(1.0, std::numeric_limits<long>::digits);
    for (double invalid : { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::max(), exclusiveLimit / 100000.0,
             -exclusiveLimit * 2 / 100000.0, 1e18, -1e18 }) {
        for (size_t axis = 0; axis < 3; ++axis) {
            dust3d::Vector3 vertex;
            vertex[axis] = invalid;
            bool rejected = false;
            try {
                const dust3d::PositionKey key(vertex);
            } catch (const dust3d::PositionQuantizationError& cause) {
                rejected = std::string(cause.what()).find("quantization") != std::string::npos;
            }
            if (!rejected)
                throw std::runtime_error("Unsafe coordinate reached position quantization");
        }
    }
    const double safeCoordinate = std::nextafter(exclusiveLimit / 100000.0, 0.0);
    const dust3d::PositionKey positive(safeCoordinate, 0, 0), negative(-safeCoordinate, 0, 0), zero(0, 0, 0);
    if (!(negative < zero && zero < positive)
        || !(dust3d::PositionKey(.123456, -.123456, .000009) == dust3d::PositionKey(.123459, -.123459, 0)))
        throw std::runtime_error("Valid position quantization changed its ordering or truncation");
}
}
int main()
{
    try {
        testPositionQuantizationBounds();
        std::cout << "Position quantization contracts passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

/*
 *  Copyright (c) 2016-2021 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved. 
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:

 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.

 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#ifndef DUST3D_BASE_POSITION_KEY_H_
#define DUST3D_BASE_POSITION_KEY_H_

#include <dust3d/base/vector3.h>
#include <stdexcept>

namespace dust3d {

// Quantization rejects nonfinite or unrepresentable scaled coordinates before
// conversion. Callers may propagate this typed failure to their error boundary.
class PositionQuantizationError : public std::range_error {
public:
    PositionQuantizationError()
        : std::range_error("Coordinate exceeds the finite position quantization range.")
    {
    }
};

class PositionKey {
public:
    // Both constructors throw PositionQuantizationError for unsafe coordinates.
    PositionKey(const Vector3& v);
    PositionKey(double x, double y, double z);
    bool operator<(const PositionKey& right) const;
    bool operator==(const PositionKey& right) const;

private:
    long m_intX;
    long m_intY;
    long m_intZ;

    static long m_toIntFactor;
};

}

#endif

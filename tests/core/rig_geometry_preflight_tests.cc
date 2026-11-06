// SPDX-License-Identifier: MIT
#include "RigFinalData.h"
#include <dust3d/base/object.h>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
void require(bool v, const char* m)
{
    if (!v)
        throw std::runtime_error(m);
}
template <class F>
void rejects(F run, RigFinalDataFailure expected)
{
    try {
        run();
    } catch (const RigFinalDataError& e) {
        require(e.failure() == expected && e.what()[0], "Wrong geometry failure");
        return;
    }
    throw std::runtime_error("Expected geometry rejection");
}
Object geometry()
{
    Object o;
    o.vertices = { { 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 } };
    o.triangles = { { 0, 1, 2 } };
    return o;
}
int main()
{
    try {
        const auto source = geometry();
        validateRigGeometry(source);
        auto degenerate = source;
        degenerate.triangles = { { 0, 0, 0 }, { 0, 0, 0 } };
        validateRigGeometry(degenerate); // topology admission does not repair/filter render faces
        auto boundary = source;
        boundary.vertices[0].setX(1000000);
        if (std::numeric_limits<long>::digits > 36)
            validateRigGeometry(boundary);
        else
            rejects([&] { validateRigGeometry(boundary); }, RigFinalDataFailure::limitExceeded);
        auto o = source;
        o.vertices[0].setX(std::numeric_limits<double>::infinity());
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        o = source;
        o.vertices[1].setY(std::numeric_limits<double>::quiet_NaN());
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        o = source;
        o.vertices[2].setZ(1000001);
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::limitExceeded);
        o = source;
        o.triangles[0][2] = 3;
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        o = source;
        o.triangles[0] = { 0, 1 };
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        o = source;
        o.triangles.clear();
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        o = source;
        o.vertices.clear();
        rejects([&] { validateRigGeometry(o); }, RigFinalDataFailure::invalidData);
        RigFinalDataOptions options;
        options.maximumVertices = 2;
        rejects([&] { validateRigGeometry(source, options); }, RigFinalDataFailure::limitExceeded);
        options.maximumVertices = 0;
        rejects([&] { validateRigGeometry(source, options); }, RigFinalDataFailure::invalidData);
        options.maximumVertices = 1000000;
        o = source;
        o.triangles.push_back({ 2, 1, 0 });
        rejects([&] { validateRigGeometry(o, options, 1); }, RigFinalDataFailure::limitExceeded);
        rejects([&] { validateRigGeometry(source, options, 0); }, RigFinalDataFailure::invalidData);
        std::atomic_bool cancelled { true };
        options.cancellation = &cancelled;
        rejects([&] { validateRigGeometry(source, options); }, RigFinalDataFailure::cancelled);
        cancelled.store(false);
        validateRigGeometry(source, options);
        require(source.vertices.size() == 3 && source.triangles[0][2] == 2, "Validation mutated input");
        std::cout << "Rig geometry preflight contracts passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

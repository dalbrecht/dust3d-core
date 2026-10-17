// SPDX-License-Identifier: MIT
#ifndef DUST3D_TEST_PRECISION_FIXTURE_H_
#define DUST3D_TEST_PRECISION_FIXTURE_H_
#include <string>

inline std::string precisionModelXml()
{
    const std::string firstPart = "{00000100-0000-4000-8000-000000000100}";
    const std::string secondPart = "{00000101-0000-4000-8000-000000000101}";
    const auto xml = "<canvas originX='0' originY='0' originZ='0'><nodes><node id='{00001000-0000-4000-8000-000000001000}' partId='" + firstPart
        + "' x='0' y='0' z='0' radius='.001'/><node id='{00001001-0000-4000-8000-000000001001}' partId='" + secondPart
        + "' x='1000000' y='1000000' z='1000000' radius='.001'/></nodes><edges/><parts><part id='" + firstPart
        + "'/><part id='" + secondPart + "'/></parts><components><component id='{00000200-0000-4000-8000-000000000200}' combineMode='Uncombined' linkDataType='partId' linkData='" + secondPart
        + "'/><component id='{00000201-0000-4000-8000-000000000201}' combineMode='Uncombined' linkDataType='partId' linkData='" + firstPart + "'/></components></canvas>";
    return xml;
}
#endif

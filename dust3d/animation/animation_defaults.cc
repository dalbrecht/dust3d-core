/*
 *  Copyright (c) 2026 Jeremy HU <jeremy-at-dust3d dot org>. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include <dust3d/animation/animation_catalog.h>
#include <dust3d/animation/animation_defaults.h>

namespace dust3d {
namespace animation {
    std::pair<double, int> defaultTiming(const std::string& type)
    {
        const auto& catalog = animation::clipCatalog();
        auto found = catalog.find(type);
        return found == catalog.end() ? std::make_pair(3.0, 90)
                                      : std::make_pair(found->second.duration, found->second.samples);
    }

    AnimationParams defaultParameters(const std::string& type)
    {
        AnimationParams result;
        auto found = animation::clipCatalog().find(type);
        if (found == animation::clipCatalog().end())
            return result;
        result.setValue("durationSeconds", found->second.duration);
        result.setValue("frameCount", found->second.samples);
        for (const auto& parameter : found->second.parameters)
            result.setValue(parameter.first, parameter.second);
        return result;
    }

}
}

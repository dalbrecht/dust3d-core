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

#ifndef DUST3D_ANIMATION_ANIMATION_DEFAULTS_H_
#define DUST3D_ANIMATION_ANIMATION_DEFAULTS_H_
#include <dust3d/animation/animation_parameters.h>
#include <string>
#include <utility>

namespace dust3d {
namespace animation {
    /** Return inherited timing declarations; unknown types retain the legacy 3-second/90-sample fallback.
     * @param type Exact case-sensitive type identity, including legacy aliases.
     * @return Duration in seconds and sample count; no clip is evaluated.
     * Example: `auto timing = dust3d::animation::defaultTiming("BipedSprint");`
     */
    std::pair<double, int> defaultTiming(const std::string& type);

    /** Copy inherited numeric defaults into their original persisted string representation.
     * @param type Exact case-sensitive catalog identity; unknown types produce an empty value map.
     * @return Independent durationSeconds/frameCount plus per-clip overrides; no rig or sampler is used.
     * Normal C++ construction errors propagate to the C++ caller.
     * Example: `auto values = dust3d::animation::defaultParameters("BipedIdle");`
     */
    AnimationParams defaultParameters(const std::string& type);
}
}
#endif

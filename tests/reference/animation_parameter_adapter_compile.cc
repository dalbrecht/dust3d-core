// SPDX-License-Identifier: MIT
#include <application/sources/animation_parameter_table.h>
// Compile-only: preserve the reference widget's callback/value interface; never run its unchecked inverse callbacks.
const std::vector<AnimationParameterDef>& referenceAnimationParameters(const std::string& type)
{
    return getAnimationParameterDefs(type);
}

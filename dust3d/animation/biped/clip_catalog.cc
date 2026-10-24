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

#include <dust3d/animation/biped/bow.h>
#include <dust3d/animation/biped/bow_aim.h>
#include <dust3d/animation/biped/bow_draw.h>
#include <dust3d/animation/biped/bow_shot.h>
#include <dust3d/animation/biped/cast_recover.h>
#include <dust3d/animation/biped/cast_release.h>
#include <dust3d/animation/biped/cast_start.h>
#include <dust3d/animation/biped/channel_enter.h>
#include <dust3d/animation/biped/channel_exit.h>
#include <dust3d/animation/biped/channel_interrupt.h>
#include <dust3d/animation/biped/cheer.h>
#include <dust3d/animation/biped/chop.h>
#include <dust3d/animation/biped/clap.h>
#include <dust3d/animation/biped/clip_catalog.h>
#include <dust3d/animation/biped/crouch_enter.h>
#include <dust3d/animation/biped/crouch_exit.h>
#include <dust3d/animation/biped/crouch_idle.h>
#include <dust3d/animation/biped/dance.h>
#include <dust3d/animation/biped/dodge_roll.h>
#include <dust3d/animation/biped/drink.h>
#include <dust3d/animation/biped/eat.h>
#include <dust3d/animation/biped/gather.h>
#include <dust3d/animation/biped/get_up.h>
#include <dust3d/animation/biped/interact.h>
#include <dust3d/animation/biped/knockdown.h>
#include <dust3d/animation/biped/mine.h>
#include <dust3d/animation/biped/mounted_idle.h>
#include <dust3d/animation/biped/mounted_ride.h>
#include <dust3d/animation/biped/one_hand_slash.h>
#include <dust3d/animation/biped/parry.h>
#include <dust3d/animation/biped/pick_up.h>
#include <dust3d/animation/biped/point.h>
#include <dust3d/animation/biped/sit_down.h>
#include <dust3d/animation/biped/sit_idle.h>
#include <dust3d/animation/biped/sleep_idle.h>
#include <dust3d/animation/biped/sleep_lie_down.h>
#include <dust3d/animation/biped/sneak.h>
#include <dust3d/animation/biped/sprint.h>
#include <dust3d/animation/biped/stand_up.h>
#include <dust3d/animation/biped/stunned.h>
#include <dust3d/animation/biped/swim_backward.h>
#include <dust3d/animation/biped/swim_forward.h>
#include <dust3d/animation/biped/swim_idle.h>
#include <dust3d/animation/biped/swim_left.h>
#include <dust3d/animation/biped/swim_right.h>
#include <dust3d/animation/biped/talk.h>
#include <dust3d/animation/biped/two_hand_swing.h>
#include <dust3d/animation/biped/wake_up.h>
#include <dust3d/animation/biped/wave.h>
namespace dust3d {
namespace biped {
    const std::vector<ClipDefinition>& additionalClips()
    {
        static const std::vector<ClipDefinition> clips = {
#define DUST3D_BIPED_CLIP(type, duration, samples, loop, entryPose, exitPose, generator) \
    { type, duration, samples, loop, entryPose, exitPose, generator },
#include <dust3d/animation/biped/clip_definitions.inc>
#undef DUST3D_BIPED_CLIP
        };
        return clips;
    }
    const ClipDefinition* additionalClip(const std::string& type)
    {
        for (const auto& clip : additionalClips())
            if (type == clip.type)
                return &clip;
        return nullptr;
    }
} // namespace biped
} // namespace dust3d

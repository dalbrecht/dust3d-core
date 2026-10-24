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

// Shared declaration data: metadata readers ignore the generator token.
DUST3D_BIPED_CLIP("BipedSprint", 0.65, 32, true, "cycle", "cycle", sprint)
DUST3D_BIPED_CLIP("BipedSneak", 1.4, 44, true, "crouched", "crouched", sneak)
DUST3D_BIPED_CLIP("BipedCrouchEnter", 0.45, 24, false, "relaxed", "crouched", crouchEnter)
DUST3D_BIPED_CLIP("BipedCrouchIdle", 2, 60, true, "crouched", "crouched", crouchIdle)
DUST3D_BIPED_CLIP("BipedCrouchExit", 0.45, 24, false, "crouched", "relaxed", crouchExit)
DUST3D_BIPED_CLIP("BipedMountedIdle", 2, 60, true, "mounted", "mounted", mountedIdle)
DUST3D_BIPED_CLIP("BipedMountedRide", 1, 40, true, "mounted", "mounted", mountedRide)
DUST3D_BIPED_CLIP("BipedStunned", 1.8, 60, true, "stunned", "stunned", stunned)
DUST3D_BIPED_CLIP("BipedKnockdown", 0.9, 48, false, "relaxed", "knockedDown", knockdown)
DUST3D_BIPED_CLIP("BipedGetUp", 2.4, 80, false, "knockedDown", "relaxed", getUp)
DUST3D_BIPED_CLIP("BipedDodgeRoll", 0.8, 48, false, "relaxed", "relaxed", dodgeRoll)
DUST3D_BIPED_CLIP("BipedSwimIdle", 2, 48, true, "swimming", "swimming", swimIdle)
DUST3D_BIPED_CLIP("BipedSwimForward", 1.4, 48, true, "swimming", "swimming", swimForward)
DUST3D_BIPED_CLIP("BipedSwimBackward", 1.4, 48, true, "swimming", "swimming", swimBackward)
DUST3D_BIPED_CLIP("BipedSwimLeft", 1.4, 48, true, "swimming", "swimming", swimLeft)
DUST3D_BIPED_CLIP("BipedSwimRight", 1.4, 48, true, "swimming", "swimming", swimRight)
DUST3D_BIPED_CLIP("BipedOneHandSlash", 0.8, 40, false, "relaxed", "relaxed", oneHandSlash)
DUST3D_BIPED_CLIP("BipedTwoHandSwing", 1.1, 48, false, "relaxed", "relaxed", twoHandSwing)
DUST3D_BIPED_CLIP("BipedBowDraw", 0.8, 40, false, "relaxed", "bowAim", bowDraw)
DUST3D_BIPED_CLIP("BipedBowAim", 2, 60, true, "bowAim", "bowAim", bowAim)
DUST3D_BIPED_CLIP("BipedBowShot", 0.7, 36, false, "bowAim", "relaxed", bowShot)
DUST3D_BIPED_CLIP("BipedParry", 0.6, 32, false, "relaxed", "relaxed", parry)
DUST3D_BIPED_CLIP("BipedCastStart", 0.25, 20, false, "relaxed", "castPrepared", castStart)
DUST3D_BIPED_CLIP("BipedCastRelease", 0.3, 24, false, "castPrepared", "castReleased", castRelease)
DUST3D_BIPED_CLIP("BipedCastRecover", 0.45, 28, false, "castReleased", "relaxed", castRecover)
DUST3D_BIPED_CLIP("BipedChannelEnter", 0.5, 28, false, "relaxed", "channel", channelEnter)
DUST3D_BIPED_CLIP("BipedChannelExit", 0.5, 28, false, "channel", "relaxed", channelExit)
DUST3D_BIPED_CLIP("BipedChannelInterrupt", 0.3, 24, false, "channel", "relaxed", channelInterrupt)
DUST3D_BIPED_CLIP("BipedGather", 1.8, 60, false, "relaxed", "relaxed", gather)
DUST3D_BIPED_CLIP("BipedPickUp", 1.2, 48, false, "relaxed", "relaxed", pickUp)
DUST3D_BIPED_CLIP("BipedInteract", 1, 36, false, "relaxed", "relaxed", interact)
DUST3D_BIPED_CLIP("BipedMine", 1.2, 48, true, "work", "work", mine)
DUST3D_BIPED_CLIP("BipedChop", 1.2, 48, true, "work", "work", chop)
DUST3D_BIPED_CLIP("BipedDrink", 2.2, 64, false, "relaxed", "relaxed", drink)
DUST3D_BIPED_CLIP("BipedEat", 2.4, 72, false, "relaxed", "relaxed", eat)
DUST3D_BIPED_CLIP("BipedSitDown", 1, 48, false, "relaxed", "seated", sitDown)
DUST3D_BIPED_CLIP("BipedSitIdle", 3, 72, true, "seated", "seated", sitIdle)
DUST3D_BIPED_CLIP("BipedStandUp", 1, 48, false, "seated", "relaxed", standUp)
DUST3D_BIPED_CLIP("BipedSleepLieDown", 3, 90, false, "relaxed", "sleeping", sleepLieDown)
DUST3D_BIPED_CLIP("BipedSleepIdle", 4, 90, true, "sleeping", "sleeping", sleepIdle)
DUST3D_BIPED_CLIP("BipedWakeUp", 3, 90, false, "sleeping", "relaxed", wakeUp)
DUST3D_BIPED_CLIP("BipedWave", 2, 64, false, "relaxed", "relaxed", wave)
DUST3D_BIPED_CLIP("BipedCheer", 2, 64, false, "relaxed", "relaxed", cheer)
DUST3D_BIPED_CLIP("BipedBow", 1.8, 60, false, "relaxed", "relaxed", bow)
DUST3D_BIPED_CLIP("BipedPoint", 1.5, 48, false, "relaxed", "relaxed", point)
DUST3D_BIPED_CLIP("BipedClap", 2, 64, false, "relaxed", "relaxed", clap)
DUST3D_BIPED_CLIP("BipedDance", 2.4, 80, true, "cycle", "cycle", dance)
DUST3D_BIPED_CLIP("BipedTalk", 3, 90, true, "cycle", "cycle", talk)

# ADR 0204: owned PCM16/WAV serialization

Status: local implementation for core #318; full gate and publication pending.

Qt exposes standalone WAV and model-plus-WAV export. Its SoundGenerator encoder
writes PCM16 RIFF/WAVE but narrows lengths without a checked public boundary.
Native animation and sound generation remain uncompiled/quarantined. A reusable
PCM encoder is needed before copied audio can reach parent-owned publication.

`Dust3DAudioWav.h` exposes a generator-independent C API. Planning takes scalar
sample count, rate, channels and a mandatory caller output quota, and performs
no sample access/allocation. It validates complete interleaved frames, uint16
block alignment, uint32 byte rate and RIFF chunk length, platform addressability
and total output quota (including the 44-byte header). Empty PCM encodes as a
valid empty WAV; the app must independently disable Export WAV without sound.
The API allows any representable positive PCM channel count; it supplies no
speaker mapping or extensible/floating-point encoding.

Creation copies signed PCM16 into immutable owned little-endian bytes. Borrowed
input must remain valid/stable for the call; borrowed output expires with its
owner. Calls share no mutable state or inherited engine globals. Errors are
explicit status values; no exception crosses C. Allocation/unknown exception
fallbacks are containment boundaries, not fault-injection acceptance evidence.

Tests use tiny signed-extrema mono/stereo buffers, exact headers, complete
interleaving, input mutation, concurrent owner lifetimes, null-owner access and
empty PCM. Scalar-only planning tests cover invalid format/frame metadata,
RIFF limits and quotas without large buffers/allocations. A pure C consumer's
54-byte output is independently decoded using Python stdlib wave. Fresh scoped
Debug and ASan/UBSan C/C++ contracts passed; these link only AudioWavBridge.cc.
The initial tests failed compilation because the public header did not exist.
The full CMake engine target compiled with the new source. The standalone Swift6
consumer in tests/SwiftAudioWav copies bytes before destruction and transfers an
immutable array through an actor; that contract passed with strict concurrency.
SwiftPM integration and the required full core gate remain pending.

No synthesis, playback, animation/rig evaluation, native file publication or
combined model export is enabled. Production pin, quarantines and the prior
rejection of the broad CTest inventory remain unchanged. Scoped positive tests
are not a full core gate, full parity, fault tolerance or app adoption signoff.
No public PR until the required full gate can be completed within authorization.

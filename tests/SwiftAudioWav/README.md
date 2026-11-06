# Standalone Swift PCM16/WAV ownership contract

This consumer imports only `Dust3DAudioWav.h`, copies the owner's 54-byte WAV
before destroying it, and transfers the immutable Swift array through an actor.
No native generator, geometry, animation, playback or file writer is linked.

After the source quarantine guards and inspection of this consumer, build a
fresh `AudioWavBridge.cc` object with C++17. In a temporary module directory,
copy the public header and write `module.modulemap` containing:

```
module Dust3DAudioWav { header "Dust3DAudioWav.h" export * }
```

Compile this source using `swiftc -swift-version 6 -strict-concurrency=complete
-warnings-as-errors -parse-as-library -I <module-directory> main.swift
<AudioWavBridge.o> -lc++ -o <fresh-smoke>` and run the resulting owned consumer.
This check does not substitute for CMake/CTest, SwiftPM integration or app adoption.

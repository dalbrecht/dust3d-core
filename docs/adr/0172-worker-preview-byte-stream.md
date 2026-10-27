# ADR 0172: Bounded incremental preview byte endpoints

Status: proposed for reviewed delivery. Tracks #270 and Apple #280.

## Decision

Add collecting-thread byte endpoints over accepted D3PC framing. Writer accepts
arbitrary readable append spans and emits canonical headers/bodies through a complete-
span-or-throw sink. It buffers one frozen chunk, requires explicit start and exact
promised bytes before finish, and rejects overrun before copying/emitting that span.
Finish emits the end marker but never closes a descriptor or claims physical EOF.

Reader accepts arbitrarily split actual bytes, retaining one80-byte header plus one
policy-sized body workspace. Decode/identity errors seal the enclosing owner, including
errors occurring before PreviewChunkSequence could observe the header. A complete
chunk reaches the provisional sink only after sequence/body-size validation. One
trailing byte after end rejects immediately. Incomplete header/body, missing end and
early/repeated EOF cannot complete framing. Only the future actual channel reader may
call finishAtEOF on an observed EOF.

Callbacks borrow bytes only until return, must copy/consume synchronously, and must
not publish or dispatch native work. Guard callback reentry; even if a callback catches
its rejected recursive call, the outer operation fails. Retain the first original
exception and rethrow it on subsequent mutation attempts instead of replacing it with
a generic failure. Endpoints cannot be copied. Policy/header validation happens before
workspace allocation; no allocation follows the declared whole-transfer size.

This layer deliberately does not implement OS I/O, complete body semantics, peer trust,
current-source admission or immutable C/Swift result ownership. Sinks remain provisional
and must discard partial results on error. Allocation bounds cover workspace logical
bytes, excluding root/capacity/function captures, caller-owned sink storage and RSS.

## Verification and evidence

The contract started red on the missing endpoint header/API. A tiny existing session
payload is emitted from3-byte spans, ingested one byte at a time, copied independently
and semantically decoded separately after EOF. Physical length matches the framing
planner. Partial headers/bodies, missing/trailing bytes, null spans, overrun/underrun,
repeated start/finish, decoder errors, foreign identity, original fake sink errors and
caught reentry failures exercise real endpoint behavior. Fixtures remain tiny; no
native archive/parser/generator, OS signal/broken-pipe/cancellation or allocation-
failure/resource-pressure probe is added.

Fresh build and216/216 ordinary CTest contracts pass (36.96s); actual SwiftPM runtime
build1.39s, pinned clang-format18, strict Swift and quarantine guard pass. Six final
source hashes match. Fresh scoped ASan/UBSan passes with protocol archive only, no
engine library, leak detection disabled and existing protocol code uninstrumented.
New implementation152/152 lines,99/99 regions,16/16 functions; inline helper/getters
17/17 lines,15/15 regions,6/6 functions. Combined branch outcomes65/68 (95.59%); some
getter alternatives remain unexercised. This is scoped evidence, not whole-core or
high-resource/real-channel acceptance. The temporary /private/tmp build alias vanished;
only its symlink to the intact persistent shared cache was restored. No second build
folder or stale pre-containment executable was used.

## Remaining work and merge gates

Next implement actual bounded channel adapters, owned request handling and complete
wide session semantics under the native range ledger. Freeze host policy/identity
before byte ingestion; preserve native indexed/source budgets, warnings/text, original
source fallback and optional real statistics. Actual EOF, authenticated peer/control,
current source and complete immutable owners still gate app publication. No automatic
fallback to parent native work or silent limit reduction. Active caps, production
pin48b0ce2d/backend and all quarantines remain unchanged. Apple #279/#280 and M4–M8
remain open. Completed clean exact-head automated review and ALL CI gate merge.


## Formatter compatibility correction

The initial PR head passed pinned18 locally but failed the macOS CI formatting step.
That workflow invokes an unpinned formatter; the newer installed local formatter
reproduced the two empty no-op callback differences. Equivalent explicit void returns
now pass both formatters without changing runtime behavior or CI configuration. The
full216-contract gate and scoped sanitizer evidence above were refreshed for the
corrected tests. Core #274 tracks separately reviewing consistent formatter pinning.

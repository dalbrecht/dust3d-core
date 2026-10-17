# ADR 0009: Source hints from bounded generation sessions

Status: Accepted for implementation; review and release gates still apply.

## Context

The stateless source API in ADR 0008 must not replace the editor's bounded preview
cache. Source hints describe current original document identities, whereas cached
engine geometry contains generated engine identities and weighted associations.
Retaining exported identities from a previous document would risk stale keys.

## Decision

Add `dust3d_generation_session_load_sourced`, returning the same independent
source owner as the stateless API. Extend the existing session implementation with
an optional per-call source capture before dirty-snapshot preprocessing. Keep its
one generator call, invalidation, asset tracking, retained-payload accounting and
cache-only fallback path. Resolve reused node/weight maps against current original
membership; source arrays and identity strings are not stored in retained caches.

Share source-option validation across stateless and cached entry points. Invalid
options fail before changing the session's cache or statistics. A valid sourced
request clears per-call statistics through the ordinary load path. Source payload
or allocation failures are primary failures for that opted-in request: preserve
the cause and discard partial cache state. They are not cache-only warnings.
Cache-only preparation failures still produce valid geometry and source metadata
with separate cache diagnostics. NO_GEOMETRY stays successful emptiness and may
retain bounded cache state.

Use one private templated C-boundary dispatcher for ordinary and sourced session
loads. It retains the existing exception/status mapping and holds the shared engine
mutex exactly once. Do not recursively call public C entry points while locked.
Geometry and source buffers are held by local RAII owners before allocating their
combined output owner, avoiding a raw mesh leak if that final allocation fails.
Returned owners remain independent of subsequent loads, reset and session deletion.

Source logical-payload and retained-cache budgets are separate contracts; neither
is a process memory limit. All quantization/unknown/ambiguous/weighted limitations
from ADR 0008 still apply. No generated ID is presented as an original archive key.
No new dependency, upstream path changes, license change or ABI stability promise.

## Consequences

The editor can later adopt copied source metadata while preserving preview reuse.
That Swift pin/adoption is a separate reviewed Apple change. Existing unsourced
session calls do not acquire source buffers. M3 still needs native adoption and
broader modifier/Qt acceptance; this API alone is not complete editor parity.

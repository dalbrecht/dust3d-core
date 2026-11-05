# ADR 0196: Machine-readable CLI export receipts

## Context

Core #306 adds an automation interface to the existing export command. The
command already owns destination preflight, serial job execution and atomic
replacement. A caller needs to distinguish a failed export from a committed
export whose receipt could not be delivered.

## Decision

Keep the existing command and add an opt-in `--json` flag. Emit a versioned
NDJSON record per attempted job so a caller can consume batch results as each
job finishes. Use the vendored JSON serializer for UTF-8 validation and
escaping. Preserve the original stdout descriptor for receipts and route
generation diagnostics to stderr.

Prepare the success record before staging the output. Deliver it only after
the existing publication path returns from the atomic rename. A receipt write
failure leaves the committed file in place and stops further receipt writes,
while the batch continues exporting. In JSON mode, ignore SIGPIPE so a failed
write can be reported as exit status 6. A failed single export retains its
original exit status even if its failure receipt cannot be delivered.

## Verification

Pure tests exercise escaping, publication order and failed delivery with
injected callbacks. Ordinary Capsule OBJ and FBX runs verify that receipt
mode writes byte-identical output files and a parseable record for each
committed destination.

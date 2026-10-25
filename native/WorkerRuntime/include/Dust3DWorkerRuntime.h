/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_RUNTIME_H_
#define DUST3D_WORKER_RUNTIME_H_
#include <stddef.h>
#include <stdint.h>
#define DUST3D_WORKER_RUNTIME_ABI_VERSION 1
#ifdef __cplusplus
extern "C" {
#define DUST3D_WORKER_RUNTIME_NOEXCEPT noexcept
#else
#define DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif

typedef enum Dust3DWorkerJobStatus {
    DUST3D_WORKER_JOB_COMPLETE = 0,
    DUST3D_WORKER_JOB_PROTOCOL_REJECTED = 1,
    DUST3D_WORKER_JOB_CODEC_REJECTED = 2,
    DUST3D_WORKER_JOB_IO_FAILED = 3,
    DUST3D_WORKER_JOB_LIMIT_EXCEEDED = 4,
    DUST3D_WORKER_JOB_INTERNAL_FAILED = 5
} Dust3DWorkerJobStatus;
typedef enum Dust3DWorkerOperation {
    DUST3D_WORKER_OPERATION_UNKNOWN = 0,
    DUST3D_WORKER_OPERATION_MESH = 1,
    DUST3D_WORKER_OPERATION_UV_MESH = 2
} Dust3DWorkerOperation;
typedef enum Dust3DWorkerDetailSource {
    DUST3D_WORKER_DETAIL_NONE = 0,
    DUST3D_WORKER_DETAIL_FRAME = 1,
    DUST3D_WORKER_DETAIL_UV_REQUEST = 2,
    DUST3D_WORKER_DETAIL_MESH = 3,
    DUST3D_WORKER_DETAIL_UV = 4,
    DUST3D_WORKER_DETAIL_UV_MESH = 5,
    DUST3D_WORKER_DETAIL_FAILURE = 6,
    DUST3D_WORKER_DETAIL_RUNTIME = 7,
    DUST3D_WORKER_DETAIL_SYSTEM = 8,
    DUST3D_WORKER_DETAIL_ADMISSION = 9
} Dust3DWorkerDetailSource;
typedef enum Dust3DWorkerIOStage {
    DUST3D_WORKER_IO_NONE = 0,
    DUST3D_WORKER_IO_INPUT = 1,
    DUST3D_WORKER_IO_OUTPUT = 2
} Dust3DWorkerIOStage;

/** Self-owned result, never a wire struct. COMPLETE means a complete reply was
 * written, including valid native-failure frames; it does not mean mesh success
 * or process exit. No pointer/handle/destruction is needed. Nonzero status may
 * leave partial output: host must discard it and close its channels.
 * detail_code preserves the source-native diagnostic enum number for this pinned
 * revision, not a separately stable numeric schema. system_error preserves errno.
 * message contains up to 511 original diagnostic bytes plus NUL; truncation is
 * explicit. Treat descriptions as diagnostic data, never commands or format strings.
 */
typedef struct Dust3DWorkerJobResult {
    Dust3DWorkerJobStatus status;
    Dust3DWorkerOperation operation;
    uint64_t request_identity;
    int32_t system_error;
    Dust3DWorkerIOStage io_stage;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerJobResult;

/** Return the header/layout version. No I/O, allocation, logging or exception. */
uint32_t dust3d_worker_runtime_abi_version(void) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Serve one bounded frame plus EOF through the maintained base/UV runtime.
 * @param input_descriptor Separate borrowed readable job channel, never closed.
 * @param output_descriptor Borrowed writable reply channel, never closed.
 * @return Independent result with original cause/request/operation. C++ exceptions
 * are caught; signals, memory corruption and OS blocking are not contained here.
 * Worker-host-only: call away from UI, exclusively serialize jobs, establish
 * stdout/stderr isolation, SIGPIPE/resource/sandbox policy before invocation, and
 * close owned channels after return. This function changes none of those global
 * policies. No app adoption, cancellation/deadline or process-exit claim follows.
 * @code
 * Dust3DWorkerJobResult result = dust3d_worker_serve(input_fd, output_fd);
 * if (result.status != DUST3D_WORKER_JOB_COMPLETE) { // discard partial reply
 *     // Preserve result.system_error/detail_source/detail_code/message.
 * }
 * @endcode
 */
Dust3DWorkerJobResult dust3d_worker_serve(int32_t input_descriptor, int32_t output_descriptor) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Parent-owned incremental frame collector; never share it concurrently. */
typedef struct Dust3DWorkerReplyCollector Dust3DWorkerReplyCollector;
typedef struct Dust3DWorkerWireLimits {
    uint64_t maximum_input_bytes;
    uint64_t maximum_output_bytes;
    uint64_t maximum_failure_bytes;
} Dust3DWorkerWireLimits;
typedef enum Dust3DWorkerReplyStatus {
    DUST3D_WORKER_REPLY_READY = 0,
    DUST3D_WORKER_REPLY_FRAMED = 1,
    DUST3D_WORKER_REPLY_PROTOCOL_REJECTED = 2,
    DUST3D_WORKER_REPLY_LIMIT_EXCEEDED = 3,
    DUST3D_WORKER_REPLY_INTERNAL_FAILED = 4
} Dust3DWorkerReplyStatus;
/** Self-owned framing receipt with bounded original diagnostic bytes.
 * READY means an operation succeeded but EOF is not established. FRAMED means
 * exactly one frame plus actual EOF was supplied; neither status implies payload
 * semantics, process exit, authenticated completion or publication admission.
 * Diagnostic fields follow Dust3DWorkerJobResult's source/code/truncation rules.
 */
typedef struct Dust3DWorkerReplyResult {
    Dust3DWorkerReplyStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerReplyResult;

/** Create an independent owner from a stable expected request header (exactly64
 * bytes) and explicit supported wire quotas. Copies identity/policy, reserves only
 * the fixed reply header. No descriptor I/O or native generation occurs.
 * @param expected_header Readable independent wire header, not a native struct.
 * @param header_size Exact readable byte count.
 * @param limits Positive supported limits; logical bytes, not allocator/RSS caps.
 * @param result Required writable self-owned receipt; NULL returns NULL.
 * @return Owner or NULL with original caught diagnostic in result. Destroy once.
 */
Dust3DWorkerReplyCollector* dust3d_worker_reply_create(const uint8_t* expected_header, size_t header_size,
    Dust3DWorkerWireLimits limits, Dust3DWorkerReplyResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Obtain bounded next channel read size, stopping at the fixed header first.
 * @param collector Live exclusively owned collector.
 * @param maximum_chunk Caller read cap, 1..65536 bytes.
 * @param count Required output; initialized to zero on failure.
 * @return READY and size, or caught diagnostic. Rejection of a live owner is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_next_read(Dust3DWorkerReplyCollector* collector,
    size_t maximum_chunk, size_t* count) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy an independent span after header identity/quota validation.
 * @param collector Live exclusively owned collector.
 * @param bytes Stable readable input, independent of all collector-owned bytes;
 * NULL is allowed only for zero size. All borrowed frame views expire on this call.
 * @param size Exact readable count, not an allocation request.
 * @return READY/FRAMED or original caught diagnostic; failure is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_append(Dust3DWorkerReplyCollector* collector,
    const uint8_t* bytes, size_t size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Supply actual channel EOF, never inferred from a callback or expected size.
 * @param collector Live exclusively owned collector. Repeated complete EOF is idempotent.
 * @return FRAMED or caught truncation/state diagnostic; failure is sticky.
 */
Dust3DWorkerReplyResult dust3d_worker_reply_finish(Dust3DWorkerReplyCollector* collector) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow one framed header+body only after successful EOF, otherwise NULL/zero.
 * @param collector Live owner, or NULL.
 * @param size Required output; NULL returns NULL. Initialized to zero on failure.
 * @return Borrowed bytes valid until any modifying call or owner destruction.
 * Swift must copy before destroying the owner; framing alone cannot authorize use.
 * @code
 * size_t size = 0;
 * const uint8_t* bytes = dust3d_worker_reply_bytes(owner, &size);
 * // Copy bytes while owner lives, then apply semantic/completion admission.
 * dust3d_worker_reply_destroy(owner);
 * @endcode
 */
const uint8_t* dust3d_worker_reply_bytes(const Dust3DWorkerReplyCollector* collector, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Destroy one owner after copied-value consumers are done. NULL is harmless. */
void dust3d_worker_reply_destroy(Dust3DWorkerReplyCollector* collector) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Immutable semantic values; independent of the framed owner and input bytes. */
typedef struct Dust3DWorkerDecodedReply Dust3DWorkerDecodedReply;
typedef enum Dust3DWorkerDecodeStatus {
    DUST3D_WORKER_DECODE_OK = 0,
    DUST3D_WORKER_DECODE_PROTOCOL_REJECTED = 1,
    DUST3D_WORKER_DECODE_CODEC_REJECTED = 2,
    DUST3D_WORKER_DECODE_LIMIT_EXCEEDED = 3,
    DUST3D_WORKER_DECODE_INTERNAL_FAILED = 4,
    DUST3D_WORKER_DECODE_ADMISSION_REJECTED = 5
} Dust3DWorkerDecodeStatus;
typedef struct Dust3DWorkerDecodeResult {
    Dust3DWorkerDecodeStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerDecodeResult;
typedef enum Dust3DWorkerDecodedKind {
    DUST3D_WORKER_DECODED_NONE = 0,
    DUST3D_WORKER_DECODED_MESH = 1,
    DUST3D_WORKER_DECODED_UV_MESH = 2,
    DUST3D_WORKER_DECODED_FAILURE = 3
} Dust3DWorkerDecodedKind;
/** Scalar summary. Zero for NULL. failure_status is a known wire status:
 * 1 invalid document, 2 unsupported asset, 3 generation failed, 4 limit exceeded,
 * 5 no geometry, 6 cancelled. Zero means this is not a native failure.
 */
typedef struct Dust3DWorkerDecodedSummary {
    Dust3DWorkerDecodedKind kind;
    size_t vertex_count;
    uint64_t node_count, part_count;
    size_t triangle_count, chart_count, gradient_count, component_count;
    double packed_texture_size;
    uint32_t failure_status;
} Dust3DWorkerDecodedSummary;
/** Value copies; no native C++ layout is exposed. */
typedef struct Dust3DWorkerVertex {
    float position[3], normal[3], color[4];
} Dust3DWorkerVertex;
/** Association 0 unknown, 1 component, 2 ambiguous; UINT64_MAX is no component.
 * known mapping and source association are independent. No rendered pixels.
 */
typedef struct Dust3DWorkerUvTriangle {
    double uv[3][2];
    uint8_t known, association;
    uint64_t component_index;
} Dust3DWorkerUvTriangle;
typedef struct Dust3DWorkerUvChart {
    double left, top, width, height, color[4];
    uint8_t flipped, association;
    uint64_t component_index;
    char image_id[39];
} Dust3DWorkerUvChart;
typedef struct Dust3DWorkerUvGradient {
    uint64_t chart_index, resolution;
    double large_color[4], small_color[4];
} Dust3DWorkerUvGradient;

/** Consume one EOF-framed owner into independent semantic values, at most once.
 * @param collector Live exclusively owned owner; retired for modification before
 * validation/allocation. Rejected semantic completion cannot be replayed.
 * @param maximum_decoded_bytes Positive aggregate logical retained-byte budget,
 * at most 128MiB. Standard per-field/count/text quotas also apply; no RSS claim.
 * @param result Required writable original typed receipt; NULL returns NULL
 * without consuming. OK includes a semantically valid native failure.
 * @return Independent owner or NULL. Destroy both owners separately. No body
 * wire copy is made; the shared codecs validate before retained arrays/text.
 * No authenticated transaction, process-exit or publication admission follows.
 */
Dust3DWorkerDecodedReply* dust3d_worker_reply_decode(Dust3DWorkerReplyCollector* collector,
    uint64_t maximum_decoded_bytes, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Return scalar metadata, or all zeros for NULL. No I/O/allocation. */
Dust3DWorkerDecodedSummary dust3d_worker_decoded_summary(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy a validated geometry range into independent caller storage.
 * @param reply Live immutable semantic owner.
 * @param first First vertex index.
 * @param count Number of output slots, each readable/writable by the caller.
 * @param output Independent writable span; NULL allowed only for zero count.
 * @return 1 on valid range; 0 without writes for wrong kind/NULL/invalid range.
 */
int dust3d_worker_decoded_vertices(const Dust3DWorkerDecodedReply* reply, size_t first,
    size_t count, Dust3DWorkerVertex* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy one UV triangle/chart/gradient. Return 1 on valid kind/index/output;
 * otherwise 0, clearing a non-NULL output. No pointers are retained.
 */
int dust3d_worker_decoded_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvTriangle* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
int dust3d_worker_decoded_chart(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvChart* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
int dust3d_worker_decoded_gradient(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerUvGradient* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow validated UTF-8 warning/component bytes, or raw native failure cause.
 * Valid until reply destruction. Required size is zeroed on invalid kind/index;
 * NULL size returns NULL. Empty valid values have size0 and may return NULL.
 * Callers copy exact sizes, including embedded NUL in opaque failure bytes.
 * These functions do not log, interpret or sanitize diagnostics.
 */
const char* dust3d_worker_decoded_warning(const Dust3DWorkerDecodedReply* reply, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
const char* dust3d_worker_decoded_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
const uint8_t* dust3d_worker_decoded_failure_cause(const Dust3DWorkerDecodedReply* reply, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Destroy exactly once; NULL is harmless. Copy all borrowed bytes first.
 * Example: read summary, copy vertices/text, then destroy decoded and framed owners.
 */
void dust3d_worker_decoded_destroy(Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Parent-issued identity, copied by value; document and request must be nonzero. */
typedef struct Dust3DWorkerRequestIdentity {
    uint8_t document[16];
    uint64_t request, revision;
} Dust3DWorkerRequestIdentity;
/** Resolved image dimensions only, never image pixels. Independent borrowed text. */
typedef struct Dust3DWorkerRequestImage {
    const char* identity;
    size_t identity_size;
    uint32_t width, height;
} Dust3DWorkerRequestImage;
/** Zero texture/budget retain native defaults. Images require nonzero UUIDs,
 * original spelling preserved, dimensions1..8192 and no semantic duplicate aliases.
 */
typedef struct Dust3DWorkerRequestUvOptions {
    uint32_t texture_size;
    uint64_t metadata_budget;
    const Dust3DWorkerRequestImage* images;
    size_t image_count;
} Dust3DWorkerRequestUvOptions;
typedef enum Dust3DWorkerRequestStatus {
    DUST3D_WORKER_REQUEST_OK = 0,
    DUST3D_WORKER_REQUEST_PROTOCOL_REJECTED = 1,
    DUST3D_WORKER_REQUEST_UV_REJECTED = 2,
    DUST3D_WORKER_REQUEST_LIMIT_EXCEEDED = 3,
    DUST3D_WORKER_REQUEST_INTERNAL_FAILED = 4
} Dust3DWorkerRequestStatus;
/** Self-owned original category/code/diagnostic; same bounded-byte rules as job results. */
typedef struct Dust3DWorkerRequestResult {
    Dust3DWorkerRequestStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    char message[512];
} Dust3DWorkerRequestResult;
typedef struct Dust3DWorkerRequest Dust3DWorkerRequest;
/** Encode one independent complete request through shared codecs; no native generation.
 * @param operation Base or UV mesh, no unknown enum values.
 * @param identity Parent-issued copied correlation values.
 * @param archive Stable readable independent opaque archive, non-NULL/nonempty.
 * @param archive_size Exact readable byte count, bounded before retained body copy.
 * @param uv NULL for base mesh. UV accepts NULL/defaults or stable options/image spans.
 * @param limits Positive supported wire quotas; logical sizes, not allocator/RSS caps.
 * @param result Required writable receipt; NULL returns NULL without allocation.
 * @return Immutable owner or NULL with original caught cause. No archive parsing,
 * signing, transport, process/transaction/publication admission or logging occurs.
 * Caller retains input spans through the call and destroys output exactly once.
 */
Dust3DWorkerRequest* dust3d_worker_request_create(Dust3DWorkerOperation operation,
    Dust3DWorkerRequestIdentity identity, const uint8_t* archive, size_t archive_size,
    const Dust3DWorkerRequestUvOptions* uv, Dust3DWorkerWireLimits limits,
    Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow full frame or exact expected request header until owner destruction.
 * Required size is zeroed on NULL owner; NULL size returns NULL. Swift copies
 * both spans before destruction. No descriptor I/O or allocation occurs.
 * Example: copy request_bytes for writing, request_header for parent collection,
 * then destroy the request owner. Input archive/options can already be released.
 */
const uint8_t* dust3d_worker_request_bytes(const Dust3DWorkerRequest* request, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
const uint8_t* dust3d_worker_request_header(const Dust3DWorkerRequest* request, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Destroy exactly one owner; NULL is harmless. */
void dust3d_worker_request_destroy(Dust3DWorkerRequest* request) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Caller observations from its code-signing-enforced transport, not receipt data. */
typedef struct Dust3DWorkerServiceObservation {
    uint64_t connection_generation;
    int64_t process_identifier;
} Dust3DWorkerServiceObservation;
/** Scalar service control receipt; preserves original numeric diagnostic context.
 * Zero runtime/admission status means frame-written/accepted, not geometry success.
 * Operation is a raw integer so unknown wire values can be rejected before enum conversion.
 * These fields do not authenticate a connection or establish actual stream EOF.
 */
typedef struct Dust3DWorkerServiceReceipt {
    uint32_t runtime_status, admission_status;
    uint32_t operation;
    uint64_t request_identity;
    int64_t process_identifier;
    int32_t system_error;
    uint32_t io_stage, detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    int32_t finalization_error;
} Dust3DWorkerServiceReceipt;
/** Create a service-specific collector before submitting work.
 * @param expected_header Exact original request header, stable through this call.
 * @param header_size Exact readable header byte count.
 * @param connection_generation Nonzero trusted lifetime chosen before submission.
 * @param limits Positive supported framing quotas; not RSS limits.
 * @param result Required original framing receipt; NULL returns NULL without allocation.
 * @return Exclusive collector with copied header/generation, or NULL with caught cause.
 * Feed actual read bytes and call reply_finish only after actual EOF. Service
 * collectors require service_admit; ordinary reply_decode rejects them.
 */
Dust3DWorkerReplyCollector* dust3d_worker_service_reply_create(const uint8_t* expected_header,
    size_t header_size, uint64_t connection_generation, Dust3DWorkerWireLimits limits,
    Dust3DWorkerReplyResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Consume one service collector into independent owned semantic values, once.
 * @param collector Live service-specific EOF-framed owner, consumed before validation.
 * @param active Authoritative current document/request/revision, not reply-derived.
 * @param observed Actual authenticated-connection lifetime/OS peer observations.
 * @param receipt Scalar callback from that connection; not trusted peer evidence.
 * @param maximum_decoded_bytes Positive retained logical quota, at most128MiB.
 * @param result Required original typed receipt; NULL leaves collector unconsumed.
 * @return Independent decoded owner or NULL; all C++ exceptions are caught.
 * ADMISSION_REJECTED preserves admission category/code/cause. Failure payloads
 * remain owned typed results. Rejection retires the collector; never replay it.
 * This function cannot authenticate caller inputs or publish app state. It makes
 * a bounded temporary frame copy; quotas are not process-memory bounds.
 * Example: service_admit(owner, active, observed, receipt, budget, &result), copy
 * values, then destroy decoded and collector owners independently.
 */
Dust3DWorkerDecodedReply* dust3d_worker_service_admit(Dust3DWorkerReplyCollector* collector,
    Dust3DWorkerRequestIdentity active, Dust3DWorkerServiceObservation observed,
    Dust3DWorkerServiceReceipt receipt, uint64_t maximum_decoded_bytes,
    Dust3DWorkerDecodeResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif

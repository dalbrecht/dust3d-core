/* SPDX-License-Identifier: MIT */
#ifndef DUST3D_WORKER_RUNTIME_H_
#define DUST3D_WORKER_RUNTIME_H_
#include "Dust3DWorkerImageDecoder.h"
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
    DUST3D_WORKER_OPERATION_UV_MESH = 2,
    DUST3D_WORKER_OPERATION_SOURCED_MESH = 3,
    DUST3D_WORKER_OPERATION_INDEXED_SOURCED_MESH = 4,
    DUST3D_WORKER_OPERATION_INDEXED_MESH = 5,
    DUST3D_WORKER_OPERATION_IMPORTED_RENDER = 6,
    DUST3D_WORKER_OPERATION_LEGACY_BAKED = 7,
    DUST3D_WORKER_OPERATION_SOURCE_BAKED = 8,
    DUST3D_WORKER_OPERATION_SESSION_INDEXED_SOURCED_MESH = 9
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
    DUST3D_WORKER_DETAIL_ADMISSION = 9,
    DUST3D_WORKER_DETAIL_QUEUE = 10,
    DUST3D_WORKER_DETAIL_SOURCE_REQUEST = 11,
    DUST3D_WORKER_DETAIL_SOURCE_HINTS = 12,
    DUST3D_WORKER_DETAIL_SOURCED_MESH = 13,
    DUST3D_WORKER_DETAIL_INDEXED_MESH = 14,
    DUST3D_WORKER_DETAIL_INDEXED_SOURCES = 15,
    DUST3D_WORKER_DETAIL_IMAGE_DECODER = 16,
    DUST3D_WORKER_DETAIL_IMPORTED_PRODUCER = 17,
    DUST3D_WORKER_DETAIL_IMPORTED_SURFACE = 18,
    DUST3D_WORKER_DETAIL_IMPORTED_MATERIALS = 19,
    DUST3D_WORKER_DETAIL_IMPORTED_RENDER = 20,
    DUST3D_WORKER_DETAIL_IMPORTED_REQUEST = 21,
    DUST3D_WORKER_DETAIL_LEGACY_REQUEST = 22,
    DUST3D_WORKER_DETAIL_LEGACY_PRODUCER = 23,
    DUST3D_WORKER_DETAIL_LEGACY_BAKED = 24,
    DUST3D_WORKER_DETAIL_PROCEDURAL_IMAGES = 25,
    DUST3D_WORKER_DETAIL_BAKED_TEXTURES = 26,
    DUST3D_WORKER_DETAIL_SOURCE_BAKED = 27,
    DUST3D_WORKER_DETAIL_SOURCE_BAKE_REQUEST = 28,
    DUST3D_WORKER_DETAIL_SESSION_OUTPUT = 29,
    DUST3D_WORKER_DETAIL_GENERATION_STATISTICS = 30
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

/** Serve one bounded frame plus EOF through the maintained expanded/indexed/UV/source runtime.
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
/** Serve with an explicit callback copied for this exclusive synchronous worker job.
 * Decoder configuration/context/code must remain valid through return; null rejects
 * before I/O. Same channel/resource/isolation obligations as dust3d_worker_serve.
 * Original callback failures travel in detailed failure frames, not generic success.
 * No platform decoder is installed globally and no parent fallback is selected.
 * Example: result = dust3d_worker_serve_with_image_decoder(input_fd, output_fd, &decoder); */
Dust3DWorkerJobResult dust3d_worker_serve_with_image_decoder(int32_t input_descriptor, int32_t output_descriptor, const Dust3DWorkerImageDecoder* decoder) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Separate platform decoder roles. An all-zero slot means unavailable; a present
 * slot must have the exact existing decoder layout and callback. Procedural callbacks
 * decode PNG with the legacy Qt grayscale transparency rule; imported callbacks
 * preserve authored transparent RGB. Context/code stay live through exclusive use.
 * This configuration is an in-process value, never wire data or a stable ABI claim. */
typedef struct Dust3DWorkerDecoders {
    size_t struct_size;
    Dust3DWorkerImageDecoder imported;
    Dust3DWorkerImageDecoder procedural;
} Dust3DWorkerDecoders;
/** Serve one job with distinct platform purposes. Required configuration and both
 * present slots validate before I/O. An unavailable required provider becomes an
 * explicit correlated failure frame, never a parent fallback. Same borrowed channel,
 * host resource/isolation and original diagnostic obligations as worker_serve.
 * Example: result = dust3d_worker_serve_with_decoders(input_fd, output_fd, &decoders). */
Dust3DWorkerJobResult dust3d_worker_serve_with_decoders(int32_t input_descriptor, int32_t output_descriptor,
    const Dust3DWorkerDecoders* decoders) DUST3D_WORKER_RUNTIME_NOEXCEPT;

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
    DUST3D_WORKER_DECODE_ADMISSION_REJECTED = 5,
    DUST3D_WORKER_DECODE_QUEUE_REJECTED = 6
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
    DUST3D_WORKER_DECODED_FAILURE = 3,
    DUST3D_WORKER_DECODED_SOURCED_MESH = 4,
    DUST3D_WORKER_DECODED_INDEXED_MESH = 5,
    DUST3D_WORKER_DECODED_INDEXED_SOURCED_MESH = 6,
    DUST3D_WORKER_DECODED_IMPORTED_RENDER = 7,
    DUST3D_WORKER_DECODED_LEGACY_BAKED = 8,
    DUST3D_WORKER_DECODED_SOURCE_BAKED = 9
} Dust3DWorkerDecodedKind;
/** Scalar summary. Zero for NULL. vertex_count is unique-table count for indexed
 * kinds, expanded count otherwise; triangle_count is derived from indexed corners
 * or the UV triangle table. Index count is available separately. failure_status:
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
/** Read immutable triangle-index count for indexed kinds; zero for NULL/wrong kind. */
size_t dust3d_worker_decoded_index_count(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy an indexed range in original triangle-corner order, without allocation.
 * @param reply Immutable decoded indexed or indexed/source owner, retained until return.
 * @param first First index ordinal, at most index_count.
 * @param count Count at most index_count-first; checked before addition or writes.
 * @param output Independent writable UInt32 storage; NULL allowed for a valid empty range.
 * @return 1 on success, 0 for NULL/wrong kind/bad range/storage, without writing on rejection.
 * Example: dust3d_worker_decoded_indices(owner, 0, count, copied_indices).
 */
int dust3d_worker_decoded_indices(const Dust3DWorkerDecodedReply* reply, size_t first, size_t count, uint32_t* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Source counts; zero for NULL or a different decoded kind. */
typedef struct Dust3DWorkerSourceSummary {
    size_t identity_count, corner_count, influence_count;
} Dust3DWorkerSourceSummary;
/** Numeric hint kinds are opaque: 0 unknown, 1 node, 2 weighted, 3 ambiguous.
 * Source hints are quantized associations, not authenticated surface ownership.
 */
typedef struct Dust3DWorkerSourceCorner {
    uint32_t kind;
    uint64_t first_influence, influence_count;
} Dust3DWorkerSourceCorner;
/** UINT64_MAX means no identity. Finite nonnegative weights are not normalized. */
typedef struct Dust3DWorkerSourceInfluence {
    uint32_t kind;
    uint64_t identity_index;
    float weight;
} Dust3DWorkerSourceInfluence;
/** Read immutable owned source counts without allocation. */
Dust3DWorkerSourceSummary dust3d_worker_decoded_source_summary(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy one validated record; return 1 on success. Required output is zeroed
 * before rejecting a NULL/wrong-kind owner or invalid index. No allocation.
 * Example: dust3d_worker_decoded_source_corner(owner, index, &corner).
 */
int dust3d_worker_decoded_source_corner(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerSourceCorner* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
int dust3d_worker_decoded_source_influence(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerSourceInfluence* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow original UTF8 bytes until decoded-owner destruction, no allocation.
 * Required size is initialized to zero; NULL/wrong-kind owner or invalid index
 * returns NULL. Copy before destroying owner; do not free borrowed strings.
 * Example: const char* node = dust3d_worker_decoded_source_node(owner, i, &size).
 */
const char* dust3d_worker_decoded_source_node(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
const char* dust3d_worker_decoded_source_part(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
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
/** Actual baked storage counts and producer sampling cap; these are not native
 * cache/generation metrics. Zero for NULL/wrong kind. Legacy output has base maps
 * only; absent map roles have zero levels. Existing geometry/UV getters also accept
 * the distinct LEGACY_BAKED kind and preserve charts/gradients/component keys. */
typedef struct Dust3DWorkerBakedSummary {
    size_t tangent_count;
    uint64_t pixel_bytes;
    double maximum_sampling_lod;
} Dust3DWorkerBakedSummary;
typedef struct Dust3DWorkerTextureTangent {
    float direction[3], handedness;
    uint8_t known;
} Dust3DWorkerTextureTangent;
/** Tight straight RGBA8 borrowed from the immutable decoded owner until destruction.
 * Copy byte_count pixels before releasing the owner. No platform decoding occurs. */
typedef struct Dust3DWorkerTextureView {
    uint32_t width, height;
    uint64_t row_stride;
    const uint8_t* pixels;
    size_t byte_count;
} Dust3DWorkerTextureView;
/** Return actual baked counts/cap or zero for NULL/wrong kind; no allocation.
 * Example: summary = dust3d_worker_decoded_baked_summary(owner). */
Dust3DWorkerBakedSummary dust3d_worker_decoded_baked_summary(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy one aligned tangent. Required output clears on rejection; returns1/0.
 * Example: ok = dust3d_worker_decoded_baked_tangent(owner, index, &tangent). */
int dust3d_worker_decoded_baked_tangent(const Dust3DWorkerDecodedReply* reply, size_t index,
    Dust3DWorkerTextureTangent* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Map roles 0 color,1 normal,2 metallic,3 roughness,4 emissive. Unknown/wrong kind
 * returns0. No allocations. Example: count = dust3d_worker_decoded_baked_level_count(owner, 0). */
size_t dust3d_worker_decoded_baked_level_count(const Dust3DWorkerDecodedReply* reply, uint32_t role) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow one existing map level. Invalid kind/role/level/output returns0 and clears
 * a nonnull output. Example: ok = dust3d_worker_decoded_baked_texture(owner, 0, 0, &view). */
int dust3d_worker_decoded_baked_texture(const Dust3DWorkerDecodedReply* reply, uint32_t role,
    size_t level, Dust3DWorkerTextureView* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Source-baked catalogs and packed surface policy, distinct from legacy charts.
 * Zero counts for NULL/wrong kind; model/component catalogs preserve exact order. */
typedef struct Dust3DWorkerBakedSurfaceSummary {
    size_t triangle_count, model_count, component_count;
} Dust3DWorkerBakedSurfaceSummary;
/** Independent scalar copy. UINT64_MAX indicates unknown catalog association.
 * Unknown mapping/policy remains explicit; no Boolean provenance is inferred. */
typedef struct Dust3DWorkerBakedTriangle {
    double uv[3][2], alpha_cutoff;
    uint64_t model_index, component_index;
    uint32_t alpha_mode;
    uint8_t uv_known, policy_known, is_double_sided;
} Dust3DWorkerBakedTriangle;
/** Return source surface counts, zero for NULL/wrong kind.
 * Example: shape = dust3d_worker_decoded_baked_surface_summary(owner). */
Dust3DWorkerBakedSurfaceSummary dust3d_worker_decoded_baked_surface_summary(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy one source triangle, returning1/0. Nonnull output clears on rejection.
 * Example: ok = dust3d_worker_decoded_baked_triangle(owner, index, &triangle). */
int dust3d_worker_decoded_baked_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerBakedTriangle* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow exact UTF8 catalog bytes until owner destruction; copy before release.
 * Required size clears on invalid kind/index, NULL size returns NULL.
 * Example: bytes = dust3d_worker_decoded_baked_model(owner, index, &size). */
const char* dust3d_worker_decoded_baked_model(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Same borrowing contract as baked_model, for the independent component catalog.
 * Example: bytes = dust3d_worker_decoded_baked_component(owner, index, &size). */
const char* dust3d_worker_decoded_baked_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Membership1 original/2 generated; zero for invalid. This does not prove provenance.
 * Example: kind = dust3d_worker_decoded_baked_component_kind(owner, index). */
uint32_t dust3d_worker_decoded_baked_component_kind(const Dust3DWorkerDecodedReply* reply, size_t index) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Imported values retain their own distinct kind even for successful absence.
 * Counts are zero for absent/NULL/wrong kind. decoded_pixel_bytes counts actual RGBA
 * storage only, not allocator/native payload metrics. */
typedef struct Dust3DWorkerImportedSummary {
    uint8_t has_value;
    size_t model_count, component_count, triangle_count;
    uint64_t decoded_pixel_bytes;
} Dust3DWorkerImportedSummary;
typedef struct Dust3DWorkerImportedTriangle {
    double uv[3][2], colors[3][4];
    uint64_t model_index, component_index;
    uint32_t color_encoding;
    uint8_t uv_known;
} Dust3DWorkerImportedTriangle;
/** Independent scalar copy with owner-scoped immutable borrowed spans. Model is
 * exact UTF8 bytes. Encoded bytes are opaque PNG1/JPEG2, never decoded here. */
typedef struct Dust3DWorkerImportedMaterial {
    const char* model_id;
    size_t model_id_size;
    double base_color[4], metallic, roughness, emissive[3], alpha_cutoff;
    uint32_t alpha_mode;
    uint8_t is_double_sided, has_sampler, has_magnification_filter, has_minification_filter;
    uint32_t magnification_filter, minification_filter, wrap_s, wrap_t;
    uint8_t has_image;
    uint32_t image_encoding;
    const uint8_t* encoded;
    size_t encoded_count;
} Dust3DWorkerImportedMaterial;
typedef struct Dust3DWorkerImportedImage {
    uint32_t width, height;
    uint64_t row_stride;
    const uint8_t* pixels;
    size_t byte_count;
} Dust3DWorkerImportedImage;
/** Detailed v2 failure metadata; zero when absent/NULL/wrong kind. Existing failure
 * status/cause accessors remain valid for both v1 and v2. Domain1 native,2 image
 * decoder,3 producer,4 surface,5 material,6 render,7 standard; status/code preserve
 * source-native values and cause_truncated is explicit. Additional domains:8 legacy bake,
 * 9 procedural inputs,10 baked textures,11 UV,12 legacy producer,13 mesh,14 source bake. */
typedef struct Dust3DWorkerFailureDetail {
    uint8_t has_detail;
    uint16_t domain;
    int32_t code;
    uint32_t status;
    uint8_t cause_truncated;
} Dust3DWorkerFailureDetail;
Dust3DWorkerImportedSummary dust3d_worker_decoded_imported_summary(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Optional contextual v3 failure resource. Role1 procedural PNG,2 imported image.
 * Identity borrows exact UTF8 bytes until decoded owner destruction; copy before
 * release. All fields zero for absent/NULL/wrong kind. Original cause is separate. */
typedef struct Dust3DWorkerFailureContext {
    uint16_t role;
    const char* identity;
    size_t identity_size;
} Dust3DWorkerFailureContext;
/** Borrow optional resource context; no allocation.
 * Example: context = dust3d_worker_decoded_failure_context(owner). */
Dust3DWorkerFailureContext dust3d_worker_decoded_failure_context(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
Dust3DWorkerFailureDetail dust3d_worker_decoded_failure_detail(const Dust3DWorkerDecodedReply* reply) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow exact validated catalog bytes until reply destruction. Required size is
 * reset to zero on rejection; null size returns null. Component kinds1 original/2
 * generated are membership only, not Boolean provenance. Copy before owner release. */
const char* dust3d_worker_decoded_imported_model(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
const char* dust3d_worker_decoded_imported_component(const Dust3DWorkerDecodedReply* reply, size_t index, size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
uint32_t dust3d_worker_decoded_imported_component_kind(const Dust3DWorkerDecodedReply* reply, size_t index) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy one validated record; return1 on valid owner/index/output, otherwise0 and
 * clear nonnull output. UINT64_MAX denotes unknown model/component identity.
 * Authored UVs may repeat outside0..1; color encoding0 sRGB/1 linear RGB. */
int dust3d_worker_decoded_imported_triangle(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedTriangle* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrowed material/image spans remain valid until reply destruction. Return1 for
 * an existing record even when image is absent (all image fields zero), otherwise0
 * and clear nonnull output. Swift copies all spans before releasing the owner. */
int dust3d_worker_decoded_imported_material(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedMaterial* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
int dust3d_worker_decoded_imported_image(const Dust3DWorkerDecodedReply* reply, size_t index, Dust3DWorkerImportedImage* output) DUST3D_WORKER_RUNTIME_NOEXCEPT;
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
    DUST3D_WORKER_REQUEST_INTERNAL_FAILED = 4,
    DUST3D_WORKER_REQUEST_SOURCE_REJECTED = 5,
    DUST3D_WORKER_REQUEST_IMPORTED_REJECTED = 6,
    DUST3D_WORKER_REQUEST_LEGACY_REJECTED = 7,
    DUST3D_WORKER_REQUEST_SOURCE_BAKE_REJECTED = 8
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
/** Encode one independent base/ordinary-indexed/UV/imported/legacy request; no native generation.
 * Source variants use their dedicated source_request_create functions instead.
 * @param operation Base/indexed/UV/imported/legacy, no unknown enum values.
 * @param identity Parent-issued copied correlation values.
 * @param archive Stable readable independent opaque archive, non-NULL/nonempty.
 * @param archive_size Exact readable byte count, bounded before retained body copy.
 * @param uv NULL for ordinary/imported/legacy requests. UV accepts NULL/defaults or stable options/image spans.
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
/** Per-request imported producer budgets. Zero selects maintained defaults; positive
 * values <=256MiB can only tighten receiver policy. struct_size must match exactly.
 * Input/output logical quotas exclude root/capacity/native/decoder workspace and RSS. */
typedef struct Dust3DWorkerRequestImportedOptions {
    size_t struct_size;
    uint64_t maximum_material_native_bytes, maximum_surface_native_bytes;
    uint64_t maximum_facts_copy_bytes, maximum_surface_copy_bytes;
    uint64_t maximum_encoded_image_bytes, maximum_decoded_pixel_bytes;
    uint64_t maximum_aggregate_decoded_bytes, maximum_output_wire_bytes;
} Dust3DWorkerRequestImportedOptions;
/** Named legacy byte budgets use zero maintained defaults and can only tighten
 * authoritative receiver byte policy. Dimensions/grid are explicit native settings;
 * zero uses native defaults subject to separately enforced host work caps. */
typedef struct Dust3DWorkerRequestLegacyOptions {
    size_t struct_size;
    uint64_t maximum_uv_native_bytes, maximum_bake_native_bytes;
    uint64_t maximum_encoded_image_bytes, maximum_encoded_total_bytes;
    uint64_t maximum_input_copy_bytes, maximum_decoded_pixel_bytes;
    uint64_t maximum_output_copy_bytes, maximum_output_wire_bytes;
    uint64_t atlas_texture_size, color_size, normal_size, material_size;
    uint64_t maximum_spatial_grid_entries, maximum_asset_name_bytes;
} Dust3DWorkerRequestLegacyOptions;
/** Explicit source-bake policies; both struct sizes must match. Zero byte policies
 * select maintained defaults; work settings independently honor receiver host caps. */
typedef struct Dust3DWorkerRequestSourceBakeOptions {
    size_t struct_size;
    Dust3DWorkerRequestLegacyOptions legacy;
    uint64_t maximum_material_native_bytes, maximum_sampler_native_bytes;
    uint64_t maximum_atlas_native_bytes, maximum_output_native_bytes;
    uint64_t maximum_chart_count, maximum_raster_samples;
    uint64_t should_generate_mips, maximum_levels;
    uint64_t maximum_output_copy_bytes, maximum_output_wire_bytes;
} Dust3DWorkerRequestSourceBakeOptions;
/** Own a complete source-bake request without native parsing. NULL options select
 * defaults. Required result preserves caught category/code/cause; NULL result does
 * no work. All framing/options validate before output allocation. Wire output policy
 * also tightens to limits. Destroy once; no C++ exception escapes.
 * Example: owner = dust3d_worker_source_bake_request_create(id, bytes, size, &options, limits, &result). */
Dust3DWorkerRequest* dust3d_worker_source_bake_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t archive_size, const Dust3DWorkerRequestSourceBakeOptions* options,
    Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Encode a complete independent legacy request without archive/native/image parsing.
 * Options may be null for defaults; otherwise struct_size must match exactly. All
 * byte policies/settings/framing validate before output allocation. Output wire also
 * tightens to limits.maximum_output_bytes. Required result preserves caught original
 * category/code/cause; null result returns null without work. Destroy returned owner
 * exactly once; borrow spans only while it lives. No C++ exception escapes.
 * Example: owner = dust3d_worker_legacy_request_create(id, data, size, &options, limits, &result). */
Dust3DWorkerRequest* dust3d_worker_legacy_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t archive_size, const Dust3DWorkerRequestLegacyOptions* options,
    Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;

/** Encode an owned imported request without native parsing. Options may be null for
 * defaults. Output wire budget also tightens to limits.maximum_output_bytes. All
 * framing/policies validate before retained output allocation. C++ exceptions are
 * caught in the required result. Archive is borrowed only for the call.
 * Example: owner = dust3d_worker_imported_request_create(id, bytes, size, &options, limits, &result); */
Dust3DWorkerRequest* dust3d_worker_imported_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t archive_size, const Dust3DWorkerRequestImportedOptions* options,
    Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Create one independent source-aware request, without parsing or generating archive data.
 * @param identity Nonzero document/request identity; revision zero is valid.
 * @param archive Stable nonempty readable archive span, borrowed only during this call.
 * @param archive_size Exact size; request prefix also counts against input wire quota.
 * @param maximum_source_bytes Zero selects native 64MiB default; at most 256MiB.
 * This can only tighten the worker's configured source-copy/native retention policy.
 * @param limits Positive supported wire quotas. Logical quotas do not bound process RSS.
 * @param result Required original typed receipt; NULL returns NULL without work.
 * @return Independent request owner or NULL; destroy with request_destroy. No exception escapes.
 * Example: owner = dust3d_worker_source_request_create(id, bytes, size, 0, limits, &result).
 */
Dust3DWorkerRequest* dust3d_worker_source_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t archive_size, uint64_t maximum_source_bytes,
    Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Indexed/source equivalent of source_request_create, operation 4, same ownership/quotas.
 * Creates one complete independent frame without archive parsing, generation or I/O.
 * Required result preserves original typed diagnostics; NULL returns NULL without work.
 * Example: owner = dust3d_worker_indexed_source_request_create(id, bytes, size, 0, limits, &result).
 */
Dust3DWorkerRequest* dust3d_worker_indexed_source_request_create(Dust3DWorkerRequestIdentity identity,
    const uint8_t* archive, size_t archive_size, uint64_t maximum_source_bytes,
    Dust3DWorkerWireLimits limits, Dust3DWorkerRequestResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
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

/** Externally serialized service coordinator: one active transport and latest waiting request.
 * Request IDs must increase across queue and connection lifetimes. No I/O is started.
 * Logical quotas do not bound process RSS. This owner cannot authenticate caller evidence.
 */
typedef struct Dust3DWorkerServiceQueue Dust3DWorkerServiceQueue;
typedef enum Dust3DWorkerQueueStatus {
    DUST3D_WORKER_QUEUE_READY = 0,
    DUST3D_WORKER_QUEUE_BEGUN = 1,
    DUST3D_WORKER_QUEUE_IDLE = 2,
    DUST3D_WORKER_QUEUE_BUSY = 3,
    DUST3D_WORKER_QUEUE_PROTOCOL_REJECTED = 4,
    DUST3D_WORKER_QUEUE_REJECTED = 5,
    DUST3D_WORKER_QUEUE_LIMIT_EXCEEDED = 6,
    DUST3D_WORKER_QUEUE_INTERNAL_FAILED = 7
} Dust3DWorkerQueueStatus;
/** Independent numeric receipt and bounded original cause; no borrowed strings. */
typedef struct Dust3DWorkerQueueResult {
    Dust3DWorkerQueueStatus status;
    Dust3DWorkerDetailSource detail_source;
    int32_t detail_code;
    uint8_t message_truncated;
    uint8_t has_replaced;
    Dust3DWorkerRequestIdentity replaced_identity;
    Dust3DWorkerRequestIdentity active_identity;
    char message[512];
} Dust3DWorkerQueueResult;
/** Create with positive supported quotas; required result, NULL on original typed failure.
 * Example: owner = dust3d_worker_service_queue_create(limits, &result).
 */
Dust3DWorkerServiceQueue* dust3d_worker_service_queue_create(Dust3DWorkerWireLimits limits,
    Dust3DWorkerQueueResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Copy and validate an exact request frame before replacing latest waiting work.
 * @return READY with optional replaced identity, or original typed rejection; active bytes survive.
 */
Dust3DWorkerQueueResult dust3d_worker_service_queue_submit(Dust3DWorkerServiceQueue* queue,
    const uint8_t* bytes, size_t size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Capture trusted nonzero connection generation before transport submission.
 * @return BEGUN + captured identity, BUSY, IDLE or typed failure. Zero preserves waiting.
 * Allocation failure after taking waiting work releases that unsubmitted slot; retry with a fresh ID.
 */
Dust3DWorkerQueueResult dust3d_worker_service_queue_begin(Dust3DWorkerServiceQueue* queue,
    uint64_t connection_generation) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow active outbound bytes; required size, zero/NULL if absent.
 * Survives submit/cancel/close, expires on admit/abandon/destroy; never free or modify.
 */
const uint8_t* dust3d_worker_service_queue_request_bytes(const Dust3DWorkerServiceQueue* queue,
    size_t* size) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Borrow active framing collector. Use framing functions only; NEVER destroy or decode standalone.
 * Expires on admit/abandon/destroy. Finish only after observed actual transport EOF.
 */
Dust3DWorkerReplyCollector* dust3d_worker_service_queue_reply(Dust3DWorkerServiceQueue* queue) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Retire matching publication without stopping transport; NULL harmless. Active transport must drain. */
void dust3d_worker_service_queue_cancel(Dust3DWorkerServiceQueue* queue,
    Dust3DWorkerRequestIdentity identity) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Permanently retire submission/publication; active transport must still drain. NULL harmless. */
void dust3d_worker_service_queue_close(Dust3DWorkerServiceQueue* queue) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Release matching active work AFTER transport cleanup, preserving newer waiting work.
 * Foreign identity rejects without consuming active. Caller retains original transport error.
 */
Dust3DWorkerQueueResult dust3d_worker_service_queue_abandon(Dust3DWorkerServiceQueue* queue,
    Dust3DWorkerRequestIdentity identity) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Admit using queue-authoritative current source and captured active identity/generation.
 * @param observed Real authenticated connection lifetime and OS peer observations.
 * @param receipt Scalar callback, never authoritative current source or peer evidence.
 * @param maximum_decoded_bytes Positive retained logical quota, at most128MiB.
 * @param result Required typed original receipt; NULL leaves attempt untouched.
 * @return Independent decoded owner or NULL. Non-NULL result consumes active attempt even on rejection,
 * retaining newer waiting work. Copy decoded buffers before destroying decoded owner.
 * All C++ exceptions are caught. No publication/authentication/native cancellation guarantee.
 * On original framing failure retain its cause and abandon after transport cleanup.
 */
Dust3DWorkerDecodedReply* dust3d_worker_service_queue_admit(Dust3DWorkerServiceQueue* queue,
    Dust3DWorkerServiceObservation observed, Dust3DWorkerServiceReceipt receipt,
    uint64_t maximum_decoded_bytes, Dust3DWorkerDecodeResult* result) DUST3D_WORKER_RUNTIME_NOEXCEPT;
/** Release coordinator and borrowed views after transport cleanup; NULL harmless. */
void dust3d_worker_service_queue_destroy(Dust3DWorkerServiceQueue* queue) DUST3D_WORKER_RUNTIME_NOEXCEPT;

#ifdef __cplusplus
}
#endif
#undef DUST3D_WORKER_RUNTIME_NOEXCEPT
#endif

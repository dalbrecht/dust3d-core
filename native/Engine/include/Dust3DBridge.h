#ifndef DUST3D_NATIVE_ENGINE_BRIDGE_H_
#define DUST3D_NATIVE_ENGINE_BRIDGE_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** An owning, immutable mesh handle. Release with dust3d_mesh_destroy. */
typedef struct Dust3DMesh Dust3DMesh;

/** Independent owning indexed buffers. Release with dust3d_indexed_mesh_destroy. */
typedef struct Dust3DIndexedMesh Dust3DIndexedMesh;

typedef struct Dust3DVertex {
    float position[3];
    float normal[3];
    float color[4];
} Dust3DVertex;

typedef enum Dust3DStatus {
    DUST3D_STATUS_OK = 0,
    DUST3D_STATUS_INVALID_DOCUMENT = 1,
    DUST3D_STATUS_UNSUPPORTED_ASSET = 2,
    DUST3D_STATUS_GENERATION_FAILED = 3,
    DUST3D_STATUS_LIMIT_EXCEEDED = 4,
    /** Generation succeeded but produced no triangles, such as an unfilled isolated loop. */
    DUST3D_STATUS_NO_GEOMETRY = 5
} Dust3DStatus;

typedef struct Dust3DError {
    Dust3DStatus status;
    char message[512];
} Dust3DError;

/** Optional per-expanded-corner source hints; not exact surface ownership. */
typedef enum Dust3DSourceKind {
    DUST3D_SOURCE_UNKNOWN = 0,
    DUST3D_SOURCE_NODE_HINT = 1,
    DUST3D_SOURCE_WEIGHTED_HINT = 2,
    DUST3D_SOURCE_AMBIGUOUS = 3
} Dust3DSourceKind;

/** A corner's range in the independent influence buffer. */
typedef struct Dust3DSourceCorner {
    Dust3DSourceKind kind;
    size_t first_influence;
    size_t influence_count;
} Dust3DSourceCorner;

/** Original document identity index, or SIZE_MAX for unknown/ambiguous identity.
 * Weights are inherited finite nonnegative values, not renormalized by export.
 */
typedef struct Dust3DSourceInfluence {
    Dust3DSourceKind kind;
    size_t source_index;
    float weight;
} Dust3DSourceInfluence;

/** Opt-in source output owner. Release only with dust3d_sourced_mesh_destroy. */
typedef struct Dust3DSourcedMesh Dust3DSourcedMesh;

/** Source payload defaults and hard maximum, in bytes. */
#define DUST3D_DEFAULT_SOURCE_PAYLOAD_BYTES 67108864
#define DUST3D_MAX_SOURCE_PAYLOAD_BYTES 268435456

/** Source-matched options. struct_size must equal sizeof(Dust3DSourceOptions).
 * maximum_payload_bytes bounds source identity records/strings, corners and influences;
 * zero selects 64 MiB, values above 256 MiB are rejected. Geometry, lookup workspace,
 * allocator overhead and container capacity growth are outside this logical payload budget.
 */
typedef struct Dust3DSourceOptions {
    size_t struct_size;
    size_t maximum_payload_bytes;
} Dust3DSourceOptions;

/** Generate independent geometry and optional quantized node association hints.
 * Sources use original opaque document keys, including non-hex legacy identifiers.
 * Generated/mirrored/interpolated identities absent from original membership remain
 * UNKNOWN; multiple original keys sharing one engine identity are AMBIGUOUS.
 * Position associations use inherited 1e-5 truncation buckets: hints can collide,
 * and Boolean-generated surfaces may have no association. No exact ownership claim.
 * Weighted hints preserve every entry, including individually unknown/ambiguous ones.
 * @param bytes Complete archive bytes, borrowed during this serialized call.
 * @param size Archive size; ordinary 256 MiB input limit applies.
 * @param options Required correctly sized options, borrowed during the call.
 * @param error Optional diagnostic, initialized on entry. Never throws.
 * @return Independent owner or NULL; INVALID_DOCUMENT for missing/invalid options,
 * LIMIT_EXCEEDED for source budget/allocation limits, normal load errors otherwise.
 * Source payload failures fail this opt-in call only; legacy load remains unchanged.
 * @code
 * Dust3DSourceOptions options = { sizeof(Dust3DSourceOptions), 0 };
 * Dust3DSourcedMesh* owner = dust3d_sourced_mesh_load(bytes, size, &options, &error);
 * if (owner != NULL) {
 *     const Dust3DMesh* geometry = dust3d_sourced_mesh_geometry(owner);
 *     Dust3DIndexedMesh* indexed = dust3d_mesh_create_indexed(geometry, 0, &error);
 *     // Copy corner records and identity strings before releasing owner.
 *     dust3d_sourced_mesh_destroy(owner); // Also releases borrowed geometry.
 *     dust3d_indexed_mesh_destroy(indexed); // Independent copy, or NULL on error.
 * }
 * @endcode
 */
Dust3DSourcedMesh* dust3d_sourced_mesh_load(const uint8_t* bytes, size_t size, const Dust3DSourceOptions* options, Dust3DError* error);
/** Release source and geometry ownership. NULL accepted. */
void dust3d_sourced_mesh_destroy(Dust3DSourcedMesh* mesh);
/** Borrowed geometry, valid until sourced owner destruction. Never destroy this view.
 * May be passed to dust3d_mesh_create_indexed to obtain an independent indexed owner.
 * NULL for NULL owner. Corner i corresponds to expanded vertex i and indexed index i.
 */
const Dust3DMesh* dust3d_sourced_mesh_geometry(const Dust3DSourcedMesh* mesh);
/** Borrowed arrays; valid until owner destruction. NULL for NULL/empty owner. */
const Dust3DSourceCorner* dust3d_sourced_mesh_corners(const Dust3DSourcedMesh* mesh);
const Dust3DSourceInfluence* dust3d_sourced_mesh_influences(const Dust3DSourcedMesh* mesh);
/** Array/identity counts; zero for NULL. */
size_t dust3d_sourced_mesh_corner_count(const Dust3DSourcedMesh* mesh);
size_t dust3d_sourced_mesh_influence_count(const Dust3DSourcedMesh* mesh);
size_t dust3d_sourced_mesh_identity_count(const Dust3DSourcedMesh* mesh);
/** Borrowed original node/part key; empty string for NULL/out-of-range. Never free.
 * All original nodes are listed; aliases are retained but never chosen arbitrarily.
 */
const char* dust3d_sourced_mesh_node_id(const Dust3DSourcedMesh* mesh, size_t identity_index);
const char* dust3d_sourced_mesh_part_id(const Dust3DSourcedMesh* mesh, size_t identity_index);
/** Logical source payload bytes charged to the configured budget; zero for NULL. */
size_t dust3d_sourced_mesh_payload_bytes(const Dust3DSourcedMesh* mesh);

/** Independent geometry/UV metadata owner. Release with dust3d_uv_mesh_destroy. */
typedef struct Dust3DUvMesh Dust3DUvMesh;
#define DUST3D_DEFAULT_UV_PAYLOAD_BYTES 67108864
#define DUST3D_MAX_UV_PAYLOAD_BYTES 268435456

/** Component association, not a guarantee of unique Boolean surface ownership. */
typedef enum Dust3DUvComponentKind {
    DUST3D_UV_UNKNOWN = 0,
    DUST3D_UV_COMPONENT = 1,
    DUST3D_UV_AMBIGUOUS = 2
} Dust3DUvComponentKind;

/** Caller-resolved image dimensions, no pixels. image_id is a NUL-terminated
 * 36/38-character engine identity in this fixed buffer; dimensions are 1..8192.
 * Descriptors are borrowed only during load. Duplicate engine identities reject.
 */
typedef struct Dust3DUvImage {
    char image_id[39];
    size_t width;
    size_t height;
} Dust3DUvImage;

/** Required source-matched options. struct_size equals sizeof(Dust3DUvOptions).
 * Zero budget selects 64 MiB, maximum 256 MiB. Charges retained atlas elements,
 * C metadata arrays and original component strings; excludes geometry, lookup
 * workspace, container capacity and allocator overhead. Not a process memory cap.
 * texture_size: zero selects 4096, otherwise 1..8192. images/image_count supply
 * at most 10000 unique resolved image descriptors; nonzero count needs an array.
 */
typedef struct Dust3DUvOptions {
    size_t struct_size;
    size_t maximum_payload_bytes;
    size_t texture_size;
    const Dust3DUvImage* images;
    size_t image_count;
} Dust3DUvOptions;

/** UVs in actual exported corner order, aligned with three expanded vertices.
 * known is 1 for an applicable chart, 0 for missing mapping (zero UVs).
 * component_index references original keys only for COMPONENT, otherwise SIZE_MAX.
 * UV mapping and source association are independent; known does not prove ownership.
 */
typedef struct Dust3DUvTriangle {
    double uv[3][2];
    uint8_t known;
    Dust3DUvComponentKind component_kind;
    size_t component_index;
} Dust3DUvTriangle;

/** Normalized chart rectangle and RGBA modulation; flipped indicates packing rotation.
 * image_id is empty for solid/gradient charts; gradient records reference chart_index.
 * Component identity semantics match Dust3DUvTriangle. Colors are normalized values
 * in the document's source encoding; this API performs no color-space conversion.
 */
typedef struct Dust3DUvChart {
    double left;
    double top;
    double width;
    double height;
    double color[4];
    uint8_t flipped;
    Dust3DUvComponentKind component_kind;
    size_t component_index;
    char image_id[39];
} Dust3DUvChart;

/** Horizontal seam gradient descriptor, not baked pixels. */
typedef struct Dust3DUvGradient {
    size_t chart_index;
    double large_color[4];
    double small_color[4];
    size_t resolution;
} Dust3DUvGradient;

/** Generate independently owned geometry, atlas and original component keys.
 * Only float-retained geometry triangles are exported in UV order. Missing charts
 * remain explicit. Original opaque spelling/case is preserved; aliases are AMBIGUOUS,
 * generated identities absent from original membership are UNKNOWN. Conflicting
 * visual definitions for one engine identity reject rather than choosing an alias.
 * Generated colors accept strict #RRGGBB/#AARRGGBB; other nonempty encodings and
 * unresolved image dimensions return UNSUPPORTED_ASSET. No decoding/baking occurs.
 * @param bytes Complete archive bytes, borrowed during this serialized call.
 * @param size Byte count, subject to normal 256 MiB document limit.
 * @param options Required validated options, borrowed only during load.
 * @param error Optional initialized diagnostic; invalid options INVALID_DOCUMENT,
 * quota/allocation LIMIT_EXCEEDED, missing visuals UNSUPPORTED_ASSET, normal load
 * and atlas packing failures otherwise. Never throws across C. Ordinary load is unchanged.
 * @return Owner or NULL. All views expire on destruction; copy before release.
 * @code
 * Dust3DUvOptions options = { sizeof(Dust3DUvOptions), 0, 0, NULL, 0 };
 * Dust3DUvMesh* owner = dust3d_uv_mesh_load(bytes, size, &options, &error);
 * if (owner != NULL) {
 *     const Dust3DUvTriangle* triangles = dust3d_uv_mesh_triangles(owner);
 *     // Copy triangles and inspect known before sampling a texture.
 *     dust3d_uv_mesh_destroy(owner);
 * }
 * @endcode
 */
Dust3DUvMesh* dust3d_uv_mesh_load(const uint8_t* bytes, size_t size, const Dust3DUvOptions* options, Dust3DError* error);
/** Release this owner and its borrowed geometry. NULL accepted. */
void dust3d_uv_mesh_destroy(Dust3DUvMesh* mesh);
/** Borrowed geometry. Never destroy this view; create an independent indexed copy
 * with dust3d_mesh_create_indexed. Corner 3*t+c matches triangle[t].uv[c].
 * NULL for NULL owner. Existing indexed sharing uses geometry attributes only;
 * UV seams must be included when constructing a future textured indexed mesh.
 */
const Dust3DMesh* dust3d_uv_mesh_geometry(const Dust3DUvMesh* mesh);
/** Borrowed arrays, NULL for NULL/empty owner; valid until owner destruction. */
const Dust3DUvTriangle* dust3d_uv_mesh_triangles(const Dust3DUvMesh* mesh);
const Dust3DUvChart* dust3d_uv_mesh_charts(const Dust3DUvMesh* mesh);
const Dust3DUvGradient* dust3d_uv_mesh_gradients(const Dust3DUvMesh* mesh);
/** Array counts, zero for NULL. */
size_t dust3d_uv_mesh_triangle_count(const Dust3DUvMesh* mesh);
size_t dust3d_uv_mesh_chart_count(const Dust3DUvMesh* mesh);
size_t dust3d_uv_mesh_gradient_count(const Dust3DUvMesh* mesh);
size_t dust3d_uv_mesh_component_count(const Dust3DUvMesh* mesh);
/** Original component key, empty for NULL/out-of-range. Borrowed; never free. */
const char* dust3d_uv_mesh_component_id(const Dust3DUvMesh* mesh, size_t index);
/** Charged logical metadata payload bytes, zero for NULL. */
size_t dust3d_uv_mesh_payload_bytes(const Dust3DUvMesh* mesh);
/** Inherited packed texture scale, zero for NULL/no charts. */
double dust3d_uv_mesh_texture_size(const Dust3DUvMesh* mesh);

/** Mutable generation cache owner. Destroy only when no calls are using it. */
typedef struct Dust3DGenerationSession Dust3DGenerationSession;

/** Integer literals keep these constants available to C and Swift importers (64/256 MiB). */
#define DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES 67108864
#define DUST3D_MAX_CACHE_PAYLOAD_BYTES 268435456

typedef enum Dust3DCacheOutcome {
    DUST3D_CACHE_EMPTY = 0,
    DUST3D_CACHE_DISABLED = 1,
    DUST3D_CACHE_RETAINED = 2,
    DUST3D_CACHE_BYPASSED = 3,
    DUST3D_CACHE_RESET_AFTER_FAILURE = 4
} Dust3DCacheOutcome;

/** Per-call work and retained object/value/buffer payload, excluding allocator overhead,
 * temporary generation workspace and caller-owned output. Cache failures are independent
 * of the primary generation diagnostic. Source-matched prototype ABI; no binary ABI guarantee.
 */
typedef struct Dust3DGenerationStats {
    size_t maximum_cache_payload_bytes;
    size_t retained_payload_bytes;
    size_t reused_components;
    size_t rebuilt_components;
    size_t rebuilt_parts;
    Dust3DCacheOutcome cache_outcome;
    Dust3DStatus cache_status;
    char cache_message[512];
} Dust3DGenerationStats;

/** Create a bounded session. Zero disables retention; values above MAX fail with LIMIT_EXCEEDED.
 * @param maximum_cache_payload_bytes Owned retained payload limit, not process memory.
 * @param error Optional diagnostic output, reset on every call.
 * @return An owner or NULL on allocation/argument failure. Never throws.
 */
Dust3DGenerationSession* dust3d_generation_session_create(size_t maximum_cache_payload_bytes, Dust3DError* error);
/** Release all owned cache state. NULL is accepted; concurrent destruction/use is forbidden. */
void dust3d_generation_session_destroy(Dust3DGenerationSession* session);
/** Generate from borrowed complete archive bytes. All legacy/session generation is serialized.
 * @param session Valid session owner; NULL reports INVALID_DOCUMENT.
 * @param bytes Archive bytes, borrowed only during this call.
 * @param size Archive byte count; the ordinary 256 MiB input limit applies.
 * @param error Optional primary diagnostic output, reset on every call.
 * @return Independent owning mesh, unaffected by later loads/reset/destruction, or NULL.
 * NO_GEOMETRY is a successful empty result and may retain cache state. Other primary
 * failures clear partial cache state. Cache-only failures fall back to fresh generation
 * and preserve their cause in stats. Never throws across the C boundary.
 */
Dust3DMesh* dust3d_generation_session_load(Dust3DGenerationSession* session, const uint8_t* bytes, size_t size, Dust3DError* error);
/** Generate source hints using the session's bounded component caches.
 * @param session Valid cache owner; NULL reports INVALID_DOCUMENT.
 * @param bytes Complete archive bytes, borrowed only during the serialized call.
 * @param size Archive size; the ordinary input limit applies.
 * @param options Required source options; source/cache budgets are independent.
 * @param error Optional primary diagnostic, initialized on entry.
 * @return Independent sourced owner or NULL with the ordinary source-load status.
 * Invalid options preserve prior session cache/statistics. After valid options,
 * source allocation/budget failures fail this request and clear partial cache;
 * cache-only failures still return valid sourced geometry with separate stats.
 * NO_GEOMETRY remains successful emptiness and may retain cache. All source arrays
 * describe current original document keys, not retained previous output metadata.
 * Never throws. Ownership/accessor rules match dust3d_sourced_mesh_load.
 * @code
 * Dust3DSourceOptions options = { sizeof(Dust3DSourceOptions), 0 };
 * Dust3DSourcedMesh* owner = dust3d_generation_session_load_sourced(session, bytes, size, &options, &error);
 * dust3d_sourced_mesh_destroy(owner); // NULL accepted; independent of session.
 * @endcode
 */
Dust3DSourcedMesh* dust3d_generation_session_load_sourced(Dust3DGenerationSession* session, const uint8_t* bytes, size_t size, const Dust3DSourceOptions* options, Dust3DError* error);

/** Release retained state and clear per-call metrics; keep the configured limit.
 * @param session Valid session owner.
 * @param error Optional diagnostic output.
 * @return OK, or INVALID_DOCUMENT for NULL; unexpected lock failures report GENERATION_FAILED.
 */
Dust3DStatus dust3d_generation_session_reset(Dust3DGenerationSession* session, Dust3DError* error);
/** Copy stats under the shared engine lock. Invalid/short outputs remain untouched.
 * @param session Valid session owner.
 * @param output Writable stats storage.
 * @param output_capacity Available bytes; at least sizeof(Dust3DGenerationStats).
 * @param error Optional diagnostic output.
 * @return OK, INVALID_DOCUMENT for NULL arguments, LIMIT_EXCEEDED for short storage,
 * or GENERATION_FAILED for an unexpected lock failure. Never throws.
 * @code
 * Dust3DGenerationSession* session = dust3d_generation_session_create(DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES, NULL);
 * Dust3DGenerationStats stats;
 * dust3d_generation_session_get_stats(session, &stats, sizeof(stats), NULL);
 * dust3d_generation_session_destroy(session);
 * @endcode
 */
Dust3DStatus dust3d_generation_session_get_stats(const Dust3DGenerationSession* session, Dust3DGenerationStats* output, size_t output_capacity, Dust3DError* error);

/**
 * Generate a mesh from a complete .ds3 container. Bytes are borrowed only during this call.
 * Calls are internally serialized. Never throws across the C boundary.
 * Returns an owning handle for nonempty geometry, or NULL and a descriptive diagnostic.
 * Zero-area triangles at exported float precision are omitted after validation.
 * If all generated triangles collapse at that precision, DUST3D_STATUS_GENERATION_FAILED is returned.
 * DUST3D_STATUS_NO_GEOMETRY means generation succeeded without triangles; other non-OK statuses are failures.
 * Maximum input size: 256 MiB. Imported GLB geometry is not supported by this viewer.
 */
Dust3DMesh* dust3d_mesh_load(const uint8_t* bytes, size_t size, Dust3DError* error);

/** Release an owning handle. NULL is accepted. */
void dust3d_mesh_destroy(Dust3DMesh* mesh);

/** Borrowed expanded triangle vertices with positive area at exported float precision.
 * Valid until mesh destruction. NULL for a NULL handle.
 */
const Dust3DVertex* dust3d_mesh_vertices(const Dust3DMesh* mesh);

/** Number of expanded vertices (a multiple of three). Zero for a NULL handle. */
size_t dust3d_mesh_vertex_count(const Dust3DMesh* mesh);

/** Number of source nodes and parts. Zero for a NULL handle. */
size_t dust3d_mesh_node_count(const Dust3DMesh* mesh);
size_t dust3d_mesh_part_count(const Dust3DMesh* mesh);

/** Borrowed compatibility warning, or an empty string. Valid until mesh destruction. */
const char* dust3d_mesh_warning(const Dust3DMesh* mesh);

/**
 * Create an independently owned indexed copy of a validated expanded mesh.
 * The source is borrowed only during this call and may be destroyed afterward.
 * Complete finite position/normal/color values define vertex sharing, preserving seams.
 * maximum_bytes limits exported vertex/index payload bytes; zero selects 256 MiB.
 * Source storage, temporary lookup memory and allocator overhead are outside this payload budget.
 * Returns an owner or NULL with a diagnostic; NULL error is accepted. Never throws.
 * INVALID_DOCUMENT rejects a NULL source; LIMIT_EXCEEDED reports insufficient budget.
 * GENERATION_FAILED reports nonfinite source attributes or unexpected conversion failure.
 * @param mesh Valid expanded owner, borrowed for this call.
 * @param maximum_bytes Exported payload budget in bytes; zero uses 256 MiB.
 * @param error Optional diagnostic output, reset on every call.
 * @return An independent owner on success, otherwise NULL with a descriptive diagnostic.
 * @code
 * Dust3DIndexedMesh* indexed = dust3d_mesh_create_indexed(source, 0, &error);
 * if (indexed != NULL) {
 *     const uint32_t* indices = dust3d_indexed_mesh_indices(indexed);
 *     // Read buffers while indexed remains alive.
 *     dust3d_indexed_mesh_destroy(indexed);
 * }
 * @endcode
 */
Dust3DIndexedMesh* dust3d_mesh_create_indexed(const Dust3DMesh* mesh, size_t maximum_bytes, Dust3DError* error);

/** Release independent indexed storage. NULL is accepted. */
void dust3d_indexed_mesh_destroy(Dust3DIndexedMesh* mesh);

/** Borrowed unique full-attribute vertices, valid until indexed owner destruction; NULL for NULL owner. */
const Dust3DVertex* dust3d_indexed_mesh_vertices(const Dust3DIndexedMesh* mesh);

/** Unique vertex count; zero for NULL owner. */
size_t dust3d_indexed_mesh_vertex_count(const Dust3DIndexedMesh* mesh);

/** Borrowed UInt32 triangle indices, valid until indexed owner destruction; NULL for NULL owner. */
const uint32_t* dust3d_indexed_mesh_indices(const Dust3DIndexedMesh* mesh);

/** Index count, a multiple of three equal to the source expanded count; zero for NULL owner. */
size_t dust3d_indexed_mesh_index_count(const Dust3DIndexedMesh* mesh);

/** Copied source node/part counts; zero for NULL owner. */
size_t dust3d_indexed_mesh_node_count(const Dust3DIndexedMesh* mesh);
size_t dust3d_indexed_mesh_part_count(const Dust3DIndexedMesh* mesh);

/** Borrowed copied compatibility warning, valid until indexed destruction; empty for NULL owner. */
const char* dust3d_indexed_mesh_warning(const Dust3DIndexedMesh* mesh);

#ifdef __cplusplus
}
#endif

#endif

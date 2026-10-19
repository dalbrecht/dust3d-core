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
    DUST3D_STATUS_NO_GEOMETRY = 5,
    /** An opt-in operation observed its cooperative cancellation token. */
    DUST3D_STATUS_CANCELLED = 6
} Dust3DStatus;

typedef struct Dust3DError {
    Dust3DStatus status;
    char message[512];
} Dust3DError;

/** Immutable imported material/image metadata owner; no geometry or pixel decoding.
 * Release with dust3d_imported_assets_destroy. Source-matched prototype ABI.
 */
typedef struct Dust3DImportedAssets Dust3DImportedAssets;

typedef enum Dust3DImportedAlphaMode {
    DUST3D_IMPORTED_ALPHA_OPAQUE = 0,
    DUST3D_IMPORTED_ALPHA_MASK = 1,
    DUST3D_IMPORTED_ALPHA_BLEND = 2
} Dust3DImportedAlphaMode;

/** Borrowed, untrusted encoded storage; MIME is declared by GLB, not pixel validation.
 * PNG/JPEG data must be validated/decoded at the caller's platform boundary.
 * Absent images have NULL pointers and zero byte_count.
 */
typedef struct Dust3DImportedEncodedImageView {
    const char* mime_type;
    const uint8_t* bytes;
    size_t byte_count;
} Dust3DImportedEncodedImageView;

/** Copied glTF factors and sampler state, plus views borrowed from the owner.
 * Factors are unpremultiplied source values; no material application is implied.
 * Filter-presence flags distinguish unspecified filtering from explicit GL constants.
 * Without a sampler, its flags/values are zero; an image binding has a sampler
 * with glTF repeat defaults even when the source omits a sampler object.
 */
typedef struct Dust3DImportedMaterialView {
    const char* model_id;
    double base_color[4];
    double metallic;
    double roughness;
    double emissive[3];
    Dust3DImportedAlphaMode alpha_mode;
    double alpha_cutoff;
    uint32_t is_double_sided;
    uint32_t has_sampler;
    uint32_t has_magnification_filter;
    uint32_t magnification_filter;
    uint32_t has_minification_filter;
    uint32_t minification_filter;
    uint32_t wrap_s;
    uint32_t wrap_t;
    Dust3DImportedEncodedImageView image;
} Dust3DImportedMaterialView;

/** Original opaque identities, not UUID-collapsed spellings. A NULL component_id
 * with non-NULL part_id represents an imported part with no component instance.
 * model_index refers to this owner's material records. No ordering is promised.
 */
typedef struct Dust3DImportedBindingView {
    const char* component_id;
    const char* part_id;
    size_t model_index;
} Dust3DImportedBindingView;

#define DUST3D_DEFAULT_IMPORTED_ASSET_PAYLOAD_BYTES 67108864
#define DUST3D_MAX_IMPORTED_ASSET_PAYLOAD_BYTES 268435456

/** NULL options and zero maximum_payload_bytes select the nonzero default.
 * struct_size must be at least sizeof(Dust3DImportedAssetOptions).
 * Output quota counts owned object/record/identity/buffer capacities, excluding
 * allocator bookkeeping, parser/generator workspace and temporary lookup indexes.
 * Existing input/decoded-model limits apply separately; this is not an RSS bound.
 */
typedef struct Dust3DImportedAssetOptions {
    size_t struct_size;
    size_t maximum_payload_bytes;
} Dust3DImportedAssetOptions;

/** Resolve required models and copy/move their materials into an independent owner.
 * Does not generate geometry, decode image pixels, or apply/bake materials.
 * Original input is unchanged and borrowed only during this call. All engine
 * entry points serialize inherited globals. Optional caches are not involved.
 * @param bytes Readable complete archive storage (NULL is invalid).
 * @param size Archive length, subject to the existing 256 MiB input limit.
 * @param options Optional validated output quota.
 * @param error Optional initialized status/original diagnostic.
 * @return Owner or NULL for malformed, unsupported, over-budget or allocation failure.
 * Missing/malformed required assets retain their cause before output quota checks.
 * Never throws across C. Copy borrowed views before destroying their owner.
 * Example: Dust3DImportedAssets* assets = dust3d_imported_assets_load(data, size, NULL, &error);
 */
Dust3DImportedAssets* dust3d_imported_assets_load(const uint8_t* bytes, size_t size, const Dust3DImportedAssetOptions* options, Dust3DError* error);
/** Release a quiescent owner. NULL accepted; destruction cannot race any use. */
void dust3d_imported_assets_destroy(Dust3DImportedAssets* assets);
/** Required unique model-material count; NULL returns zero. */
size_t dust3d_imported_assets_model_count(const Dust3DImportedAssets* assets);
/** Instance/uninstanced-part binding count; NULL returns zero. */
size_t dust3d_imported_assets_binding_count(const Dust3DImportedAssets* assets);
/** Accounted retained output payload; NULL returns zero. */
size_t dust3d_imported_assets_payload_bytes(const Dust3DImportedAssets* assets);
/** Return copied factors plus owner-borrowed strings/image bytes.
 * NULL/out-of-range returns a zeroed view with NULL model_id. All non-NULL
 * pointers remain valid until owner destruction, independent of subsequent loads.
 * Example: Dust3DImportedMaterialView material = dust3d_imported_assets_material(assets, 0);
 */
Dust3DImportedMaterialView dust3d_imported_assets_material(const Dust3DImportedAssets* assets, size_t index);
/** Return owner-borrowed original identities. NULL/out-of-range returns a zeroed
 * view with NULL part_id. Lifetimes match dust3d_imported_assets_material.
 */
Dust3DImportedBindingView dust3d_imported_assets_binding(const Dust3DImportedAssets* assets, size_t index);

/** One-shot cooperative cancellation state, independent of the engine lock.
 * The caller must retain the token until the operation and all concurrent cancel/query
 * calls finish. Destruction must never race any use. Tokens cannot be reset/reused
 * for a new operation once cancelled. No hard cancellation latency is promised.
 */
typedef struct Dust3DCancellationToken Dust3DCancellationToken;

/** Create an initially uncancelled token. NULL on allocation failure; optional error
 * is initialized and receives LIMIT_EXCEEDED with its cause. Never throws.
 * Example: Dust3DCancellationToken* token = dust3d_cancellation_token_create(&error);
 */
Dust3DCancellationToken* dust3d_cancellation_token_create(Dust3DError* error);
/** Atomically request cancellation; repeat calls and NULL are accepted. Does not
 * acquire the engine lock. Safe concurrently with operations and queries on token.
 */
void dust3d_cancellation_token_cancel(Dust3DCancellationToken* token);
/** Read cancellation state (0 or 1); NULL returns 0. Does not acquire the engine lock. */
int dust3d_cancellation_token_is_cancelled(const Dust3DCancellationToken* token);
/** Release a quiescent token after all operations/cancel/query calls finish. NULL accepted. */
void dust3d_cancellation_token_destroy(Dust3DCancellationToken* token);

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
 * Maximum input size: 256 MiB. Required embedded GLB models use the bounded
 * single-mesh decoder and a 64 MiB aggregate decoded-owner payload limit.
 * Geometry previews disclose unapplied GLB materials/textures/transparency through
 * dust3d_mesh_warning. UV and baked-mesh APIs still reject imported models.
 * Missing, malformed or ambiguous required model assets fail the primary load;
 * unreferenced model assets are preserved as opaque input and are not decoded.
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

/** Independently owned baked output; release only with dust3d_baked_mesh_destroy. */
typedef struct Dust3DBakedMesh Dust3DBakedMesh;

/** Borrowed decoded straight-alpha RGBA8 image, valid throughout the synchronous
 * load. image_id must terminate within 39 bytes and resolve to a non-null UUID.
 * Dimensions are 1..8192; row_stride includes padding and byte_count bounds every
 * readable row. No decoding, color conversion or ownership transfer occurs.
 */
typedef struct Dust3DBakeImage {
    char image_id[39];
    size_t width;
    size_t height;
    size_t row_stride;
    size_t byte_count;
    const uint8_t* pixels;
} Dust3DBakeImage;

/** Sized opt-in bake options. Zero dimensions select atlas/color 4096, normal 2048,
 * material 1024. Explicit dimensions are 1..8192. Zero UV quota selects the existing
 * UV default; zero bake quota selects 256 MiB. Both quotas have a 256 MiB hard maximum.
 * Bake quota counts final map bytes and exported tangent records; temporary C++
 * tangent storage has an additional independent 256 MiB baker output ceiling.
 * Workspace, input buffers, allocator overhead and geometry are excluded.
 * maximum_spatial_grid_entries defaults to 1000000; hard maximum 16000000.
 * At most 10000 image descriptors may be borrowed. Cancellation token lifetime
 * follows its API contract. Generation only observes phase boundaries; there is
 * no whole-call cancellation latency guarantee.
 */
typedef struct Dust3DBakeOptions {
    size_t struct_size;
    size_t maximum_uv_payload_bytes;
    size_t maximum_bake_payload_bytes;
    size_t atlas_texture_size;
    size_t color_size;
    size_t normal_size;
    size_t material_size;
    size_t maximum_spatial_grid_entries;
    const Dust3DBakeImage* images;
    size_t image_count;
    const Dust3DCancellationToken* cancellation;
} Dust3DBakeOptions;

typedef enum Dust3DTextureMap {
    DUST3D_TEXTURE_COLOR = 0,
    DUST3D_TEXTURE_NORMAL = 1,
    DUST3D_TEXTURE_METALLIC = 2,
    DUST3D_TEXTURE_ROUGHNESS = 3,
    DUST3D_TEXTURE_EMISSIVE = 4
} Dust3DTextureMap;

/** Borrowed tightly packed straight-alpha RGBA8 bytes. Color/emission retain
 * source encoding; normal/metallic/roughness are linear data. Valid until baked
 * owner destruction. Absent maps and invalid requests return an all-zero value.
 */
typedef struct Dust3DTextureView {
    size_t width;
    size_t height;
    size_t row_stride;
    size_t byte_count;
    const uint8_t* pixels;
} Dust3DTextureView;

/** One tangent frame per exported triangle, aligned with the borrowed UV mesh.
 * known=0 marks unavailable/degenerate UVs; never infer validity from zero values.
 */
typedef struct Dust3DTextureTangent {
    float direction[3];
    float handedness;
    uint8_t known;
} Dust3DTextureTangent;

/** Generate and bake without retaining generated Object/Snapshot state.
 * @param bytes Complete archive, borrowed during this serialized call.
 * @param size Archive size; ordinary input and geometry limits apply.
 * @param options Required correctly sized options; all referenced inputs borrowed.
 * @param error Optional diagnostic initialized on entry; original causes preserved.
 * @return Independent owner or NULL. INVALID_DOCUMENT for invalid options/views,
 * UNSUPPORTED_ASSET for unresolved/conflicting visual inputs, LIMIT_EXCEEDED for
 * quotas/allocation, CANCELLED for observed cancellation, normal load errors otherwise.
 * Never throws. No partial owner escapes; input pixels are never retained.
 * Example: Dust3DBakeOptions options = {0}; options.struct_size = sizeof(options);
 * Dust3DBakedMesh* result = dust3d_baked_mesh_load(bytes, size, &options, &error);
 */
Dust3DBakedMesh* dust3d_baked_mesh_load(const uint8_t* bytes, size_t size, const Dust3DBakeOptions* options, Dust3DError* error);
/** Release baked output and all borrowed views. NULL accepted. */
void dust3d_baked_mesh_destroy(Dust3DBakedMesh* mesh);
/** Borrowed UV/geometry owner; never destroy separately. NULL for NULL owner. */
const Dust3DUvMesh* dust3d_baked_mesh_uvs(const Dust3DBakedMesh* mesh);
/** Borrowed map view; map is a Dust3DTextureMap constant transported as uint32_t
 * so unknown foreign selector values can be rejected without C++ enum undefined
 * behavior. Zero for NULL owner, unknown selector or absent optional map. */
Dust3DTextureView dust3d_baked_mesh_texture(const Dust3DBakedMesh* mesh, uint32_t map);
/** Borrowed aligned tangents, valid until owner destruction; NULL for NULL owner. */
const Dust3DTextureTangent* dust3d_baked_mesh_tangents(const Dust3DBakedMesh* mesh);
/** Number of exported triangle tangent records; zero for NULL owner. */
size_t dust3d_baked_mesh_tangent_count(const Dust3DBakedMesh* mesh);
/** Final map/tangent logical bytes, excluding independent UV and geometry storage;
 * zero for NULL owner. Does not establish a process memory cap.
 */
size_t dust3d_baked_mesh_payload_bytes(const Dust3DBakedMesh* mesh);

#ifdef __cplusplus
}
#endif

#endif

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

/** Mutable generation cache owner. Destroy only when no calls are using it. */
typedef struct Dust3DGenerationSession Dust3DGenerationSession;

#define DUST3D_DEFAULT_CACHE_PAYLOAD_BYTES ((size_t)64 * 1024 * 1024)
#define DUST3D_MAX_CACHE_PAYLOAD_BYTES ((size_t)256 * 1024 * 1024)

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

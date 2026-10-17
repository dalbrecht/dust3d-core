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

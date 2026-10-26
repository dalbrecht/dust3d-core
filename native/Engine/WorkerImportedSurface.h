// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_H_
#define DUST3D_NATIVE_WORKER_IMPORTED_SURFACE_H_
#include "WorkerMesh.h"
#include <limits>
namespace dust3d {
namespace worker {
    constexpr size_t importedSurfacePrefixBytes = 24;
    constexpr size_t importedMetadataPrefixBytes = 40;
    constexpr size_t importedMetadataTriangleBytes = 168;
    constexpr uint64_t noImportedIdentity = std::numeric_limits<uint64_t>::max();
    /// Authored UVs may lie outside the unit square. Colors are straight RGBA;
    /// encoding 0 is sRGB and 1 is linear RGB. Unknown UVs must be zero.
    struct ImportedSurfaceTriangle {
        std::array<std::array<double, 2>, 3> uv {};
        std::array<std::array<double, 4>, 3> colors {};
        uint64_t modelIndex = noImportedIdentity, componentIndex = noImportedIdentity;
        uint8_t uvKnown = 0;
        uint32_t colorEncoding = 0;
    };
    /// Catalog membership, not proof of unique Boolean provenance.
    struct ImportedSurfaceComponent {
        std::string id;
        uint32_t kind = 0;
    };
    /// Matching expanded geometry and authored source attributes with independent ownership.
    struct ImportedSurfacePayload {
        MeshPayload geometry;
        std::vector<ImportedSurfaceTriangle> triangles;
        std::vector<std::string> models;
        std::vector<ImportedSurfaceComponent> components;
    };
    struct ImportedMetadataLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        uint64_t maximumTriangles = 1000000;
        uint64_t maximumModels = 100000, maximumComponents = 100000;
        uint64_t maximumTextBytes = 8ULL * 1024 * 1024;
    };
    /// Logical charge includes records, geometry and terminated text; excludes roots,
    /// capacities, validation workspace, native generation and process RSS (Apple #280).
    struct ImportedSurfaceLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = 128ULL * 1024 * 1024;
        MeshPayloadLimits geometry;
        ImportedMetadataLimits surface;
    };
    struct ImportedSurfaceInspection {
        uint64_t vertexCount, triangleCount, logicalBytes;
    };
    enum class ImportedSurfaceErrorCode { invalidInput,
        invalidLimits,
        unsupportedVersion,
        invalidLayout,
        truncated,
        trailingData,
        limitExceeded,
        countMismatch,
        invalidMetadata,
        invalidIdentity };
    class ImportedSurfaceError : public std::runtime_error {
    public:
        ImportedSurfaceError(ImportedSurfaceErrorCode code, const char* cause);
        ImportedSurfaceErrorCode code() const noexcept { return m_code; }

    private:
        ImportedSurfaceErrorCode m_code;
    };
    /// Validate supported positive inner/aggregate policies; throws typed original errors.
    /// Example: validateImportedSurfaceLimits({});
    void validateImportedSurfaceLimits(const ImportedSurfaceLimits& limits);
    /// Encode stable owned geometry/source records under independent inner/aggregate quotas.
    /// Returns independent explicit-endian bytes. Throws ImportedSurfaceError, original
    /// MeshPayloadError or allocation errors. Example: auto bytes = encodeImportedSurfacePayload(value, {});
    std::vector<uint8_t> encodeImportedSurfacePayload(const ImportedSurfacePayload& value, const ImportedSurfaceLimits& limits);
    /// Inspect an exact readable borrowed span before retained output arrays are allocated.
    /// Returns matching counts/logical charge. Throws typed schema/semantic/quota errors
    /// or workspace allocation errors. Example: auto shape = inspectImportedSurfacePayload(bytes.data(), bytes.size(), {});
    ImportedSurfaceInspection inspectImportedSurfacePayload(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits);
    /// Decode independent storage after complete inspection. Input must remain stable
    /// for the call; no pointers escape. Throws original typed errors or allocation errors.
    /// Example: auto value = decodeImportedSurfacePayload(bytes.data(), bytes.size(), {});
    ImportedSurfacePayload decodeImportedSurfacePayload(const uint8_t* bytes, size_t size, const ImportedSurfaceLimits& limits);
}
}
#endif

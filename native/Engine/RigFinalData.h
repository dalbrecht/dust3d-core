// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_RIG_FINAL_DATA_H
#define DUST3D_NATIVE_RIG_FINAL_DATA_H
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace dust3d {
struct RigStructure;
class Object;
namespace native {
    constexpr uint32_t rigFinalNoJoint = std::numeric_limits<uint32_t>::max();
    enum class RigFinalDataFailure { invalidData,
        limitExceeded,
        cancelled };
    class RigFinalDataError : public std::runtime_error {
    public:
        RigFinalDataError(RigFinalDataFailure failure, const char* cause);
        RigFinalDataFailure failure() const { return m_failure; }

    private:
        RigFinalDataFailure m_failure;
    };
    struct RigFinalJoint {
        std::string name;
        uint32_t parentIndex = rigFinalNoJoint;
        std::array<float, 3> headPosition {};
        std::array<float, 3> tailPosition {};
        float capsuleRadius = 0;
        float closingAngle = 0;
    };
    struct RigFinalVertexWeights {
        std::array<uint32_t, 4> joints { rigFinalNoJoint, rigFinalNoJoint, rigFinalNoJoint, rigFinalNoJoint };
        std::array<float, 4> weights {};
    };
    struct RigFinalData {
        std::vector<RigFinalJoint> joints;
        std::vector<RigFinalVertexWeights> vertices;
        size_t logicalBytes = 0;
    };
    struct RigFinalDataOptions {
        size_t maximumBones = 256;
        size_t maximumVertices = 1000000;
        size_t maximumPayloadBytes = 64 * 1024 * 1024;
        const std::atomic_bool* cancellation = nullptr;
    };
    // Copy already-finalized values; never invokes generation, Object caches or pose evaluation.
    // Names/order/positive influence channels are retained exactly; zero-weight slots are unused.
    // Empty source channel vectors mean unused; nonempty channels must match source vertex count.
    // Bound weights must sum to1 within1e-4; no normalization or invented bindings is performed.
    // Zero-length bones are valid actual-rig data. Parents must resolve to a valid forest.
    // Logical quota charges native joint/weight records plus name bytes including NUL, before
    // result allocation; excludes source storage, lookup workspace and allocator overhead.
    // Input references and optional cancellation are borrowed for this call only. Throws typed
    // RigFinalDataError; allocation exceptions remain for a future C boundary to catch.
    RigFinalData copyFinalRigData(const RigStructure&, const Object&, const RigFinalDataOptions& = {});
    /// Validate original mesh fields before inherited binding/eyelid calls. Requires
    /// nonempty positions/triangles, finite coordinates within1M AND PositionKey range,
    /// exactly3 corners per face, and valid indices. Repeated/degenerate faces stay valid.
    /// Uses only options.maximumVertices (1..1M) and borrowed cancellation; triangle
    /// limit is1..5M. No input mutation, rig call or cache materializer; throws typed errors.
    void validateRigGeometry(const Object&, const RigFinalDataOptions& = {}, size_t maximumTriangles = 5000000);
}
}
#endif

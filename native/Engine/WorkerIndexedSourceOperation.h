// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_INDEXED_SOURCE_OPERATION_H_
#define DUST3D_NATIVE_WORKER_INDEXED_SOURCE_OPERATION_H_
#include "WorkerFailure.h"
#include "WorkerIndexedSources.h"
#include <Dust3DBridge.h>
#include <variant>
namespace dust3d {
namespace worker {
    using IndexedSourceGenerationResult = std::variant<IndexedSourcesPayload, FailurePayload>;
    /// Copy a stable borrowed indexed owner under checked output policies.
    /// @param owner Live immutable native indexed mesh, retained until return.
    /// @param limits Supported wire/count/logical quotas, not native workspace/RSS limits.
    /// @return Fully validated independent vertex/index/count/warning values.
    /// @throws IndexedMeshPayloadError, original MeshPayloadError or allocation errors.
    /// Example: auto mesh = copyNativeIndexedMesh(owner.get(), {});
    IndexedMeshPayload copyNativeIndexedMesh(const Dust3DIndexedMesh* owner, const IndexedMeshPayloadLimits& limits);
    /// Pair copies from an indexed owner derived from this exact stable source geometry.
    /// @param source Live immutable source owner retained until return.
    /// @param indexed Live indexed owner derived from source's borrowed geometry, retained until return.
    /// @param limits Supported independent and aggregate quotas; caller guarantees same-generation derivation.
    /// @return Complete independent pair after index/corner and node/identity correspondence.
    /// @throws Original typed codec or allocation errors; count agreement does not authenticate provenance.
    /// Example: auto pair = copyNativeIndexedSourcedMesh(source.get(), indexed.get(), {});
    IndexedSourcesPayload copyNativeIndexedSourcedMesh(const Dust3DSourcedMesh* source, const Dust3DIndexedMesh* indexed, const IndexedSourcesPayloadLimits& limits);
    /// Generate once, derive native indices from that same geometry and return an owned source pair.
    /// @param archive Stable complete borrowed archive bytes, retained until return.
    /// @param limits Supported copy policy, validated before generation; native source/index retention is bounded.
    /// @return Complete owned pair or mapped original failure; no borrowed native storage escapes.
    /// @throws Invalid configuration before execution; failure-reply allocation may still throw.
    /// Native generation/index lookup workspace, CPU and RSS remain outside these logical quotas.
    /// Example: auto result = generateIndexedSourcedMesh(archive, {});
    IndexedSourceGenerationResult generateIndexedSourcedMesh(const std::vector<uint8_t>& archive, const IndexedSourcesPayloadLimits& limits);
}
}
#endif

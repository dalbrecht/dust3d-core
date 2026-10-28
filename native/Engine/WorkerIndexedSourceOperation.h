// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_INDEXED_SOURCE_OPERATION_H_
#define DUST3D_NATIVE_WORKER_INDEXED_SOURCE_OPERATION_H_
#include "WorkerFailure.h"
#include "WorkerIndexedSources.h"
#include "WorkerPreviewArchive.h"
#include "WorkerPreviewRange.h"
#include <Dust3DBridge.h>
#include <variant>
namespace dust3d {
namespace worker {
    using IndexedSourceGenerationResult = std::variant<IndexedSourcesPayload, FailurePayload>;
    using IndexedGenerationResult = std::variant<IndexedMeshPayload, FailurePayload>;
    /// Copy a stable borrowed indexed owner under checked output policies.
    /// @param owner Live immutable native indexed mesh, retained until return.
    /// @param limits Supported wire/count/logical quotas, not native workspace/RSS limits.
    /// @return Fully validated independent vertex/index/count/warning values.
    /// @throws IndexedMeshPayloadError, original MeshPayloadError or allocation errors.
    /// Example: auto mesh = copyNativeIndexedMesh(owner.get(), {});
    IndexedMeshPayload copyNativeIndexedMesh(const Dust3DIndexedMesh* owner, const IndexedMeshPayloadLimits& limits);
    /// Copy stable native indexed records under independently captured native budgets.
    /// @param owner Live immutable owner retained until return; no borrowed data escapes.
    /// @param context Original archive count and native policies, not legacy wire quotas.
    /// @return Independent geometry preserving all vertex/index/warning values.
    /// @throws Original chunk/range/geometry/text or allocation errors. Counts, budget,
    /// membership and borrowed values are checked before allocating output arrays.
    /// No whole-wire validation buffer, generation, authentication or RSS guarantee.
    /// Example: auto mesh = copyNativePreviewIndexedMesh(owner.get(), captured);
    IndexedMeshPayload copyNativePreviewIndexedMesh(const Dust3DIndexedMesh* owner, PreviewArchiveContext context);
    /// Copy a same-generation stable source/indexed pair under captured native policies.
    /// @param source Immutable source owner retained through return.
    /// @param indexed Immutable indexed owner derived from this source's geometry.
    /// @param context Independent archive count and original source/indexed budgets.
    /// @return Fully validated independent records, original identities/kinds/weights.
    /// @throws Original chunk/range/source/geometry or allocation errors. Preflight
    /// source counts, text and associations before retaining output arrays. No whole
    /// wire buffer, generation or provenance authentication; caller guarantees derivation.
    /// Example: auto pair = copyNativePreviewIndexedSourcedMesh(source, indexed, captured);
    IndexedSourcesPayload copyNativePreviewIndexedSourcedMesh(const Dust3DSourcedMesh* source, const Dust3DIndexedMesh* indexed, PreviewArchiveContext context);
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
    /// Generate ordinary indexed geometry independently of optional source hints.
    /// @param archive Stable complete borrowed archive bytes, retained until return.
    /// @param limits Supported copy policy, validated before native execution.
    /// @return Complete owned indexed mesh or original bounded typed failure.
    /// @throws Invalid configuration before execution; failure-reply allocation may throw.
    /// Native lookup workspace, CPU and RSS are outside the logical record allowance.
    /// Example: auto result = generateIndexedMesh(archive, {});
    IndexedGenerationResult generateIndexedMesh(const std::vector<uint8_t>& archive, const IndexedMeshPayloadLimits& limits);
    /// Same owned source/index copy path using a borrowed existing native cache owner.
    /// Caller serializes session lifetime/generation/statistics. NULL rejects before work;
    /// preserves original generation errors and independent quotas. No stateless fallback.
    /// Example: auto result = generateSessionIndexedSourcedMesh(session, archive, limits);
    IndexedSourceGenerationResult generateSessionIndexedSourcedMesh(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedSourcesPayloadLimits& limits);
    /// Ordinary indexed equivalent using the same session. Caller serializes access.
    /// NULL rejects; typed failures/copy quotas match generateIndexedMesh.
    /// Example: auto result = generateSessionIndexedMesh(session, archive, limits);
    IndexedGenerationResult generateSessionIndexedMesh(Dust3DGenerationSession* session, const std::vector<uint8_t>& archive, const IndexedMeshPayloadLimits& limits);
    /// Ordinary native-budget fallback adapter for an existing borrowed session.
    /// @param session Live native owner; caller serializes generation/lifetime.
    /// @param archive Stable readable borrowed archive and captured native policies.
    /// @return Independent geometry or original classified native/copy failure.
    /// @throws Original argument/receipt-allocation errors; no scope admission here.
    /// Uses no legacy envelope quota, copy count cap or whole wire buffer. Kept
    /// independently callable so healthy fallback behavior needs no native fault probe.
    /// Example: auto result = generateSessionPreviewIndexedMesh(owner.get(), archive);
    IndexedGenerationResult generateSessionPreviewIndexedMesh(Dust3DGenerationSession* session, PreviewArchiveView archive);
    /// Validate a complete indexed-source request and return a correlated pair/failure frame.
    /// @param request Stable exact source-budget request, operation 4, request kind.
    /// @param limits Supported aggregate/inner policies; requested budget can only tighten them.
    /// @return Complete independent version-two pair or original typed failure.
    /// @throws Protocol/request/policy/codec or allocation errors; no partial reply.
    /// Example: auto reply = generateIndexedSourcesReply(request, {});
    Frame generateIndexedSourcesReply(const Frame& request, const IndexedSourcesPayloadLimits& limits);
    /// Validate a complete ordinary indexed request and return a correlated mesh/failure frame.
    /// @param request Stable exact raw archive request, operation 5, request kind.
    /// @param limits Supported geometry policies, validated before generation.
    /// @return Complete independent indexed mesh or original typed failure.
    /// @throws Protocol/policy/codec or allocation errors; no partial reply.
    /// Example: auto reply = generateIndexedMeshReply(request, {});
    Frame generateIndexedMeshReply(const Frame& request, const IndexedMeshPayloadLimits& limits);
}
}
#endif

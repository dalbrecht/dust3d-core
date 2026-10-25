// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_OPERATION_H_
#define DUST3D_NATIVE_WORKER_SOURCE_OPERATION_H_
#include "WorkerOperation.h"
#include "WorkerSourcedMesh.h"
#include <variant>

namespace dust3d {
namespace worker {
    using SourcedGenerationResult = std::variant<SourcedMeshPayload, FailurePayload>;
    /// Copy one stable native geometry/source owner under independent and aggregate quotas.
    /// @param owner Borrowed owner kept alive and unchanged throughout this call.
    /// @param limits Positive supported copy/wire policies, excluding generation workspace/RSS.
    /// @return Independent complete geometry/source values; no borrowed native storage escapes.
    /// @throws Original typed codec errors or allocation exceptions; no partial pair is returned.
    /// Example: auto pair = copyNativeSourcedMesh(owner.get(), {});
    SourcedMeshPayload copyNativeSourcedMesh(const Dust3DSourcedMesh* owner, const SourcedMeshPayloadLimits& limits);
    /// Generate source-associated geometry through the existing serialized native C ABI.
    /// @param archive Complete borrowed archive bytes, held stable until return.
    /// @param limits Supported copy policies, validated before native execution. Native source
    /// retention is also bounded by sources.maximumDecodedBytes; native workspace is excluded.
    /// @return Complete owned pair or mapped original native/codec failure with bounded cause.
    /// @throws Invalid policy errors before execution; allocation of a failure may still throw.
    /// This adapter does not activate an opcode or enforce CPU, process-memory or isolation limits.
    /// Example: auto generated = generateSourcedMesh(archive, {});
    SourcedGenerationResult generateSourcedMesh(const std::vector<uint8_t>& archive, const SourcedMeshPayloadLimits& limits);
}
}
#endif

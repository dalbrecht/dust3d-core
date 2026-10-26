// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_SOURCE_BAKE_REQUEST_H_
#define DUST3D_NATIVE_WORKER_SOURCE_BAKE_REQUEST_H_
#include "WorkerLegacyRequest.h"
#include "WorkerSourceProducerNative.h"
namespace dust3d {
namespace worker {
    constexpr size_t sourceBakeRequestPrefixBytes = 96;
    /// Zero byte policies choose maintained defaults and only tighten receiver quotas.
    /// Chart/raster/level settings are independently checked against host work caps.
    struct SourceBakeRequestOptions {
        uint64_t maximumMaterialNativeBytes = 0, maximumSamplerNativeBytes = 0;
        uint64_t maximumAtlasNativeBytes = 0, maximumOutputNativeBytes = 0;
        uint64_t maximumChartCount = 0, maximumRasterSamples = 0;
        uint64_t shouldGenerateMips = 0, maximumLevels = 0;
        uint64_t maximumOutputCopyBytes = 0, maximumOutputWireBytes = 0;
    };
    struct SourceBakeRequest {
        LegacyRequest legacy;
        SourceBakeRequestOptions options;
    };
    /// Explicit-endian source prefix followed by the complete legacy settings/archive
    /// envelope. Both prefixes count against wire quota; retained quota charges archive.
    /// No native archive parsing. Throws LegacyRequestError or allocation errors.
    /// Example: auto bytes = encodeSourceBakeRequest(request, {});
    std::vector<uint8_t> encodeSourceBakeRequest(const SourceBakeRequest& value, const LegacyRequestLimits& limits);
    /// Borrowed archive equivalent; validates complete quotas before copying input.
    /// Throws original codec/allocation errors. Example: auto bytes = encodeSourceBakeRequestView(view, options, {});
    std::vector<uint8_t> encodeSourceBakeRequestView(const LegacyRequestView& legacy, const SourceBakeRequestOptions& options, const LegacyRequestLimits& limits);
    /// Inspect exact nested framing and options before retaining the opaque archive.
    /// Throws original typed codec errors. Example: auto value = decodeSourceBakeRequest(bytes.data(), bytes.size(), {});
    SourceBakeRequest decodeSourceBakeRequest(const uint8_t* bytes, size_t size, const LegacyRequestLimits& limits);
    /// Apply independent host byte/work caps; never widen receiver admission.
    /// Throws original typed policy errors; no native work. Example: auto policy = applySourceBakeRequestOptions(request, {});
    NativeSourceProducerOptions applySourceBakeRequestOptions(const SourceBakeRequest& value, const NativeSourceProducerOptions& receiver);
}
}
#endif

// SPDX-License-Identifier: MIT
#ifndef DUST3D_NATIVE_WORKER_UV_REQUEST_H_
#define DUST3D_NATIVE_WORKER_UV_REQUEST_H_
#include "WorkerProtocol.h"
#include <string>
namespace dust3d {
namespace worker {
    constexpr size_t uvRequestPrefixBytes = 32;
    constexpr size_t uvRequestImageBytes = 48;
    struct UvRequestImage {
        std::string identity;
        uint32_t width = 0, height = 0;
    };
    /// Resolved dimensions only, never pixels. Zero options retain native defaults.
    struct UvRequest {
        std::vector<uint8_t> archive;
        std::vector<UvRequestImage> images;
        uint32_t textureSize = 0;
        uint64_t metadataBudget = 0;
    };
    /// Logical output charge includes archive, image objects and identity text;
    /// excludes root objects, capacity, validation workspace and process RSS.
    struct UvRequestLimits {
        uint64_t maximumWireBytes = maximumPayloadBytes;
        uint64_t maximumDecodedBytes = maximumPayloadBytes;
        uint32_t maximumImages = 10000;
    };
    enum class UvRequestErrorCode { invalidLimits,
        invalidInput,
        truncated,
        trailingData,
        unsupportedVersion,
        invalidLayout,
        invalidOptions,
        limitExceeded };
    class UvRequestError : public std::runtime_error {
    public:
        UvRequestError(UvRequestErrorCode code, const char* message);
        UvRequestErrorCode code() const noexcept { return m_code; }

    private:
        UvRequestErrorCode m_code;
    };
    /// Validate options/identities/quotas and encode explicit v1 big-endian fields.
    /// @param request Stable borrowed values; nonempty opaque archive, texture 0..8192,
    /// metadata budget 0..256 MiB, images with nonzero UUID and dimensions 1..8192.
    /// @param limits Positive supported wire/logical/image quotas.
    /// @return Independent bytes preserving archive and UUID spelling, including braces/case.
    /// @throws UvRequestError or allocation errors; no partial output is returned.
    /// Example: auto wire = encodeUvRequest(request, {});
    std::vector<uint8_t> encodeUvRequest(const UvRequest& request, const UvRequestLimits& limits);
    /// Validate complete layout, every descriptor and semantic UUID uniqueness before copies.
    /// @param bytes Stable borrowed complete request bytes for this call.
    /// @param size Exact span; trailing data rejects, archive contents remain opaque.
    /// @param limits Positive supported quotas; not a process-memory limit.
    /// @return Independently owned archive/options; no native engine is invoked.
    /// @throws UvRequestError or allocation errors; no partial output is returned.
    /// Example: auto request = decodeUvRequest(wire.data(), wire.size(), {});
    UvRequest decodeUvRequest(const uint8_t* bytes, size_t size, const UvRequestLimits& limits);
}
}
#endif

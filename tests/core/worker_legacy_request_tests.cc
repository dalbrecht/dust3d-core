// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerLegacyRequest.h>
#include <native/Engine/WorkerWire.h>
#include <stdexcept>
using namespace dust3d::worker;
namespace {
void require(bool value, const char* cause)
{
    if (!value)
        throw std::runtime_error(cause);
}
template <typename Action>
void reject(LegacyRequestErrorCode expected, Action action)
{
    try {
        action();
    } catch (const LegacyRequestError& cause) {
        require(cause.code() == expected && cause.what()[0], "Original request cause survives");
        return;
    }
    throw std::runtime_error("Invalid legacy request accepted");
}
}
int main()
{
    try {
        LegacyRequest value { { 0, 255, 42 }, {} };
        value.options.maximumUvNativeBytes = 4096;
        value.options.maximumOutputCopyBytes = 8192;
        value.options.maximumDecodedPixelBytes = 1024;
        const auto bytes = encodeLegacyRequest(value, {});
        auto copy = decodeLegacyRequest(bytes.data(), bytes.size(), {});
        require(bytes.size() == legacyRequestPrefixBytes + 3 && copy.archive == value.archive
                && copy.options.maximumUvNativeBytes == 4096 && copy.options.maximumOutputCopyBytes == 8192
                && copy.options.maximumDecodedPixelBytes == 1024,
            "Opaque archive and independent producer budgets round trip");
        require(encodeLegacyRequestView({ value.archive.data(), value.archive.size(), value.options }, {}) == bytes, "Borrowed request encoder owns identical bytes");
        LegacyRequestLimits exact { bytes.size(), 3 };
        require(encodeLegacyRequest(copy, exact) == bytes, "Exact wire/archive limits accept");
        auto receiver = LegacyProducerOptions {};
        receiver.maximumUvNativeBytes = 2048;
        const auto policy = applyLegacyRequestOptions(copy.options, receiver);
        require(policy.maximumUvNativeBytes == 2048 && policy.output.maximumDecodedBytes == 8192
                && policy.maximumDecodedImageBytes == 1024,
            "Request budgets only tighten authoritative receiver limits");
        require(applyLegacyRequestOptions({}, {}).maximumUvNativeBytes == 64ULL * 1024 * 1024, "Zero request budgets select maintained defaults");
        const auto budgets = [](const LegacyProducerOptions& value) {
            return std::array<uint64_t, 8> { value.maximumUvNativeBytes, value.maximumBakeNativeBytes,
                value.images.maximumEncodedImageBytes, value.images.maximumEncodedTotalBytes,
                value.images.maximumDecodedBytes, value.maximumDecodedImageBytes,
                value.output.maximumDecodedBytes, value.output.maximumWireBytes };
        };
        LegacyRequestOptions tight { 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192 };
        const auto tightened = budgets(applyLegacyRequestOptions(tight, {}));
        require(tightened == std::array<uint64_t, 8> { 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192 }, "Each independent request byte quota tightens its receiver field");
        const auto tightReceiver = applyLegacyRequestOptions(tight, {});
        LegacyRequestOptions permissive { maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes,
            maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes };
        require(budgets(applyLegacyRequestOptions(permissive, tightReceiver)) == tightened, "No byte quota expands an already tighter host policy");
        auto selected = copy.options;
        selected.atlasTextureSize = 8192;
        selected.maximumSpatialGridEntries = 16000000;
        const auto supported = applyLegacyRequestOptions(selected, {});
        require(supported.atlasTextureSize == 8192 && supported.maximumSpatialGridEntries == 16000000, "Explicit supported settings survive independently from maintained default dimensions");
        for (int guard = 0; guard < 2; ++guard) {
            auto host = LegacyProducerOptions {};
            if (guard == 0)
                host.maximumPermittedTextureDimension = 8;
            if (guard == 1)
                host.maximumPermittedSpatialGridEntries = 100;
            bool rejected = false;
            try {
                (void)applyLegacyRequestOptions(selected, host);
            } catch (const LegacyProducerError& cause) {
                rejected = cause.code() == LegacyProducerErrorCode::invalidLimits;
            }
            require(rejected, "Request settings cannot override either lower authoritative host work cap");
        }
        for (int field = 0; field < 5; ++field) {
            for (uint64_t invalid : { uint64_t(0), maximumPayloadBytes + 1 }) {
                auto host = LegacyProducerOptions {};
                uint64_t* values[] = { &host.maximumDecodedImageBytes, &host.maximumUvNativeBytes, &host.maximumBakeNativeBytes,
                    &host.maximumPermittedTextureDimension, &host.maximumPermittedSpatialGridEntries };
                *values[field] = invalid;
                bool rejected = false;
                try {
                    validateLegacyProducerOptions(host);
                } catch (const LegacyProducerError& cause) {
                    rejected = cause.code() == LegacyProducerErrorCode::invalidLimits;
                }
                require(rejected, "Pure host byte/work policy guards reject before native work");
            }
        }
        auto emptyArchive = bytes;
        detail::writeUnsigned(emptyArchive.data() + 8, 0, 8);
        reject(LegacyRequestErrorCode::invalidInput, [&] { decodeLegacyRequest(emptyArchive.data(), emptyArchive.size(), {}); });
        reject(LegacyRequestErrorCode::invalidInput, [&] { encodeLegacyRequest({}, {}); });
        reject(LegacyRequestErrorCode::invalidInput, [&] { encodeLegacyRequestView({ nullptr, 3, {} }, {}); });
        for (const auto& limits : { LegacyRequestLimits { bytes.size() - 1, 3 }, LegacyRequestLimits { bytes.size(), 2 }, LegacyRequestLimits { 1, 3 } }) {
            reject(LegacyRequestErrorCode::limitExceeded, [&] { encodeLegacyRequest(value, limits); });
            reject(LegacyRequestErrorCode::limitExceeded, [&] { decodeLegacyRequest(bytes.data(), bytes.size(), limits); });
        }
        for (const auto& limits : { LegacyRequestLimits { 0, 3 }, LegacyRequestLimits { maximumPayloadBytes + 1, 3 }, LegacyRequestLimits { bytes.size(), 0 }, LegacyRequestLimits { bytes.size(), maximumPayloadBytes + 1 } })
            reject(LegacyRequestErrorCode::invalidLimits, [&] { decodeLegacyRequest(bytes.data(), bytes.size(), limits); });
        reject(LegacyRequestErrorCode::invalidInput, [&] { decodeLegacyRequest(nullptr, bytes.size(), {}); });
        for (size_t size : { size_t(0), legacyRequestPrefixBytes - 1, bytes.size() - 1 })
            reject(LegacyRequestErrorCode::truncated, [&] { decodeLegacyRequest(bytes.data(), size, {}); });
        auto extra = bytes;
        extra.push_back(0);
        reject(LegacyRequestErrorCode::trailingData, [&] { decodeLegacyRequest(extra.data(), extra.size(), {}); });
        for (size_t offset : { size_t(0), size_t(2), size_t(4) }) {
            auto bad = bytes;
            detail::writeUnsigned(bad.data() + offset, offset == 0 ? 2 : 1, offset == 4 ? 4 : 2);
            reject(offset == 0 ? LegacyRequestErrorCode::unsupportedVersion : LegacyRequestErrorCode::invalidLayout,
                [&] { decodeLegacyRequest(bad.data(), bad.size(), {}); });
        }
        for (size_t field = 0; field < 14; ++field) {
            auto bad = bytes;
            const uint64_t maximum = field >= 8 && field <= 11 ? 8192 : field == 12 ? 16000000
                                                                                    : maximumPayloadBytes;
            detail::writeUnsigned(bad.data() + 16 + field * 8, maximum + 1, 8);
            reject(LegacyRequestErrorCode::invalidOptions, [&] { decodeLegacyRequest(bad.data(), bad.size(), {}); });
        }
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

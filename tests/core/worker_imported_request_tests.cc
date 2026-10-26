// SPDX-License-Identifier: MIT
#include <iostream>
#include <native/Engine/WorkerImportedRequest.h>
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
void reject(ImportedRequestErrorCode expected, Action action)
{
    try {
        action();
    } catch (const ImportedRequestError& cause) {
        require(cause.code() == expected && cause.what()[0], "Original request cause survives");
        return;
    }
    throw std::runtime_error("Invalid imported request accepted");
}
}
int main()
{
    try {
        ImportedRequest value { { 0, 255, 42 }, {} };
        value.options.maximumMaterialNativeBytes = 4096;
        value.options.maximumSurfaceCopyBytes = 8192;
        value.options.maximumDecodedPixelBytes = 1024;
        const auto bytes = encodeImportedRequest(value, {});
        auto copy = decodeImportedRequest(bytes.data(), bytes.size(), {});
        require(bytes.size() == importedRequestPrefixBytes + 3 && copy.archive == value.archive
                && copy.options.maximumMaterialNativeBytes == 4096 && copy.options.maximumSurfaceCopyBytes == 8192
                && copy.options.maximumDecodedPixelBytes == 1024,
            "Opaque archive and independent producer budgets round trip");
        require(encodeImportedRequestView({ value.archive.data(), value.archive.size(), value.options }, {}) == bytes, "Borrowed request encoder owns identical bytes");
        ImportedRequestLimits exact { bytes.size(), 3 };
        require(encodeImportedRequest(copy, exact) == bytes, "Exact wire/archive limits accept");
        auto receiver = NativeImportedProducerLimits {};
        receiver.maximumMaterialNativeBytes = 2048;
        const auto policy = applyImportedRequestOptions(copy.options, receiver);
        require(policy.maximumMaterialNativeBytes == 2048 && policy.preparation.render.surface.maximumDecodedBytes == 8192
                && policy.preparation.render.maximumDecodedPixelBytes == 1024,
            "Request budgets only tighten authoritative receiver limits");
        require(applyImportedRequestOptions({}, {}).maximumMaterialNativeBytes == 64ULL * 1024 * 1024, "Zero request budgets select maintained defaults");
        const auto budgets = [](const NativeImportedProducerLimits& value) {
            return std::array<uint64_t, 8> { value.maximumMaterialNativeBytes, value.maximumSurfaceNativeBytes,
                value.preparation.facts.maximumDecodedBytes, value.preparation.render.surface.maximumDecodedBytes,
                value.preparation.maximumEncodedImageBytes, value.preparation.render.maximumDecodedPixelBytes,
                value.preparation.render.maximumDecodedBytes, value.preparation.render.maximumWireBytes };
        };
        ImportedRequestOptions tight { 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192 };
        const auto tightened = budgets(applyImportedRequestOptions(tight, {}));
        require(tightened == std::array<uint64_t, 8> { 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192 },
            "Every independent requested budget tightens its matching receiver field");
        const auto tightReceiver = applyImportedRequestOptions(tight, {});
        ImportedRequestOptions permissive { maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes,
            maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes, maximumPayloadBytes };
        require(budgets(applyImportedRequestOptions(permissive, tightReceiver)) == tightened,
            "No requested budget can expand an already tighter receiver");
        auto emptyArchive = bytes;
        detail::writeUnsigned(emptyArchive.data() + 8, 0, 8);
        reject(ImportedRequestErrorCode::invalidInput, [&] { decodeImportedRequest(emptyArchive.data(), emptyArchive.size(), {}); });
        reject(ImportedRequestErrorCode::invalidInput, [&] { encodeImportedRequest({}, {}); });
        reject(ImportedRequestErrorCode::invalidInput, [&] { encodeImportedRequestView({ nullptr, 3, {} }, {}); });
        for (const auto& limits : { ImportedRequestLimits { bytes.size() - 1, 3 }, ImportedRequestLimits { bytes.size(), 2 }, ImportedRequestLimits { 1, 3 } }) {
            reject(ImportedRequestErrorCode::limitExceeded, [&] { encodeImportedRequest(value, limits); });
            reject(ImportedRequestErrorCode::limitExceeded, [&] { decodeImportedRequest(bytes.data(), bytes.size(), limits); });
        }
        for (const auto& limits : { ImportedRequestLimits { 0, 3 }, ImportedRequestLimits { maximumPayloadBytes + 1, 3 }, ImportedRequestLimits { bytes.size(), 0 }, ImportedRequestLimits { bytes.size(), maximumPayloadBytes + 1 } })
            reject(ImportedRequestErrorCode::invalidLimits, [&] { decodeImportedRequest(bytes.data(), bytes.size(), limits); });
        reject(ImportedRequestErrorCode::invalidInput, [&] { decodeImportedRequest(nullptr, bytes.size(), {}); });
        for (size_t size : { size_t(0), importedRequestPrefixBytes - 1, bytes.size() - 1 })
            reject(ImportedRequestErrorCode::truncated, [&] { decodeImportedRequest(bytes.data(), size, {}); });
        auto extra = bytes;
        extra.push_back(0);
        reject(ImportedRequestErrorCode::trailingData, [&] { decodeImportedRequest(extra.data(), extra.size(), {}); });
        for (size_t offset : { size_t(0), size_t(2), size_t(4) }) {
            auto bad = bytes;
            detail::writeUnsigned(bad.data() + offset, offset == 0 ? 2 : 1, offset == 4 ? 4 : 2);
            reject(offset == 0 ? ImportedRequestErrorCode::unsupportedVersion : ImportedRequestErrorCode::invalidLayout,
                [&] { decodeImportedRequest(bad.data(), bad.size(), {}); });
        }
        for (size_t field = 0; field < 8; ++field) {
            auto bad = bytes;
            detail::writeUnsigned(bad.data() + 16 + field * 8, maximumPayloadBytes + 1, 8);
            reject(ImportedRequestErrorCode::invalidOptions, [&] { decodeImportedRequest(bad.data(), bad.size(), {}); });
        }
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

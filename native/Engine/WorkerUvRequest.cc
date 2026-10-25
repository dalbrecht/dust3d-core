// SPDX-License-Identifier: MIT
#include "WorkerUvRequest.h"
#include <algorithm>
#include <set>
#include <string_view>
namespace dust3d {
namespace worker {
    namespace {
        [[noreturn]] void reject(UvRequestErrorCode code, const char* message)
        {
            throw UvRequestError(code, message);
        }
        void policy(const UvRequestLimits& limits)
        {
            if (!limits.maximumWireBytes || limits.maximumWireBytes > maximumPayloadBytes
                || !limits.maximumDecodedBytes || limits.maximumDecodedBytes > maximumPayloadBytes
                || !limits.maximumImages || limits.maximumImages > 10000)
                reject(UvRequestErrorCode::invalidLimits, "Worker UV request limits are invalid.");
        }
        void options(uint32_t texture, uint64_t budget, uint64_t archive)
        {
            if (texture > 8192 || budget > maximumPayloadBytes || !archive)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV request options are invalid.");
        }
        std::array<uint8_t, 16> identity(std::string_view text)
        {
            if (text.size() == 38 && text.front() == '{' && text.back() == '}')
                text = text.substr(1, 36);
            if (text.size() != 36)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity is invalid.");
            std::array<uint8_t, 16> result {};
            size_t digit = 0;
            bool nonzero = false;
            for (size_t i = 0; i < text.size(); ++i) {
                if (i == 8 || i == 13 || i == 18 || i == 23) {
                    if (text[i] != '-')
                        reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity separator is invalid.");
                    continue;
                }
                const char c = text[i];
                uint8_t value;
                if (c >= '0' && c <= '9')
                    value = static_cast<uint8_t>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    value = static_cast<uint8_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    value = static_cast<uint8_t>(c - 'A' + 10);
                else
                    reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity digit is invalid.");
                result[digit / 2] = static_cast<uint8_t>((result[digit / 2] << 4) | value);
                ++digit;
                nonzero = nonzero || value != 0;
            }
            if (!nonzero)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity is zero.");
            return result;
        }
        void image(std::string_view text, uint32_t width, uint32_t height, std::set<std::array<uint8_t, 16>>& identities)
        {
            if (!width || width > 8192 || !height || height > 8192)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV image dimensions are invalid.");
            if (!identities.insert(identity(text)).second)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity is duplicated.");
        }
        void charge(uint64_t& total, uint64_t amount, uint64_t limit)
        {
            if (amount > limit || total > limit - amount)
                reject(UvRequestErrorCode::limitExceeded, "Worker UV request quota is exceeded.");
            total += amount;
        }
        void counts(uint64_t archive, uint64_t images, const UvRequestLimits& limits, uint64_t& wire, uint64_t& decoded)
        {
            if (images > limits.maximumImages)
                reject(UvRequestErrorCode::limitExceeded, "Worker UV image count is exceeded.");
            wire = uvRequestPrefixBytes;
            charge(wire, archive, limits.maximumWireBytes);
            charge(wire, images * uvRequestImageBytes, limits.maximumWireBytes);
            decoded = 0;
            charge(decoded, archive, limits.maximumDecodedBytes);
            charge(decoded, images * sizeof(UvRequestImage), limits.maximumDecodedBytes);
        }
        uint64_t read(const uint8_t* bytes, size_t width)
        {
            uint64_t value = 0;
            for (size_t i = 0; i < width; ++i)
                value = (value << 8) | bytes[i];
            return value;
        }
        void write(uint8_t* bytes, size_t width, uint64_t value)
        {
            for (size_t i = width; i > 0; --i) {
                bytes[i - 1] = static_cast<uint8_t>(value);
                value >>= 8;
            }
        }
        std::string_view imageText(const uint8_t* record)
        {
            const size_t length = record[0];
            if (length != 36 && length != 38)
                reject(UvRequestErrorCode::invalidOptions, "Worker UV image identity length is invalid.");
            for (size_t i = length + 1; i < 40; ++i)
                if (record[i])
                    reject(UvRequestErrorCode::invalidLayout, "Worker UV image padding is nonzero.");
            return { reinterpret_cast<const char*>(record + 1), length };
        }
    }
    UvRequestError::UvRequestError(UvRequestErrorCode code, const char* message)
        : std::runtime_error(message)
        , m_code(code)
    {
    }
    namespace {
        uint64_t requestWireSize(const UvRequest& request, const UvRequestLimits& limits)
        {
            policy(limits);
            options(request.textureSize, request.metadataBudget, request.archive.size());
            uint64_t wire, decoded;
            counts(request.archive.size(), request.images.size(), limits, wire, decoded);
            std::set<std::array<uint8_t, 16>> identities;
            for (const auto& entry : request.images) {
                image(entry.identity, entry.width, entry.height, identities);
                charge(decoded, entry.identity.size(), limits.maximumDecodedBytes);
            }
            return wire;
        }
    }
    void validateUvRequest(const UvRequest& request, const UvRequestLimits& limits)
    {
        (void)requestWireSize(request, limits);
    }
    std::vector<uint8_t> encodeUvRequest(const UvRequest& request, const UvRequestLimits& limits)
    {
        const auto wire = requestWireSize(request, limits);
        std::vector<uint8_t> result(static_cast<size_t>(wire), 0);
        write(result.data(), 2, 1);
        write(result.data() + 2, 2, uvRequestPrefixBytes);
        write(result.data() + 8, 4, request.textureSize);
        write(result.data() + 12, 4, request.images.size());
        write(result.data() + 16, 8, request.metadataBudget);
        write(result.data() + 24, 8, request.archive.size());
        size_t offset = uvRequestPrefixBytes;
        for (const auto& entry : request.images) {
            auto* record = result.data() + offset;
            record[0] = static_cast<uint8_t>(entry.identity.size());
            std::copy(entry.identity.begin(), entry.identity.end(), record + 1);
            write(record + 40, 4, entry.width);
            write(record + 44, 4, entry.height);
            offset += uvRequestImageBytes;
        }
        std::copy(request.archive.begin(), request.archive.end(), result.begin() + offset);
        return result;
    }
    UvRequest decodeUvRequest(const uint8_t* bytes, size_t size, const UvRequestLimits& limits)
    {
        policy(limits);
        if (!bytes)
            reject(UvRequestErrorCode::invalidInput, "Worker UV request input is null.");
        if (size > limits.maximumWireBytes)
            reject(UvRequestErrorCode::limitExceeded, "Worker UV request wire quota is exceeded.");
        if (size < uvRequestPrefixBytes)
            reject(UvRequestErrorCode::truncated, "Worker UV request prefix is truncated.");
        if (read(bytes, 2) != 1)
            reject(UvRequestErrorCode::unsupportedVersion, "Worker UV request version is unsupported.");
        if (read(bytes + 2, 2) != uvRequestPrefixBytes || read(bytes + 4, 4))
            reject(UvRequestErrorCode::invalidLayout, "Worker UV request layout is invalid.");
        const auto texture = static_cast<uint32_t>(read(bytes + 8, 4));
        const auto images = static_cast<uint32_t>(read(bytes + 12, 4));
        const auto budget = read(bytes + 16, 8);
        const auto archive = read(bytes + 24, 8);
        options(texture, budget, archive);
        uint64_t wire, decoded;
        counts(archive, images, limits, wire, decoded);
        if (wire > size)
            reject(UvRequestErrorCode::truncated, "Worker UV request body is truncated.");
        if (wire < size)
            reject(UvRequestErrorCode::trailingData, "Worker UV request has trailing data.");
        std::set<std::array<uint8_t, 16>> identities;
        size_t offset = uvRequestPrefixBytes;
        for (uint32_t i = 0; i < images; ++i, offset += uvRequestImageBytes) {
            const auto* record = bytes + offset;
            const auto text = imageText(record);
            image(text, static_cast<uint32_t>(read(record + 40, 4)), static_cast<uint32_t>(read(record + 44, 4)), identities);
            charge(decoded, text.size(), limits.maximumDecodedBytes);
        }
        UvRequest result;
        result.textureSize = texture;
        result.metadataBudget = budget;
        result.images.reserve(images);
        for (uint32_t i = 0; i < images; ++i) {
            const auto* record = bytes + uvRequestPrefixBytes + i * uvRequestImageBytes;
            result.images.push_back({ std::string(imageText(record)), static_cast<uint32_t>(read(record + 40, 4)), static_cast<uint32_t>(read(record + 44, 4)) });
        }
        result.archive.assign(bytes + offset, bytes + size);
        return result;
    }
}
}

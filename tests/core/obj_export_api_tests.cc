// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vector>
namespace {
void require(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
}
int main(int argc, char** argv)
{
    try {
        require(argc == 2, "Fixture directory required");
        std::ifstream file(std::string(argv[1]) + "/Capsule.ds3", std::ios::binary);
        require(bool(file), "Capsule fixture missing");
        const std::vector<uint8_t> bytes { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
        Dust3DError error {};
        Dust3DObjExportOptions settings {};
        settings.struct_size = sizeof(settings);
        using Owner = std::unique_ptr<Dust3DObjExport, decltype(&dust3d_obj_export_destroy)>;
        require(!dust3d_obj_export_bytes(nullptr) && dust3d_obj_export_byte_count(nullptr) == 0
                && dust3d_obj_export_payload_bytes(nullptr) == 0 && !*dust3d_obj_export_warning(nullptr),
            "Null accessors");
        dust3d_obj_export_destroy(nullptr);
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), nullptr, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Options required");
        --settings.struct_size;
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Sized options required");
        settings.struct_size = sizeof(settings);
        settings.maximum_output_payload_bytes = 1;
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error)
                && error.status == DUST3D_STATUS_LIMIT_EXCEEDED,
            "Quota admission");
        settings.maximum_output_payload_bytes = 0;
        Owner owner(dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_obj_export_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, "Ordinary original OBJ generation");
        const auto count = dust3d_obj_export_byte_count(owner.get());
        const auto* buffer = dust3d_obj_export_bytes(owner.get());
        require(buffer && count > 100 && std::memcmp(buffer, "# Dust3D", 8) == 0, "Owned text header");
        const std::vector<uint8_t> copy(buffer, buffer + count);
        settings.maximum_output_payload_bytes = dust3d_obj_export_payload_bytes(owner.get());
        Owner exact(dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_obj_export_destroy);
        require(exact && dust3d_obj_export_payload_bytes(exact.get()) <= settings.maximum_output_payload_bytes, "Exact quota");
        --settings.maximum_output_payload_bytes;
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error)
                && error.status == DUST3D_STATUS_LIMIT_EXCEEDED,
            "One-byte-short quota");
        const auto* retainedFirst = dust3d_obj_export_bytes(owner.get());
        require(dust3d_obj_export_byte_count(owner.get()) == count && retainedFirst
                && std::equal(copy.begin(), copy.end(), retainedFirst),
            "Later export success/rejection invalidated the first owner's bytes");
        owner.reset();
        const auto* retainedSecond = dust3d_obj_export_bytes(exact.get());
        require(copy.size() == count && dust3d_obj_export_byte_count(exact.get()) == count && retainedSecond
                && std::equal(copy.begin(), copy.end(), retainedSecond),
            "Destroying the first owner invalidated independent output bytes");
        settings.maximum_output_payload_bytes = 0;
        const uint8_t invalid[] { 1, 2, 3 };
        Owner malformed(dust3d_obj_export_load(invalid, sizeof(invalid), &settings, &error), dust3d_obj_export_destroy);
        require(!malformed && error.status == DUST3D_STATUS_INVALID_DOCUMENT && *error.message, "Original archive diagnostic");
        std::unique_ptr<Dust3DCancellationToken, decltype(&dust3d_cancellation_token_destroy)> token(
            dust3d_cancellation_token_create(&error), dust3d_cancellation_token_destroy);
        require(bool(token), "Owned cancellation token");
        settings.cancellation = token.get();
        Owner cooperative(dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error), dust3d_obj_export_destroy);
        require(cooperative && error.status == DUST3D_STATUS_OK, "Uncancelled token");
        dust3d_cancellation_token_cancel(token.get());
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), &settings, &error)
                && error.status == DUST3D_STATUS_CANCELLED && *error.message,
            "Cooperative cancellation");
        require(!dust3d_obj_export_load(bytes.data(), bytes.size(), nullptr, nullptr), "Optional diagnostic");
        std::cout << "OBJ API contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

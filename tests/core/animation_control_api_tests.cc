// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
}
int main()
{
    try {
        using Owner = std::unique_ptr<Dust3DAnimationCatalog, decltype(&dust3d_animation_catalog_destroy)>;
        Dust3DError error {};
        Dust3DAnimationControlInfo control {};
        require(dust3d_animation_catalog_control_count(nullptr, 0) == 0
                && !dust3d_animation_catalog_control_at(nullptr, 0, 0, &control),
            "Null control owner");
        Owner owner(dust3d_animation_catalog_load(&error), dust3d_animation_catalog_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, "Owned control catalog construction");
        size_t total = 0;
        size_t wingClip = SIZE_MAX;
        size_t wingControl = SIZE_MAX;
        for (size_t clipIndex = 0; clipIndex < dust3d_animation_catalog_clip_count(owner.get()); ++clipIndex) {
            Dust3DAnimationClipInfo clip {};
            require(dust3d_animation_catalog_clip_at(owner.get(), clipIndex, &clip), "Clip view");
            const auto count = dust3d_animation_catalog_control_count(owner.get(), clipIndex);
            total += count;
            for (size_t index = 0; index < count; ++index) {
                require(dust3d_animation_catalog_control_at(owner.get(), clipIndex, index, &control), "Owned control view");
                require(control.name && *control.name && control.display_name && *control.display_name
                        && std::isfinite(control.default_parameter_value) && control.slider_divisor > 0
                        && control.parameter_multiplier > 0 && control.minimum_slider_value <= control.maximum_slider_value,
                    "Control identity/scalars");
                double value = 7;
                require(dust3d_animation_catalog_control_value(owner.get(), clipIndex, index,
                            control.minimum_slider_value, &value, &error)
                        && error.status == DUST3D_STATUS_OK && std::isfinite(value),
                    "Checked endpoint value transport");
                int slider = 7;
                require(dust3d_animation_catalog_control_slider(owner.get(), clipIndex, index,
                            control.default_parameter_value, &slider, &error)
                        && slider == control.default_slider_value && error.status == DUST3D_STATUS_OK,
                    "Checked default slider transport");
                if (std::strcmp(clip.type, "BirdGlide") == 0 && std::strcmp(control.name, "wingSpreadAngle") == 0) {
                    wingClip = clipIndex;
                    wingControl = index;
                }
            }
            control.name = "sentinel";
            require(!dust3d_animation_catalog_control_at(owner.get(), clipIndex, count, &control)
                    && !control.name && control.default_parameter_value == 0,
                "Control bounds clear output");
        }
        require(total == 1592 && wingClip != SIZE_MAX, "Real schema inventory");
        int slider = 123;
        require(!dust3d_animation_catalog_control_slider(owner.get(), wingClip, wingControl,
                    std::numeric_limits<double>::quiet_NaN(), &slider, &error)
                && slider == 0 && error.status == DUST3D_STATUS_INVALID_DOCUMENT && std::string(error.message).find("must be finite") != std::string::npos,
            "Nonfinite input preserves cause without conversion");
        require(!dust3d_animation_catalog_control_slider(owner.get(), wingClip, wingControl, 1e20, &slider, &error)
                && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Representability rejection stays inside C ABI");
        double value = 123;
        require(!dust3d_animation_catalog_control_value(owner.get(), wingClip, wingControl, -1, &value, &error)
                && value == 0 && error.status == DUST3D_STATUS_INVALID_DOCUMENT,
            "Widget range rejection");
        require(!dust3d_animation_catalog_control_value(owner.get(), wingClip, wingControl, 100, nullptr, &error)
                && !dust3d_animation_catalog_control_slider(owner.get(), wingClip, wingControl, 0.15, nullptr, &error),
            "Null destinations reject");
        require(!dust3d_animation_catalog_control_at(owner.get(), 0, 0, nullptr)
                && dust3d_animation_catalog_control_count(owner.get(), SIZE_MAX) == 0
                && !dust3d_animation_catalog_control_value(nullptr, 0, 0, 0, &value, nullptr)
                && !dust3d_animation_catalog_control_slider(owner.get(), SIZE_MAX, 0, 0, &slider, nullptr),
            "Owner/index admission");
        require(dust3d_animation_catalog_control_value(owner.get(), wingClip, wingControl, 100, &value, &error)
                && value == 100.0 / 667.0 && error.status == DUST3D_STATUS_OK && !*error.message,
            "Successful conversion clears earlier diagnostic");
        require(dust3d_animation_catalog_control_at(owner.get(), wingClip, wingControl, &control), "Copied control identity");
        const std::string copiedLabel(control.display_name);
        Owner second(dust3d_animation_catalog_load(nullptr), dust3d_animation_catalog_destroy);
        require(bool(second), "Independent control owner");
        owner.reset();
        require(dust3d_animation_catalog_control_at(second.get(), wingClip, wingControl, &control)
                && copiedLabel == control.display_name,
            "Control storage is independently owned");
        std::cout << "Owned animation control C API contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

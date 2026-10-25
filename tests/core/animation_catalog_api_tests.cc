// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <cmath>
#include <cstring>
#include <iostream>
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
        Dust3DAnimationClipInfo clip {};
        Dust3DAnimationParameterInfo parameter {};
        require(dust3d_animation_catalog_clip_count(nullptr) == 0 && dust3d_animation_catalog_rig_count(nullptr) == 0,
            "Null owners must have no rows");
        require(!dust3d_animation_catalog_clip_at(nullptr, 0, &clip)
                && !dust3d_animation_catalog_parameter_at(nullptr, 0, 0, &parameter),
            "Null owners must reject borrowed views");
        require(!dust3d_animation_catalog_rig_name(nullptr, 0)
                && !dust3d_animation_catalog_bone_name(nullptr, 0, 0)
                && dust3d_animation_catalog_bone_count(nullptr, 0) == 0,
            "Null owner rig views must be empty");
        dust3d_animation_catalog_destroy(nullptr);
        Dust3DError error {};
        Owner first(dust3d_animation_catalog_load(&error), dust3d_animation_catalog_destroy);
        require(first && error.status == DUST3D_STATUS_OK && !*error.message, "Owned catalog construction");
        require(dust3d_animation_catalog_clip_count(first.get()) == 180, "Inherited catalog membership");
        size_t aliases = 0;
        for (size_t index = 0; index < dust3d_animation_catalog_clip_count(first.get()); ++index) {
            require(dust3d_animation_catalog_clip_at(first.get(), index, &clip), "Borrow clip view");
            require(clip.type && *clip.type && clip.rig && *clip.rig && clip.generator && *clip.generator,
                "Owned clip identity");
            require(std::isfinite(clip.duration_seconds) && clip.duration_seconds > 0 && clip.sample_count > 0,
                "Finite clip timing metadata");
            require((clip.loops == 0 || clip.loops == 1) && (clip.is_legacy_alias == 0 || clip.is_legacy_alias == 1),
                "Boolean metadata layout");
            aliases += clip.is_legacy_alias;
            for (size_t value = 0; value < clip.parameter_count; ++value) {
                require(dust3d_animation_catalog_parameter_at(first.get(), index, value, &parameter)
                        && parameter.name && *parameter.name && std::isfinite(parameter.value),
                    "Owned finite default override");
            }
            parameter.name = "sentinel";
            parameter.value = 7;
            require(!dust3d_animation_catalog_parameter_at(first.get(), index, clip.parameter_count, &parameter)
                    && !parameter.name && parameter.value == 0,
                "Parameter bounds clear output");
        }
        require(aliases == 4 && dust3d_animation_catalog_rig_count(first.get()) == 7, "Alias/rig inventory");
        require(!dust3d_animation_catalog_clip_at(first.get(), 0, nullptr)
                && !dust3d_animation_catalog_parameter_at(first.get(), 0, 0, nullptr),
            "Null destination views");
        require(!dust3d_animation_catalog_clip_at(first.get(), SIZE_MAX, &clip) && !clip.type
                && !dust3d_animation_catalog_parameter_at(first.get(), SIZE_MAX, 0, &parameter),
            "Out-of-range clip views");
        for (size_t rig = 0; rig < dust3d_animation_catalog_rig_count(first.get()); ++rig) {
            require(dust3d_animation_catalog_rig_name(first.get(), rig), "Owned rig identity");
            const auto count = dust3d_animation_catalog_bone_count(first.get(), rig);
            require(count > 0, "Known rig bone metadata");
            for (size_t bone = 0; bone < count; ++bone)
                require(dust3d_animation_catalog_bone_name(first.get(), rig, bone), "Owned bone identity");
            require(!dust3d_animation_catalog_bone_name(first.get(), rig, count), "Bone bounds");
        }
        require(!dust3d_animation_catalog_rig_name(first.get(), SIZE_MAX)
                && !dust3d_animation_catalog_bone_name(first.get(), SIZE_MAX, 0)
                && dust3d_animation_catalog_bone_count(first.get(), SIZE_MAX) == 0,
            "Rig bounds");
        require(dust3d_animation_catalog_clip_at(first.get(), 0, &clip), "First copied identity");
        const std::string copiedType(clip.type);
        Owner second(dust3d_animation_catalog_load(nullptr), dust3d_animation_catalog_destroy);
        require(bool(second), "Optional error output");
        first.reset();
        require(dust3d_animation_catalog_clip_at(second.get(), 0, &clip) && copiedType == clip.type,
            "Independent owner/copy lifetime");
        std::cout << "Owned animation catalog C API contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

// SPDX-License-Identifier: MIT
#include <Dust3DBridge.h>
#include <array>
#include <dust3d/animation/sound_generator.h>
#include <dust3d/animation/surface_material.h>
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
        const std::array<std::string, 8> names { "Stone", "Wood", "Sand", "Metal", "Grass", "Water", "Dirt", "Snow" };
        const auto& materials = dust3d::surfaceMaterials();
        require(materials.size() == names.size(), "Reference material count changed");
        for (size_t index = 0; index < names.size(); ++index) {
            require(dust3d::surfaceMaterialName(materials[index]) == names[index]
                    && dust3d::surfaceMaterialFromName(names[index]) == materials[index],
                "Reference material order or conversion changed");
        }
        require(dust3d::surfaceMaterialFromName("wood") == dust3d::SurfaceMaterial::Stone
                && dust3d::surfaceMaterialFromName("future surface") == dust3d::SurfaceMaterial::Stone,
            "Inherited exact-name/fallback semantics changed");
        Dust3DError error {};
        std::unique_ptr<Dust3DAnimationCatalog, decltype(&dust3d_animation_catalog_destroy)> owner(
            dust3d_animation_catalog_load(&error), dust3d_animation_catalog_destroy);
        require(owner && error.status == DUST3D_STATUS_OK, "Owned catalog load failed");
        require(dust3d_animation_catalog_surface_material_count(owner.get()) == names.size(), "Owned material count changed");
        std::array<std::string, 8> copies;
        for (size_t index = 0; index < names.size(); ++index) {
            const auto* name = dust3d_animation_catalog_surface_material_name(owner.get(), index);
            require(name && name == names[index], "Owned material name changed");
            copies[index] = name;
        }
        require(dust3d_animation_catalog_surface_material_count(nullptr) == 0
                && !dust3d_animation_catalog_surface_material_name(nullptr, 0)
                && !dust3d_animation_catalog_surface_material_name(owner.get(), names.size()),
            "Material null/index boundary changed");
        owner.reset();
        require(copies == names, "Copied material names changed after owner destruction");
        std::cout << "Generator-free audio surface metadata contracts passed\n";
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

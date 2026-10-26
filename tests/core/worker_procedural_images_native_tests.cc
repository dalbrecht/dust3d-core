// SPDX-License-Identifier: MIT
#include <fstream>
#include <iostream>
#include <iterator>
#include <native/Engine/WorkerProceduralImagesNative.h>
#include <stdexcept>
using namespace dust3d::worker;
int main(int argc, char** argv)
{
    try {
        if (argc != 2)
            throw std::runtime_error("Existing fixture directory required");
        for (const char* name : { "Capsule.ds3", "ImportedSurface.ds3" }) {
            std::ifstream input(std::string(argv[1]) + '/' + name, std::ios::binary);
            if (!input)
                throw std::runtime_error("Existing positive archive opens");
            const std::vector<uint8_t> archive { std::istreambuf_iterator<char>(input), {} };
            if (!loadProceduralImageInputs(archive, {}).empty())
                throw std::runtime_error("These existing fixtures have no procedural image references");
        }
        std::ifstream textured(std::string(argv[1]) + "/ProceduralImage.ds3", std::ios::binary);
        std::ifstream png(std::string(argv[1]) + "/ImportedMaterial.png", std::ios::binary);
        if (!textured || !png)
            throw std::runtime_error("Positive encoded-image fixtures open");
        std::vector<uint8_t> archive { std::istreambuf_iterator<char>(textured), {} };
        const std::vector<uint8_t> expected { std::istreambuf_iterator<char>(png), {} };
        const auto copied = loadProceduralImageInputs(archive, {});
        archive.clear();
        if (copied.size() != 1 || copied[0].identity != "{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}"
            || copied[0].assetName != "images/{eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee}.png" || copied[0].encoded != expected)
            throw std::runtime_error("Native metadata extraction returns exact independently owned referenced PNG bytes");
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
    return 0;
}

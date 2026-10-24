// SPDX-License-Identifier: MIT
#include "EngineInternal.h"
#include "PngEncoding.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace dust3d;
using namespace dust3d::native;
namespace {
void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <typename Operation>
void rejects(Operation operation, Dust3DStatus status)
{
    try {
        operation();
    } catch (const DocumentError& cause) {
        require(cause.status() == status, "PNG rejection lost typed cause");
        return;
    }
    throw std::runtime_error("Invalid PNG input accepted");
}
}
int main(int argc, char** argv)
{
    try {
        const auto plan = planPngEncoding(8192, 8192, 268435456);
        require(plan.rowBytes == 32768 && plan.pixelBytes == 268435456, "Numeric-only image bounds changed");
        rejects([] { planPngEncoding(0, 2, 0); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planPngEncoding(2, 0, 0); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planPngEncoding(8193, 2, 0); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planPngEncoding(std::numeric_limits<size_t>::max(), 2, 0); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planPngEncoding(2, 2, 15); }, DUST3D_STATUS_INVALID_DOCUMENT);
        rejects([] { planPngEncoding(2, 2, 16, 67108865); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        rejects([] { planPngEncoding(2, 2, 16, 1); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        TextureImage image { 2, 2, { 255, 0, 0, 128, 0, 255, 0, 64, 0, 0, 255, 32, 1, 2, 3, 0 } };
        const auto pixels = image.pixels;
        const auto encoded = encodePng(image);
        require(encoded.size() > 57 && encoded[0] == 137 && encoded[1] == 'P', "PNG signature missing");
        require(image.pixels == pixels && encodePng(image) == encoded, "PNG changed source or is nondeterministic within runtime");
        rejects([&] { encodePng(image, 57); }, DUST3D_STATUS_LIMIT_EXCEEDED);
        TextureImage large { 128, 129, {} };
        large.pixels.resize(large.width * large.height * 4);
        uint32_t state = 123456789;
        for (auto& pixel : large.pixels) {
            state = state * uint32_t(1664525) + uint32_t(1013904223);
            pixel = static_cast<uint8_t>(state >> 24);
        }
        const auto largerEncoded = encodePng(large);
        require(largerEncoded.size() > 65536, "Ordinary multi-block image fixture did not exercise streamed output");
        if (argc == 2 || argc == 3) {
            const auto& bytes = argc == 2 ? encoded : largerEncoded;
            std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            output.close();
        }
        std::cout << "Bounded RGBA PNG encoding passed\n";
        return 0;
    } catch (const std::exception& cause) {
        std::cerr << cause.what() << '\n';
        return 1;
    }
}

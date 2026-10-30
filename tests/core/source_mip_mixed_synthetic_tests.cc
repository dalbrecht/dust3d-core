// SPDX-License-Identifier: MIT
// Caller-owned synthetic mixed archive; no fixture or inherited writer is used.
#include <Dust3DBridge.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& cause)
{
    if (!condition)
        throw std::runtime_error(cause);
}
void appendWord(std::vector<uint8_t>& bytes, uint32_t value)
{
    for (unsigned int shift : { 0u, 8u, 16u, 24u })
        bytes.push_back(static_cast<uint8_t>((value >> shift) & 0xff));
}
void appendFloat(std::vector<uint8_t>& bytes, float value)
{
    static_assert(sizeof(float) == sizeof(uint32_t));
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    appendWord(bytes, bits);
}
std::vector<uint8_t> blueTriangleGlb()
{
    std::vector<uint8_t> binary;
    for (float value : { -.5f, 0.f, 0.f, .5f, 1.f, 0.f, 0.f, .5f, .5f,
             0.f, 0.f, 1.f, 0.f, .5f, 1.f })
        appendFloat(binary, value);
    require(binary.size() == 60, "unexpected synthetic GLB binary length");
    std::string json = R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":60}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[-0.5,0,0],"max":[0.5,1,0.5]},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"material":0}]}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0,0,1,1]}}]})";
    while (json.size() % 4)
        json.push_back(' ');
    std::vector<uint8_t> glb;
    appendWord(glb, 0x46546c67);
    appendWord(glb, 2);
    appendWord(glb, static_cast<uint32_t>(28 + json.size() + binary.size()));
    appendWord(glb, static_cast<uint32_t>(json.size()));
    appendWord(glb, 0x4e4f534a);
    glb.insert(glb.end(), json.begin(), json.end());
    appendWord(glb, static_cast<uint32_t>(binary.size()));
    appendWord(glb, 0x004e4942);
    glb.insert(glb.end(), binary.begin(), binary.end());
    return glb;
}
std::vector<uint8_t> mixedArchive()
{
    const std::string imported = "{00000100-0000-4000-8000-000000000100}";
    const std::string model = "{00000300-0000-4000-8000-000000000300}";
    const std::string procedural = "{00000101-0000-4000-8000-000000000101}";
    const std::string xml = "<canvas originX='.5' originY='.5' originZ='.5'><nodes>"
                            "<node id='{00001000-0000-4000-8000-000000001000}' partId='"
        + imported + "' x='.5' y='.2' z='.5' radius='.15'/>"
                     "<node id='{00001001-0000-4000-8000-000000001001}' partId='"
        + imported + "' x='.5' y='.8' z='.5' radius='.15'/>"
                     "<node id='{00001002-0000-4000-8000-000000001002}' partId='"
        + procedural + "' x='2' y='.2' z='.5' radius='.15'/>"
                       "<node id='{00001003-0000-4000-8000-000000001003}' partId='"
        + procedural + "' x='2' y='.8' z='.5' radius='.15'/>"
                       "</nodes><edges>"
                       "<edge id='{00002000-0000-4000-8000-000000002000}' partId='"
        + imported + "' from='{00001000-0000-4000-8000-000000001000}' to='{00001001-0000-4000-8000-000000001001}'/>"
                     "<edge id='{00002001-0000-4000-8000-000000002001}' partId='"
        + procedural + "' from='{00001002-0000-4000-8000-000000001002}' to='{00001003-0000-4000-8000-000000001003}'/>"
                       "</edges><parts><part id='"
        + imported + "' target='ImportedModel' importedModelId='" + model + "' interpolated='false'/>"
                                                                            "<part id='"
        + procedural + "' interpolated='false'/></parts><components>"
                       "<component id='{00000200-0000-4000-8000-000000000200}' linkDataType='partId' linkData='"
        + imported + "' combineMode='Uncombined'/>"
                     "<component id='{00000201-0000-4000-8000-000000000201}' linkDataType='partId' linkData='"
        + procedural + "' color='#ff0000' combineMode='Uncombined'/>"
                       "</components></canvas>";
    const auto glb = blueTriangleGlb();
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size())
        + "'/><asset name='models/" + model + ".glb' offset='" + std::to_string(xml.size())
        + "' size='" + std::to_string(glb.size()) + "'/></ds3>";
    std::ostringstream text;
    text << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
         << header << xml;
    const auto envelope = text.str();
    std::vector<uint8_t> archive(envelope.begin(), envelope.end());
    archive.insert(archive.end(), glb.begin(), glb.end());
    return archive;
}
double sample(const Dust3DTextureView& view, double u, double v, size_t channel)
{
    require(view.pixels && view.width && view.height && view.width <= 256 && view.height <= 256
            && view.row_stride <= 256 * 4 && view.byte_count <= 256 * 256 * 4,
        "mip view dimensions and storage must fit the fixture bounds");
    require(view.row_stride >= view.width * 4
            && view.byte_count >= (view.height - 1) * view.row_stride + view.width * 4,
        "mip view must contain its full RGBA storage");
    const double x = u * view.width - .5, y = v * view.height - .5;
    const double left = std::floor(x), top = std::floor(y);
    double value = 0;
    for (size_t row = 0; row < 2; ++row)
        for (size_t col = 0; col < 2; ++col) {
            const auto sx = static_cast<size_t>(std::clamp(left + col, 0.0, double(view.width - 1)));
            const auto sy = static_cast<size_t>(std::clamp(top + row, 0.0, double(view.height - 1)));
            value += view.pixels[sy * view.row_stride + sx * 4 + channel]
                * (col ? x - left : 1 - (x - left)) * (row ? y - top : 1 - (y - top));
        }
    return value;
}
void mixedMipLoadsAndSamples()
{
    auto archive = mixedArchive();
    Dust3DSourceBakeMipOptions options {};
    options.struct_size = sizeof(options);
    options.maximum_levels = 8;
    options.source.struct_size = sizeof(options.source);
    options.source.maximum_output_payload_bytes = 32 * 1024 * 1024;
    options.source.maximum_atlas_payload_bytes = 16 * 1024 * 1024;
    options.source.maximum_sampler_payload_bytes = 16 * 1024 * 1024;
    options.source.maximum_raster_samples = 10000000;
    options.source.maximum_chart_count = 100;
    options.source.legacy.struct_size = sizeof(options.source.legacy);
    options.source.legacy.atlas_texture_size = 256;
    options.source.legacy.color_size = 256;
    options.source.legacy.material_size = 256;
    options.source.legacy.normal_size = 256;
    options.source.legacy.maximum_uv_payload_bytes = 16 * 1024 * 1024;
    options.source.legacy.maximum_bake_payload_bytes = 16 * 1024 * 1024;
    Dust3DError error {};
    using Owner = std::unique_ptr<Dust3DSourceBakedMesh, decltype(&dust3d_source_baked_mesh_destroy)>;
    Owner owner(dust3d_source_baked_mesh_load_with_mips(archive.data(), archive.size(), &options, &error), dust3d_source_baked_mesh_destroy);
    require(bool(owner), std::string("source mip load failed: ") + error.message);
    std::vector<uint8_t>().swap(archive);
    const auto count = dust3d_source_baked_mesh_triangle_count(owner.get());
    const auto* triangles = dust3d_source_baked_mesh_triangles(owner.get());
    require(count && triangles && dust3d_source_baked_mesh_model_count(owner.get()) == 1, "mixed source records required");
    std::array<const Dust3DSourceBakedTriangle*, 2> selected {};
    for (size_t i = 0; i < count; ++i) {
        const auto& triangle = triangles[i];
        require(triangle.uv_known && triangle.policy_known, "each retained triangle needs known source UV and policy");
        for (const auto& uv : triangle.uv)
            require(std::isfinite(uv[0]) && std::isfinite(uv[1]) && uv[0] >= 0 && uv[0] <= 1 && uv[1] >= 0 && uv[1] <= 1, "UVs must be finite and normalized");
        const size_t kind = triangle.model_index == SIZE_MAX ? 0 : 1;
        if (kind)
            require(triangle.model_index == 0, "imported triangle must belong to the fixture model");
        if (!selected[kind])
            selected[kind] = &triangle;
    }
    require(selected[0] && selected[1], "both procedural and imported triangles are required");
    const auto levels = dust3d_source_baked_mesh_texture_level_count(owner.get(), DUST3D_TEXTURE_COLOR);
    const auto lod = dust3d_source_baked_mesh_maximum_sampling_lod(owner.get());
    require(std::isfinite(lod) && lod >= 1 && lod <= double(options.maximum_levels - 1)
            && levels >= 2 && levels <= options.maximum_levels,
        "positive bounded mip levels required");
    require(static_cast<size_t>(lod) < levels, "LOD must fit retained levels");
    const auto* geometry = dust3d_source_baked_mesh_geometry(owner.get());
    require(geometry, "owned geometry required");
    const std::string warning = dust3d_mesh_warning(geometry);
    for (const auto* fallback : { "Mip atlas padding could not be packed", "Known procedural mip triangle has no single owning island", "Base mip footprint has no owned chart coverage" })
        require(warning.find(fallback) == std::string::npos, "mip fallback diagnostic: " + warning);
    for (size_t kind = 0; kind < selected.size(); ++kind) {
        const auto& uv = selected[kind]->uv;
        const double u = (uv[0][0] + uv[1][0] + uv[2][0]) / 3;
        const double v = (uv[0][1] + uv[1][1] + uv[2][1]) / 3;
        for (size_t level = 0; level <= static_cast<size_t>(lod); ++level) {
            const auto view = dust3d_source_baked_mesh_texture_level(owner.get(), DUST3D_TEXTURE_COLOR, level);
            const double wanted = sample(view, u, v, kind ? 2 : 0);
            const double unwanted = sample(view, u, v, kind ? 0 : 2);
            require(wanted > 245 && unwanted < 10 && sample(view, u, v, 1) < 10 && sample(view, u, v, 3) > 245,
                "bilinear chart color must remain isolated at every admitted level");
            if (level) {
                const auto previous = dust3d_source_baked_mesh_texture_level(owner.get(), DUST3D_TEXTURE_COLOR, level - 1);
                for (double fraction : { .25, .5, .75 }) {
                    const auto mixed = sample(previous, u, v, kind ? 0 : 2) * (1 - fraction) + unwanted * fraction;
                    require(mixed < 10, "fractional trilinear sampling must exclude the other chart");
                }
            }
        }
    }
}
}
int main()
{
    try {
        mixedMipLoadsAndSamples();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

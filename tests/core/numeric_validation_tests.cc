#include <Dust3DBridge.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
using MeshOwner = std::unique_ptr<Dust3DMesh, decltype(&dust3d_mesh_destroy)>;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

std::string model()
{
    return "<canvas originX='0.5' originY='0.5' originZ='0.5'><nodes>"
           "<node id='{11111111-1111-4111-8111-111111111111}' partId='{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}' x='0.5' y='0.5' z='0.0' radius='0.1'/>"
           "<node id='{22222222-2222-4222-8222-222222222222}' partId='{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}' x='0.5' y='0.6' z='1.0' radius='0.15'/>"
           "</nodes><edges><edge id='{33333333-3333-4333-8333-333333333333}' partId='{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}' from='{11111111-1111-4111-8111-111111111111}' to='{22222222-2222-4222-8222-222222222222}'/></edges>"
           "<parts><part id='{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}' cutRotation='0.25' deformWidth='0.8' deformThickness='0.6'/></parts>"
           "<components><component id='{bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb}' linkDataType='partId' linkData='{aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa}' color='#3366cc' smoothCutoffDegrees='60.0' backCloseDepthRatio='0.5' backCloseSharpness='0.2' wrapOffset='0.1' wrapWrinkles='0.2'/></components></canvas>";
}

std::string archive(const std::string& xml)
{
    const std::string prefix = "DUST3D 1.0 xml ";
    const std::string header = "<ds3><model name='model.xml' offset='0' size='" + std::to_string(xml.size()) + "'/></ds3>\n";
    std::ostringstream stream;
    stream << prefix << std::setfill('0') << std::setw(10) << prefix.size() + 12 + header.size() << "\r\n"
           << header << xml;
    return stream.str();
}

MeshOwner load(const std::string& xml)
{
    const std::string bytes = archive(xml);
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &error), dust3d_mesh_destroy);
    require(mesh != nullptr && error.status == DUST3D_STATUS_OK, std::string("Valid numeric fixture failed: ") + error.message);
    return mesh;
}

void reject(const std::string& xml)
{
    const std::string bytes = archive(xml);
    Dust3DError error {};
    MeshOwner mesh(dust3d_mesh_load(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(), &error), dust3d_mesh_destroy);
    require(!mesh && error.status == DUST3D_STATUS_INVALID_DOCUMENT, "Invalid numeric fixture was accepted");
    require(error.message[0] != '\0', "Numeric rejection lost its diagnostic");
}

std::string replaceValue(std::string xml, const std::string& oldValue, const std::string& newValue)
{
    const auto position = xml.find(oldValue);
    require(position != std::string::npos, "Numeric fixture marker missing");
    xml.replace(position, oldValue.size(), newValue);
    return xml;
}
}

int main()
{
    try {
        const auto dot = load(model());
        std::string commaXml = model();
        // The synthetic model contains decimal points only in numeric attributes.
        std::replace(commaXml.begin(), commaXml.end(), '.', ',');
        const auto comma = load(commaXml);
        const size_t count = dust3d_mesh_vertex_count(dot.get());
        require(count > 0 && count % 3 == 0 && dust3d_mesh_vertex_count(comma.get()) == count, "Numeric locale changed triangle counts");
        require(dust3d_mesh_node_count(comma.get()) == 2 && dust3d_mesh_part_count(comma.get()) == 1, "Numeric fixture source counts changed");
        const auto* expected = dust3d_mesh_vertices(dot.get());
        const auto* actual = dust3d_mesh_vertices(comma.get());
        for (size_t index = 0; index < count; ++index) {
            float normalSquared = 0;
            for (size_t axis = 0; axis < 3; ++axis) {
                require(std::isfinite(actual[index].position[axis]) && actual[index].position[axis] == expected[index].position[axis], "Decimal separator changed positions");
                require(std::isfinite(actual[index].normal[axis]) && actual[index].normal[axis] == expected[index].normal[axis], "Decimal separator changed normals");
                require(actual[index].color[axis] == expected[index].color[axis], "Decimal separator changed colors");
                normalSquared += actual[index].normal[axis] * actual[index].normal[axis];
            }
            require(std::abs(normalSquared - 1) < 0.001f, "Numeric fixture normal is not unit length");
        }
        for (const auto* value : { "0,25junk", "0,25,0", "0.25junk", "0.25.0", "nan", "inf", "1e309", "1e-999", "5e-324", "1000001" }) {
            reject(replaceValue(model(), "cutRotation='0.25'", std::string("cutRotation='") + value + "'"));
        }
        reject(replaceValue(model(), "radius='0.1'", "radius='-0,1'"));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

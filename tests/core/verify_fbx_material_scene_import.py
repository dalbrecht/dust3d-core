#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Assert Assimp's dump of fbx_material_scene_tests, including embedded pixels.

Usage: assimp dump fixture.fbx fixture.assxml; python3 THIS_FILE fixture.assxml
Only controlled acceptance fixtures are supported; this is not an asset decoder.
"""
import struct
import sys
import xml.etree.ElementTree as ET
import zlib
from verify_fbx_mesh_scene_import import require


def png_pixels(data):
    """Verify the known2x2 authored RGBA PNG's structure, CRCs and exact pixels."""
    require(50 < len(data) < 4096 and data[:8] == b"\x89PNG\r\n\x1a\n", "invalid controlled PNG size/signature")
    offset = 8
    compressed = bytearray()
    kinds = []
    while offset < len(data):
        require(offset + 12 <= len(data), "truncated PNG chunk")
        length = struct.unpack_from(">I", data, offset)[0]
        require(offset + 12 + length <= len(data), "PNG chunk length exceeds input")
        kind = data[offset + 4:offset + 8]
        payload = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        require(zlib.crc32(kind + payload) & 0xffffffff == crc, "PNG CRC changed")
        if kind == b"IHDR":
            require(payload == struct.pack(">IIBBBBB", 2, 2, 8, 6, 0, 0, 0), "expected authored 2x2 RGBA PNG")
        elif kind == b"IDAT":
            compressed.extend(payload)
        else:
            require(kind == b"IEND" and not payload, "unexpected PNG chunk")
        kinds.append(kind)
        offset += length + 12
    require(kinds == [b"IHDR", b"IDAT", b"IEND"], "PNG chunk order changed")
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, 19)
    require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail, "PNG compressed payload changed")
    require(len(raw) == 18 and raw[0] == raw[9] == 0, "PNG scanlines changed")
    return raw[1:9] + raw[10:18]


def verify(path):
    """Reject material loss, swapped channels, UV loss and altered image content."""
    scene = ET.parse(path).getroot().find("Scene")
    require(scene is not None, "missing material scene")
    materials = scene.findall("./MaterialList/Material")
    require(len(materials) == 1, "expected one material")
    properties = materials[0].findall("./MatPropertyList/MatProperty")

    def property_value(key, usage="n/a"):
        matches = [item for item in properties if item.get("key") == key and item.get("tex_usage") == usage]
        require(len(matches) == 1, f"missing or duplicate material property {key}/{usage}")
        return (matches[0].text or '').strip()

    def numbers(key):
        return [float(value) for value in property_value(key).split()]

    require(numbers("$clr.diffuse") == numbers("$clr.base") == [.25, .5, .75], "base color changed")
    require(numbers("$mat.opacity") == [.625], "opacity changed")
    require(numbers("$mat.metallicFactor") == [.375], "metallic factor changed")
    require(numbers("$mat.roughnessFactor") == [.875], "roughness factor changed")
    require(numbers("$clr.emissive") == [1, 1, 1], "emission changed")
    require(numbers("$mat.emissiveIntensity") == [1], "emission intensity changed")
    names = ["color", "normal", "metallic", "roughness", "emissive", "ao"]
    textures = scene.findall("./TextureList/Texture")
    require(len(textures) == 6, "expected six textures")
    for channel, name in enumerate(names):
        filename = f"Dust3D_{name}.png"
        matches = [(index, item) for index, item in enumerate(textures) if item.get("name") == filename]
        require(len(matches) == 1, f"missing or duplicate texture {name}")
        index, texture = matches[0]
        data_node = texture.find("Data")
        require(data_node is not None, f"missing embedded data for {name}")
        tokens = (data_node.text or '').split()
        require(len(tokens) < 4096, "controlled embedded image exceeds fixture limit")
        data = bytes(int(value, 16) for value in tokens)
        require(texture.get("compressed") == "true" and len(data) == int(data_node.get("length", '-1')), "embedded length/encoding changed")
        expected = bytes([20 + channel * 30, 40 + channel * 20, 60 + channel * 10, 255,
                          200 - channel * 20, 120, 80, 128, 70, 90, 110, 255, 150, 170, 190, 64])
        require(png_pixels(data) == expected, f"embedded pixels changed for {name}")
        require(property_value(f"$raw.Maya|TEX_{name}_map|file", "Unknown") == f'"*{index}"', f"embedded binding changed for {name}")
    for usage, name in [("Diffuse", "color"), ("Normals", "normal"), ("Metalness", "metallic"),
                        ("DiffuseRoughness", "roughness"), ("Emissive", "emissive"), ("AmbientOcclusion", "ao")]:
        require(property_value("$tex.file", usage) == f'"Dust3D_{name}.png"', f"texture role changed for {name}")
        require(property_value("$tex.uvwsrc", usage) == "0", f"UV set changed for {name}")
    meshes = scene.findall("./MeshList/Mesh")
    require(len(meshes) == 1 and meshes[0].get("material_index") == "0", "mesh material assignment changed")
    mesh = meshes[0]
    for name in ['Positions', 'TextureCoords', './FaceList/Face']:
        require(mesh.find(name) is not None, f"missing material geometry {name}")
    require([float(v) for v in (mesh.find("Positions").text or '').split()] == [0, 0, 0, 2, 0, 0, 0, 3, 0], "positions changed")
    require([float(v) for v in (mesh.find("TextureCoords").text or '').split()] == [0, 1, 1, 1, 0, 0], "UVs changed")
    require([int(v) for v in (mesh.find("./FaceList/Face").text or '').split()] == [0, 1, 2], "face winding changed")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: verify_fbx_material_scene_import.py fixture.assxml", file=sys.stderr)
        sys.exit(2)
    try:
        verify(sys.argv[1])
        print("Independent Assimp material, six embedded image channels, pixels and UV acceptance passed")
    except (OSError, ValueError, ET.ParseError, struct.error, zlib.error) as cause:
        print(f"FBX material verification failed: {cause}", file=sys.stderr)
        sys.exit(1)

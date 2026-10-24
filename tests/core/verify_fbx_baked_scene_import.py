#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare a controlled generated/baked FBX Assimp dump with its original source oracle.

Usage: python3 -B -O THIS_FILE baked.assxml expected.json
The oracle contains original double corners and raw baked pixels, not encoded FBX data.
This optional acceptance tool is not an arbitrary native-asset decoder.
"""
import json
import math
import sys
import xml.etree.ElementTree as ET
from verify_fbx_mesh_scene_import import require
from verify_png_encoding import verify_png_bytes


def verify(path, expected):
    scene = ET.parse(path).getroot().find('Scene')
    require(scene is not None, 'missing generated scene')
    materials = scene.findall('./MaterialList/Material')
    require(len(materials) == 1, 'expected one generated atlas material')
    properties = materials[0].findall('./MatPropertyList/MatProperty')

    def property_value(key, usage='n/a'):
        values = [item for item in properties if item.get('key') == key and item.get('tex_usage') == usage]
        require(len(values) == 1, f'missing/duplicate {key}/{usage}')
        return (values[0].text or '').strip()

    def scalar(key, value):
        require([float(item) for item in property_value(key).split()] == value, f'material factor changed: {key}')

    images = expected['images']
    channels = [image['channel'] for image in images]
    require(channels and channels[0] == 'color' and len(set(channels)) == len(channels), 'invalid controlled image oracle')
    scalar('$clr.diffuse', [1, 1, 1])
    scalar('$clr.base', [1, 1, 1])
    scalar('$mat.opacity', [1])
    scalar('$mat.metallicFactor', [1 if 'metallic' in channels else 0])
    scalar('$mat.roughnessFactor', [1])
    emission = 1 if 'emissive' in channels else 0
    scalar('$clr.emissive', [emission] * 3)
    scalar('$mat.emissiveIntensity', [emission])
    roles = {'color': 'Diffuse', 'normal': 'Normals', 'metallic': 'Metalness',
             'roughness': 'DiffuseRoughness', 'emissive': 'Emissive', 'ao': 'AmbientOcclusion'}
    textures = scene.findall('./TextureList/Texture')
    require(len(textures) == len(images), 'baked texture count changed')
    for image in images:
        channel = image['channel']
        require(channel in roles, 'unknown controlled map channel')
        name = f'Dust3D_{channel}.png'
        matches = [(index, item) for index, item in enumerate(textures) if item.get('name') == name]
        require(len(matches) == 1, f'missing/duplicate {channel} image')
        index, texture = matches[0]
        data = texture.find('Data')
        require(data is not None and texture.get('compressed') == 'true', 'missing encoded PNG')
        tokens = (data.text or '').split()
        require(len(tokens) == int(data.get('length', '-1')), 'encoded image length differs')
        verify_png_bytes(bytes(int(value, 16) for value in tokens), image['width'], image['height'], bytes(image['pixels']))
        require(property_value('$tex.file', roles[channel]) == f'"{name}"', f'{channel} role changed')
        require(property_value('$tex.uvwsrc', roles[channel]) == '0', f'{channel} UV set changed')
        require(property_value(f'$raw.Maya|TEX_{channel}_map|file', 'Unknown') == f'"*{index}"', f'{channel} embedded binding changed')

    meshes = scene.findall('./MeshList/Mesh')
    require(len(meshes) == 1 and meshes[0].get('material_index') == '0', 'generated mesh material changed')
    mesh = meshes[0]

    def values(name, width):
        item = mesh.find(name)
        require(item is not None, f'missing generated {name}')
        result = [float(value) for value in (item.text or '').split()]
        require(result and len(result) % width == 0 and all(math.isfinite(value) for value in result), f'invalid {name}')
        return [result[offset:offset+width] for offset in range(0, len(result), width)]

    positions, normals, uvs = values('Positions', 3), values('Normals', 3), values('TextureCoords', 2)
    require(len(positions) == len(normals) == len(uvs), 'corner streams lost alignment')
    source = expected['corners']
    require(source and len(source) % 3 == 0, 'invalid controlled corner oracle')
    faces = mesh.findall('./FaceList/Face')
    require(len(faces) * 3 == len(source), 'source face count changed')
    for triangle, face in enumerate(faces):
        indices = [int(value) for value in (face.text or '').split()]
        require(len(indices) == 3, 'generated face is not triangular')
        for corner, index in enumerate(indices):
            require(0 <= index < len(positions), 'generated face index out of range')
            actual = positions[index] + normals[index] + uvs[index]
            original = source[triangle * 3 + corner]
            require(len(original) == 8 and all(math.isclose(a, b, rel_tol=2e-5, abs_tol=2e-5)
                                             for a, b in zip(actual, original)), 'source position/normal/UV/winding changed')
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    matrices = scene.findall('.//Matrix4')
    require(len(matrices) == 2 and all([float(value) for value in (item.text or '').split()] == identity
                                     for item in matrices), 'generated transforms changed')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: verify_fbx_baked_scene_import.py baked.assxml expected.json')
    with open(sys.argv[2], encoding='utf-8') as source:
        verify(sys.argv[1], json.load(source))
    print('Independent generated-bake geometry/material/embedded-pixel acceptance passed')

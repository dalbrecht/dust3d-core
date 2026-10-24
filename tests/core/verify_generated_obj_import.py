#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare controlled generated geometry OBJ with its Assimp XML import.

Usage: python3 -B -O THIS_FILE original.obj imported.assxml
Only freshly generated known-positive outputs reach the separate native importer.
This compares emitted text, not independent evidence of generator precision.
"""
import math
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
from verify_fbx_mesh_scene_import import require

FLOAT_IMPORT_TOLERANCE = 2e-5


def source_geometry(path):
    vertices, faces = [], []
    for line in path.read_text(encoding='utf-8').splitlines():
        fields = line.split()
        if not fields or fields[0].startswith('#'):
            continue
        if fields[0] == 'v':
            require(len(fields) == 4, 'invalid source position')
            vertex = [float(value) for value in fields[1:]]
            require(all(math.isfinite(value) for value in vertex), 'nonfinite source position')
            vertices.append(vertex)
        elif fields[0] == 'f':
            require(len(fields) in (4, 5), 'source face must remain triangular or quadrilateral')
            faces.append([int(value) - 1 for value in fields[1:]])
        else:
            raise ValueError('unexpected source record outside geometry-only OBJ')
    require(vertices and faces, 'missing source geometry')
    require(all(0 <= index < len(vertices) for face in faces for index in face), 'source index out of range')
    return vertices, faces


def verify(source, imported):
    vertices, faces = source_geometry(source)
    scene = ET.parse(imported).getroot().find('Scene')
    require(scene is not None, 'missing imported scene')
    meshes = scene.findall('./MeshList/Mesh')
    require(len(meshes) == 1, 'expected one controlled mesh')
    positions = meshes[0].find('Positions')
    require(positions is not None, 'missing imported positions')
    coordinates = [float(value) for value in (positions.text or '').split()]
    require(coordinates and len(coordinates) % 3 == 0
            and all(math.isfinite(value) for value in coordinates), 'invalid imported positions')
    imported_vertices = [coordinates[offset:offset+3] for offset in range(0, len(coordinates), 3)]
    imported_faces = meshes[0].findall('./FaceList/Face')
    require(len(imported_faces) == len(faces), 'source polygon count changed')
    for original, imported_face in zip(faces, imported_faces):
        indices = [int(value) for value in (imported_face.text or '').split()]
        require(len(indices) == len(original), 'source polygon arity changed')
        for source_index, imported_index in zip(original, indices):
            require(0 <= imported_index < len(imported_vertices), 'imported index out of range')
            require(all(math.isclose(expected, actual, rel_tol=FLOAT_IMPORT_TOLERANCE,
                                     abs_tol=FLOAT_IMPORT_TOLERANCE)
                        for expected, actual in zip(vertices[source_index], imported_vertices[imported_index])),
                    'source corner position, polygon order or winding changed')
    roots = scene.findall('./Node')
    require(len(roots) == 1, 'expected one scene root node')
    child_lists = roots[0].findall('./NodeList')
    require(len(child_lists) == 1 and child_lists[0].get('num') == '1', 'source node hierarchy changed')
    children = child_lists[0].findall('./Node')
    require(len(children) == 1 and scene.findall('.//Node') == [roots[0], children[0]],
            'source mesh node is missing, disconnected or duplicated')
    references = scene.findall('.//MeshRefs')
    require(len(references) == 1 and children[0].find('./MeshRefs') is references[0]
            and references[0].get('num') == '1' and (references[0].text or '').split() == ['0'],
            'source mesh must be referenced exactly once by its scene node')
    for node in [roots[0], children[0]]:
        require(len(node.findall('./Matrix4')) == 1, 'each source node requires its own transform')
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    matrices = scene.findall('.//Matrix4')
    require(len(matrices) == 2 and all([float(value) for value in (item.text or '').split()] == identity
                                     for item in matrices), 'source transforms changed')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: verify_generated_obj_import.py original.obj imported.assxml')
    verify(Path(sys.argv[1]), Path(sys.argv[2]))
    print('Generated OBJ face-corner, polygon order/arity/winding and transform acceptance passed')

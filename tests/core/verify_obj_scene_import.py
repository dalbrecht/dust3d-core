#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify Assimp's known-positive obj_document_tests quad/triangle fixture.

Usage: python3 -B -O THIS_FILE polygons.assxml
This controlled acceptance oracle is not a general native OBJ decoder.
"""
import sys
import xml.etree.ElementTree as ET
from verify_fbx_mesh_scene_import import require


def verify(path):
    scene = ET.parse(path).getroot().find('Scene')
    require(scene is not None, 'missing OBJ import scene')
    meshes = scene.findall('./MeshList/Mesh')
    require(len(meshes) == 1, 'expected one OBJ mesh')
    positions = meshes[0].find('Positions')
    require(positions is not None, 'missing OBJ positions')
    require([float(value) for value in (positions.text or '').split()] ==
            [-1.25, 0, 0, 2.5, 0, 0, 2.5, 3, 0, -1.25, 3, 0,
             -1.25, 0, 0, -1.25, 3, 0, 0, 0, 4], 'OBJ positions changed')
    faces = [[int(value) for value in (face.text or '').split()]
             for face in meshes[0].findall('./FaceList/Face')]
    require(faces == [[0, 1, 2, 3], [4, 5, 6]], 'OBJ polygon arity/order/winding changed')
    matrices = scene.findall('.//Matrix4')
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    require(len(matrices) == 2 and all([float(value) for value in (item.text or '').split()] == identity
                                     for item in matrices), 'OBJ transforms changed')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: verify_obj_scene_import.py polygons.assxml')
    verify(sys.argv[1])
    print('Independent OBJ quad/triangle positions, winding and transforms passed')

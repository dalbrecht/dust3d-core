#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify Assimp's XML dump of the explicit fbx_mesh_scene_tests fixture.

Usage: assimp dump fixture.fbx fixture.assxml; python3 THIS_FILE fixture.assxml
Assimp is a separate optional acceptance tool, never a production dependency.
"""
import sys
import xml.etree.ElementTree as ET


class VerificationError(ValueError):
    """The controlled fixture lost required geometry, attributes or transforms."""


def require(condition, cause):
    """Keep acceptance checks active when Python optimization removes assertions."""
    if not condition:
        raise VerificationError(cause)


def verify(path):
    """Reject a dump that loses geometry, corner attributes, or identity frames."""
    scene = ET.parse(path).getroot().find("Scene")
    require(scene is not None, "missing Assimp scene")
    meshes = scene.findall("./MeshList/Mesh")
    require(len(meshes) == 1, "expected one mesh")
    mesh = meshes[0]

    def read_values(name):
        element = mesh.find(name)
        require(element is not None, f"missing {name}")
        try:
            return [float(value) for value in (element.text or '').split()]
        except ValueError as cause:
            raise VerificationError(f"invalid numeric {name}: {cause}") from cause

    require(read_values("Positions") == [0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 4, 2, 0, 0], "positions changed")
    require(read_values("Normals") == [0, 0, 1] * 3 + [0, 1, 0] * 3, "normals changed")
    require(read_values("TextureCoords") == [0, 1, 1, 1, 0, 0, .25, .75, .5, .5, .75, .25], "UVs changed")
    faces = mesh.findall("./FaceList/Face")
    require([[int(value) for value in (face.text or '').split()] for face in faces] == [[0, 1, 2], [3, 4, 5]], "faces changed")
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    matrices = scene.findall(".//Matrix4")
    require(len(matrices) == 2, "expected two transforms")
    require(all([float(value) for value in (matrix.text or '').split()] == identity for matrix in matrices), "transforms changed")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("usage: verify_fbx_mesh_scene_import.py fixture.assxml", file=sys.stderr)
        sys.exit(2)
    try:
        verify(sys.argv[1])
        print("Independent Assimp mesh/UV/normal/winding/transform acceptance passed")
    except (OSError, ValueError, ET.ParseError) as cause:
        print(f"FBX fixture verification failed: {cause}", file=sys.stderr)
        sys.exit(1)

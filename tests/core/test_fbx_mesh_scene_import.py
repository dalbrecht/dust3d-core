"""Value-only checks for the optional controlled Assimp fixture verifier."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET


SCRIPT = Path(__file__).with_name('verify_fbx_mesh_scene_import.py')
FIXTURE = '''<ASSIMP><Scene><MeshList><Mesh>
<Positions>0 0 0 2 0 0 0 3 0 0 0 0 0 0 4 2 0 0</Positions>
<Normals>0 0 1 0 0 1 0 0 1 0 1 0 0 1 0 0 1 0</Normals>
<TextureCoords>0 1 1 1 0 0 .25 .75 .5 .5 .75 .25</TextureCoords>
<FaceList><Face>0 1 2</Face><Face>3 4 5</Face></FaceList>
</Mesh></MeshList><Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4>
<Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4></Node>
</Node></Scene></ASSIMP>'''


class FbxMeshSceneImportTests(unittest.TestCase):
    def setUp(self):
        specification = importlib.util.spec_from_file_location('verify_fbx_mesh_scene_import', SCRIPT)
        self.module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.module)
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / 'fixture.assxml'
        self.root = ET.fromstring(FIXTURE)

    def write(self):
        ET.ElementTree(self.root).write(self.path)

    def test_should_accept_complete_controlled_fixture(self):
        self.write()
        self.module.verify(self.path)

    def test_should_reject_changed_positions_normals_and_uvs(self):
        for name in ['Positions', 'Normals', 'TextureCoords']:
            with self.subTest(name=name):
                self.root = ET.fromstring(FIXTURE)
                element = self.root.find('.//' + name)
                values = element.text.split()
                values[0] = '7'
                element.text = ' '.join(values)
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path)

    def test_should_reject_reversed_face_and_changed_transform(self):
        for name, value in [('Face', '0 2 1'), ('Matrix4', '1 0 0 0 0 1 0 0 0 0 1 0 2 0 0 1')]:
            with self.subTest(name=name):
                self.root = ET.fromstring(FIXTURE)
                self.root.find('.//' + name).text = value
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path)

    def test_should_reject_missing_corner_stream(self):
        mesh = self.root.find('.//Mesh')
        mesh.remove(mesh.find('Normals'))
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path)

    def test_should_reject_missing_scene_mesh_or_transform(self):
        for kind in ['scene', 'mesh', 'transform']:
            with self.subTest(kind=kind):
                self.root = ET.fromstring(FIXTURE)
                if kind == 'scene':
                    self.root.remove(self.root.find('Scene'))
                elif kind == 'mesh':
                    mesh_list = self.root.find('.//MeshList')
                    mesh_list.remove(mesh_list.find('Mesh'))
                else:
                    node = self.root.find('.//Node')
                    node.remove(node.find('Matrix4'))
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path)

    def test_should_reject_nonfinite_and_invalid_numeric_values(self):
        for value in ['nan', 'inf', 'not-a-number']:
            with self.subTest(value=value):
                self.root.find('.//Positions').text = value
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path)

    def test_should_reject_changed_fixture_even_with_python_optimization(self):
        self.root.find('.//Positions').text = '7 0 0 2 0 0 0 3 0 0 0 0 0 0 4 2 0 0'
        self.write()
        result = subprocess.run([sys.executable, '-B', '-O', str(SCRIPT), str(self.path)],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0, 'Optimized verifier falsely reported acceptance')

    def test_should_accept_controlled_fixture_with_python_optimization(self):
        self.write()
        result = subprocess.run([sys.executable, '-B', '-O', str(SCRIPT), str(self.path)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)


    def selected_fixture(self):
        self.root.find('.//Positions').text = '0 0 0 0 0 4 2 0 0'
        self.root.find('.//Normals').text = '0 1 0 0 1 0 0 1 0'
        self.root.find('.//TextureCoords').text = '.125 .75 .5 .25 .875 0'
        faces = self.root.find('.//FaceList')
        faces.remove(faces.findall('Face')[1])

    def test_should_accept_selected_face_with_replacement_uvs(self):
        self.selected_fixture()
        self.write()
        self.module.verify(self.path, selected_face=True)

    def test_should_reject_selected_face_with_changed_uv(self):
        self.selected_fixture()
        self.root.find('.//TextureCoords').text = '.25 .75 .5 .25 .875 0'
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path, selected_face=True)

    def test_should_verify_selected_face_with_optimized_python(self):
        self.selected_fixture()
        self.write()
        command = [sys.executable, '-B', '-O', str(SCRIPT), str(self.path), '--selected-face']
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.root.find('.//Normals').text = '0 0 1 0 0 1 0 0 1'
        self.write()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

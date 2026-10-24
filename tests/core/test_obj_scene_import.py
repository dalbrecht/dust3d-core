"""Value-only regressions for controlled original-polygon OBJ acceptance."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

SCRIPT = Path(__file__).with_name('verify_obj_scene_import.py')
FIXTURE = '''<ASSIMP><Scene><MeshList><Mesh>
<Positions>-1.25 0 0 2.5 0 0 2.5 3 0 -1.25 3 0 -1.25 0 0 -1.25 3 0 0 0 4</Positions>
<FaceList><Face>0 1 2 3</Face><Face>4 5 6</Face></FaceList></Mesh></MeshList>
<Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4>
<Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4></Node></Node></Scene></ASSIMP>'''


class ObjSceneImportTests(unittest.TestCase):
    def setUp(self):
        specification = importlib.util.spec_from_file_location('obj_scene_verifier', SCRIPT)
        self.module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.module)
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / 'fixture.assxml'
        self.root = ET.fromstring(FIXTURE)

    def write(self):
        ET.ElementTree(self.root).write(self.path)

    def test_should_accept_complete_quad_and_triangle(self):
        self.write()
        self.module.verify(self.path)

    def test_should_reject_triangulation_or_reversed_polygon(self):
        for value in ['0 1 2', '3 2 1 0']:
            self.root.find('.//Face').text = value
            self.write()
            with self.assertRaises(ValueError):
                self.module.verify(self.path)

    def test_should_reject_changed_position_or_transform(self):
        for name in ['Positions', 'Matrix4']:
            self.root = ET.fromstring(FIXTURE)
            self.root.find('.//' + name).text = '7'
            self.write()
            with self.assertRaises(ValueError):
                self.module.verify(self.path)

    def test_should_keep_validation_active_under_python_optimization(self):
        self.write()
        command = [sys.executable, '-B', '-O', str(SCRIPT), str(self.path)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.root.find('.//Face').text = '0 2 1'
        self.write()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

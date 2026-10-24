"""Value-only contracts for controlled generated OBJ import acceptance."""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

SCRIPT = Path(__file__).with_name('verify_generated_obj_import.py')
SOURCE = '''# controlled geometry
v -1.25 0 0
v 2.5 0 0
v 2.5 3 0
v -1.25 3 0
v 0 0 4
f 1 2 3 4
f 1 4 5
'''
IMPORTED = '''<ASSIMP><Scene><MeshList><Mesh>
<Positions>-1.25 0 0 2.5 0 0 2.5 3 0 -1.25 3 0 -1.25 0 0 -1.25 3 0 0 0 4</Positions>
<FaceList><Face>0 1 2 3</Face><Face>4 5 6</Face></FaceList></Mesh></MeshList>
<Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4>
<NodeList num="1"><Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4>
<MeshRefs num="1">0</MeshRefs></Node></NodeList></Node></Scene></ASSIMP>'''


class GeneratedObjImportTests(unittest.TestCase):
    def setUp(self):
        specification = importlib.util.spec_from_file_location('generated_obj_verifier', SCRIPT)
        self.module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.module)
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.source = Path(directory.name) / 'source.obj'
        self.imported = Path(directory.name) / 'imported.assxml'
        self.source.write_text(SOURCE)
        self.root = ET.fromstring(IMPORTED)

    def write_import(self):
        ET.ElementTree(self.root).write(self.imported)

    def test_should_accept_shared_source_vertices_and_split_import_vertices(self):
        self.write_import()
        self.module.verify(self.source, self.imported)

    def test_should_reject_changed_positions_or_nonfinite_values(self):
        original = self.root.find('.//Positions').text
        for value in [original.replace('-1.25', '-1.20', 1), '7', 'nan 0 0']:
            with self.subTest(value=value):
                self.root.find('.//Positions').text = value
                self.write_import()
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)

    def test_should_reject_polygon_reordering_and_missing_position_stream(self):
        faces = self.root.find('.//FaceList')
        original = list(faces)
        faces.clear()
        faces.extend(reversed(original))
        self.write_import()
        with self.assertRaises(ValueError):
            self.module.verify(self.source, self.imported)
        self.root = ET.fromstring(IMPORTED)
        mesh = self.root.find('.//Mesh')
        mesh.remove(mesh.find('Positions'))
        self.write_import()
        with self.assertRaises(ValueError):
            self.module.verify(self.source, self.imported)

    def test_should_reject_triangulation_winding_and_invalid_indices(self):
        for value in ['0 1 2', '3 2 1 0', '0 1 2 999', '-1 1 2 3']:
            with self.subTest(value=value):
                self.root.find('.//Face').text = value
                self.write_import()
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)

    def test_should_reject_missing_faces_and_nonidentity_transform(self):
        for name in ['FaceList', 'Matrix4']:
            with self.subTest(name=name):
                self.root = ET.fromstring(IMPORTED)
                self.root.find('.//' + name).clear()
                self.write_import()
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)

    def test_should_reject_unsupported_source_records_and_invalid_source_indices(self):
        self.write_import()
        for value in ['', SOURCE + 'vt 0 0\n', SOURCE.replace('f 1 4 5', 'f 1 4'),
                      SOURCE.replace('f 1 4 5', 'f 0 4 5'),
                      SOURCE.replace('f 1 4 5', 'f 1 4 99'), SOURCE.replace('v 0 0 4', 'v nan 0 4')]:
            with self.subTest(value=value):
                self.source.write_text(value)
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)

    def test_should_reject_missing_invalid_or_misplaced_mesh_references(self):
        for value in [None, '', '1', '-1', '0 0', 'bad']:
            with self.subTest(value=value):
                self.root = ET.fromstring(IMPORTED)
                node = self.root.find('./Scene/Node/NodeList/Node')
                reference = node.find('MeshRefs')
                if value is None:
                    node.remove(reference)
                else:
                    reference.text = value
                self.write_import()
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)
        self.root = ET.fromstring(IMPORTED)
        reference = self.root.find('.//MeshRefs')
        reference.set('num', '2')
        self.write_import()
        with self.assertRaises(ValueError):
            self.module.verify(self.source, self.imported)
        self.root = ET.fromstring(IMPORTED)
        root_node = self.root.find('./Scene/Node')
        child = root_node.find('./NodeList/Node')
        reference = child.find('MeshRefs')
        child.remove(reference)
        root_node.append(reference)
        self.write_import()
        with self.assertRaises(ValueError):
            self.module.verify(self.source, self.imported)

    def test_should_reject_disconnected_or_extra_scene_nodes(self):
        for mutation in ['disconnect', 'extra', 'count']:
            with self.subTest(mutation=mutation):
                self.root = ET.fromstring(IMPORTED)
                scene = self.root.find('Scene')
                root_node = scene.find('Node')
                children = root_node.find('NodeList')
                if mutation == 'disconnect':
                    child = children.find('Node')
                    children.remove(child)
                    scene.append(child)
                elif mutation == 'extra':
                    ET.SubElement(root_node, 'Node')
                else:
                    children.set('num', '2')
                self.write_import()
                with self.assertRaises(ValueError):
                    self.module.verify(self.source, self.imported)

    def test_should_keep_validation_active_under_python_optimization(self):
        self.write_import()
        command = [sys.executable, '-B', '-O', str(SCRIPT), str(self.source), str(self.imported)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.root.find('.//Face').text = '0 2 1'
        self.write_import()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

        self.root = ET.fromstring(IMPORTED)
        node = self.root.find('./Scene/Node/NodeList/Node')
        node.remove(node.find('MeshRefs'))
        self.write_import()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

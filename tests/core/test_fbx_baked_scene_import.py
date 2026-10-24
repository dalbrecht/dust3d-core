"""Small value-only regressions for the generated baked FBX acceptance verifier."""
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zlib

SCRIPT = Path(__file__).with_name('verify_fbx_baked_scene_import.py')


class BakedSceneImportTests(unittest.TestCase):
    def setUp(self):
        specification = importlib.util.spec_from_file_location('baked_scene_verifier', SCRIPT)
        self.module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.module)
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / 'fixture.assxml'
        self.expected = {'corners': [[0, 0, 0, 0, 0, 1, 0, 1], [1, 0, 0, 0, 0, 1, 1, 1],
                                     [0, 1, 0, 0, 0, 1, 0, 0]],
                         'images': [{'channel': 'color', 'width': 2, 'height': 2, 'pixels': list(range(16))}]}
        self.root = ET.fromstring('''<ASSIMP><Scene><MaterialList><Material><MatPropertyList/>
        </Material></MaterialList><TextureList><Texture name="Dust3D_color.png" compressed="true"><Data/></Texture>
        </TextureList><MeshList><Mesh material_index="0"><Positions>0 0 0 1 0 0 0 1 0</Positions>
        <Normals>0 0 1 0 0 1 0 0 1</Normals><TextureCoords>0 1 1 1 0 0</TextureCoords>
        <FaceList><Face>0 1 2</Face></FaceList></Mesh></MeshList>
        <Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4>
        <Node><Matrix4>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</Matrix4></Node></Node></Scene></ASSIMP>''')
        properties = self.root.find('.//MatPropertyList')
        for key, value, usage in [('$clr.diffuse', '1 1 1', 'n/a'), ('$clr.base', '1 1 1', 'n/a'),
                                  ('$mat.opacity', '1', 'n/a'), ('$mat.metallicFactor', '0', 'n/a'),
                                  ('$mat.roughnessFactor', '1', 'n/a'), ('$clr.emissive', '0 0 0', 'n/a'),
                                  ('$mat.emissiveIntensity', '0', 'n/a'), ('$tex.file', '"Dust3D_color.png"', 'Diffuse'),
                                  ('$tex.uvwsrc', '0', 'Diffuse'), ('$raw.Maya|TEX_color_map|file', '"*0"', 'Unknown')]:
            ET.SubElement(properties, 'MatProperty', key=key, tex_usage=usage).text = value
        def chunk(kind, payload):
            return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))
        image = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
        image += chunk(b'IDAT', zlib.compress(b'\0' + bytes(range(8)) + b'\0' + bytes(range(8, 16)))) + chunk(b'IEND', b'')
        data = self.root.find('.//Texture/Data')
        data.set('length', str(len(image)))
        data.text = ' '.join(f'{value:02x}' for value in image)

    def write(self):
        ET.ElementTree(self.root).write(self.path)

    def test_should_accept_complete_owned_source_expectations(self):
        self.write()
        self.module.verify(self.path, self.expected)

    def test_should_reject_changed_geometry_uvs_normals_or_winding(self):
        for name, value in [('Positions', '0 0 0 2 0 0 0 1 0'), ('Normals', '0 1 0 0 1 0 0 1 0'),
                            ('TextureCoords', '0 0 1 1 0 0'), ('Face', '0 2 1')]:
            with self.subTest(name=name):
                element = self.root.find('.//' + name)
                previous = element.text
                element.text = value
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path, self.expected)
                element.text = previous

    def test_should_reject_changed_image_pixels_or_binding(self):
        self.expected['images'][0]['pixels'][0] = 255
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path, self.expected)
        self.expected['images'][0]['pixels'][0] = 0
        self.root.find('.//MatProperty[@key="$tex.file"]').text = '"wrong.png"'
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path, self.expected)

    def test_should_reject_missing_material_factor(self):
        properties = self.root.find('.//MatPropertyList')
        properties.remove(properties.find('MatProperty[@key="$mat.opacity"]'))
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path, self.expected)

    def test_should_keep_validation_active_in_optimized_python(self):
        self.write()
        expectation = self.path.with_suffix('.json')
        expectation.write_text(json.dumps(self.expected))
        command = [sys.executable, '-B', '-O', str(SCRIPT), str(self.path), str(expectation)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.root.find('.//Face').text = '0 2 1'
        self.write()
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0)

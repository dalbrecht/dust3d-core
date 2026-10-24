"""Value-only regressions for the optional controlled material-import verifier."""
import importlib.util
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
import zlib


SCRIPT = Path(__file__).with_name('verify_fbx_material_scene_import.py')
NAMES = ['color', 'normal', 'metallic', 'roughness', 'emissive', 'ao']
USAGES = ['Diffuse', 'Normals', 'Metalness', 'DiffuseRoughness', 'Emissive', 'AmbientOcclusion']


def fixture_png(channel):
    pixels = bytes([20 + channel * 30, 40 + channel * 20, 60 + channel * 10, 255,
                    200 - channel * 20, 120, 80, 128, 70, 90, 110, 255, 150, 170, 190, 64])
    def chunk(kind, payload):
        return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2, 2, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(b'\0' + pixels[:8] + b'\0' + pixels[8:])) + chunk(b'IEND', b''))


def fixture():
    root = ET.Element('ASSIMP')
    scene = ET.SubElement(root, 'Scene')
    material = ET.SubElement(ET.SubElement(scene, 'MaterialList'), 'Material')
    properties = ET.SubElement(material, 'MatPropertyList')
    def add_property(key, value, usage='n/a'):
        ET.SubElement(properties, 'MatProperty', key=key, tex_usage=usage).text = value
    for key, value in [('$clr.diffuse', '.25 .5 .75'), ('$clr.base', '.25 .5 .75'),
                       ('$mat.opacity', '.625'), ('$mat.metallicFactor', '.375'),
                       ('$mat.roughnessFactor', '.875'), ('$clr.emissive', '1 1 1'),
                       ('$mat.emissiveIntensity', '1')]:
        add_property(key, value)
    textures = ET.SubElement(scene, 'TextureList')
    for channel, (name, usage) in enumerate(zip(NAMES, USAGES)):
        data = fixture_png(channel)
        texture = ET.SubElement(textures, 'Texture', name=f'Dust3D_{name}.png', compressed='true')
        ET.SubElement(texture, 'Data', length=str(len(data))).text = data.hex(' ')
        add_property(f'$raw.Maya|TEX_{name}_map|file', f'"*{channel}"', 'Unknown')
        add_property('$tex.file', f'"Dust3D_{name}.png"', usage)
        add_property('$tex.uvwsrc', '0', usage)
    mesh = ET.SubElement(ET.SubElement(scene, 'MeshList'), 'Mesh', material_index='0')
    ET.SubElement(mesh, 'Positions').text = '0 0 0 2 0 0 0 3 0'
    ET.SubElement(mesh, 'TextureCoords').text = '0 1 1 1 0 0'
    ET.SubElement(ET.SubElement(mesh, 'FaceList'), 'Face').text = '0 1 2'
    return root


class FbxMaterialSceneImportTests(unittest.TestCase):
    def setUp(self):
        specification = importlib.util.spec_from_file_location('verify_fbx_material_scene_import', SCRIPT)
        self.module = importlib.util.module_from_spec(specification)
        specification.loader.exec_module(self.module)
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.path = Path(directory.name) / 'material.assxml'
        self.root = fixture()

    def write(self):
        ET.ElementTree(self.root).write(self.path)

    def test_should_accept_controlled_material_and_all_authored_pixels(self):
        self.write()
        self.module.verify(self.path)

    def test_should_reject_changed_opacity_binding_and_uv_values(self):
        changes = [(".//MatProperty[@key='$mat.opacity']", '.25'),
                   (".//MatProperty[@key='$raw.Maya|TEX_color_map|file']", '"*5"'),
                   ('.//TextureCoords', '0 0 1 1 0 0')]
        for selector, value in changes:
            with self.subTest(selector=selector):
                self.root = fixture()
                self.root.find(selector).text = value
                self.write()
                with self.assertRaises(ValueError):
                    self.module.verify(self.path)

    def test_should_reject_swapped_channels(self):
        textures = self.root.findall('.//Texture')
        textures[0].set('name', 'Dust3D_normal.png')
        textures[1].set('name', 'Dust3D_color.png')
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path)

    def test_should_reject_changed_embedded_crc(self):
        node = self.root.find('.//Data')
        data = bytearray.fromhex(node.text)
        data[29] ^= 1
        node.text = data.hex(' ')
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path)

    def test_should_reject_missing_texture(self):
        textures = self.root.find('.//TextureList')
        textures.remove(textures.find('Texture'))
        self.write()
        with self.assertRaises(ValueError):
            self.module.verify(self.path)

    def test_should_reject_changed_fixture_with_python_optimization(self):
        self.root.find(".//MatProperty[@key='$mat.opacity']").text = '.25'
        self.write()
        result = subprocess.run([sys.executable, '-B', '-O', str(SCRIPT), str(self.path)],
                                capture_output=True, text=True, timeout=10)
        self.assertNotEqual(result.returncode, 0, 'Optimized material verifier falsely reported acceptance')

    def test_should_accept_controlled_fixture_with_python_optimization(self):
        self.write()
        result = subprocess.run([sys.executable, '-B', '-O', str(SCRIPT), str(self.path)],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)

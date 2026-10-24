"""Ordinary filesystem contracts for the Qt-free static FBX export command."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class ExportCliTests(unittest.TestCase):
    def setUp(self):
        self.executable = os.environ['DUST3D_EXPORT_EXECUTABLE']
        directory = tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT'))
        self.addCleanup(directory.cleanup)
        self.directory = Path(directory.name)
        self.original = (Path(os.environ['DUST3D_CORE_FIXTURES']) / 'Capsule.ds3').read_bytes()
        self.source = self.directory / 'source.ds3'
        self.source.write_bytes(self.original)
        self.destination = self.directory / 'rendered ü model.fbx'

    def run_export(self, *arguments):
        return subprocess.run([self.executable, *map(str, arguments)], cwd=self.directory,
                              capture_output=True, text=True, timeout=30)

    def assert_unchanged(self):
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual(list(self.directory.glob('.dust3d-export-*')), [])

    def test_should_export_original_obj_polygons_and_reject_baked_obj(self):
        destination = self.directory / 'original ü.OBJ'
        result = self.run_export(self.source, destination)
        self.assertEqual(result.returncode, 0, result.stderr)
        lines = destination.read_text().splitlines()
        self.assertTrue(any(line.startswith('v ') for line in lines))
        self.assertTrue(any(line.startswith('f ') for line in lines))
        previous = destination.read_bytes()
        for options in [('--baked',), ('--baked', '--texture-size', '64'), ('--max-output-bytes', '1')]:
            result = self.run_export(*options, self.source, destination)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(destination.read_bytes(), previous)
            self.assertNotIn('Exported', result.stdout)
        self.assert_unchanged()

    def test_should_replace_destination_only_after_geometry_export_succeeds(self):
        self.destination.write_bytes(b'old output')
        result = self.run_export(self.source, self.destination)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(self.destination.read_bytes().startswith(b'Kaydara FBX Binary'))
        self.assertIn('Exported', result.stdout)
        self.assert_unchanged()

    def test_should_export_real_baked_atlas(self):
        result = self.run_export('--baked', '--texture-size', '64', self.source, self.destination)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'Dust3D_color.png', self.destination.read_bytes())
        self.assert_unchanged()

    def test_should_accept_literal_dash_filenames_after_option_terminator(self):
        source = self.directory / '-source.ds3'
        source.write_bytes(self.original)
        result = self.run_export('--', '-source.ds3', '-output.FBX')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.directory / '-output.FBX').read_bytes().startswith(b'Kaydara FBX Binary'))
        self.assertEqual(source.read_bytes(), self.original)
        self.assert_unchanged()

    def test_should_preserve_existing_output_on_archive_or_quota_rejection(self):
        self.destination.write_bytes(b'old output')
        malformed = self.directory / 'malformed.ds3'
        malformed.write_bytes(b'abc')
        for arguments in [(malformed, self.destination), ('--max-output-bytes', '1', self.source, self.destination)]:
            with self.subTest(arguments=arguments):
                result = self.run_export(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr.strip())
                self.assertEqual(self.destination.read_bytes(), b'old output')
                self.assertNotIn('Exported', result.stdout)
                self.assert_unchanged()

    def test_should_reject_source_hardlink_and_destination_symlink(self):
        os.link(self.source, self.destination)
        result = self.run_export(self.source, self.destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.destination.read_bytes(), self.original)
        self.destination.unlink()
        self.destination.symlink_to(self.source)
        result = self.run_export(self.source, self.destination)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(self.destination.is_symlink())
        self.assert_unchanged()

    def test_should_reject_invalid_destination_without_modifying_files(self):
        directory = self.directory / 'directory.fbx'
        directory.mkdir()
        for destination in [self.source, directory, self.directory / 'absent' / 'output.fbx']:
            with self.subTest(destination=destination):
                result = self.run_export(self.source, destination)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('Exported', result.stdout)
                self.assert_unchanged()

    def test_should_report_missing_or_nonregular_source_without_touching_output(self):
        self.destination.write_bytes(b'old output')
        for source in [self.directory / 'missing.ds3', self.directory]:
            with self.subTest(source=source):
                result = self.run_export(source, self.destination)
                self.assertNotEqual(result.returncode, 0)
                self.assertTrue(result.stderr.strip())
                self.assertEqual(self.destination.read_bytes(), b'old output')
                self.assert_unchanged()

    def test_should_preserve_existing_output_when_directory_disallows_staging(self):
        if os.geteuid() == 0:
            self.skipTest('Root bypasses directory permissions; no permission-failure claim')
        self.destination.write_bytes(b'old output')
        previous_mode = self.directory.stat().st_mode & 0o777
        try:
            self.directory.chmod(0o500)
            result = self.run_export(self.source, self.destination)
        finally:
            self.directory.chmod(previous_mode)
        self.assertEqual(result.returncode, 4, result.stderr)
        self.assertIn('errno', result.stderr)
        self.assertNotIn('Exported', result.stdout)
        self.assertEqual(self.destination.read_bytes(), b'old output')
        self.assert_unchanged()

    def test_should_validate_arguments_and_report_help(self):
        result = self.run_export('--help')
        self.assertEqual(result.returncode, 0)
        self.assertIn('dust3d-export', result.stdout)
        for arguments in [(), ('--unknown',), ('--texture-size',), ('--max-output-bytes', '999999999999999999999999', self.source, self.destination),
                          ('--texture-size', '0', self.source, self.destination),
                          ('--baked', '--texture-size', '8193', self.source, self.destination),
                          ('--baked', '--texture-size', 'not-a-number', self.source, self.destination)]:
            with self.subTest(arguments=arguments):
                result = self.run_export(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assert_unchanged()

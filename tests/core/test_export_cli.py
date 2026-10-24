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

    def test_should_continue_batch_after_rejected_archive_and_disclose_partial_commit(self):
        bad = self.directory / 'bad.ds3'
        bad.write_bytes(b'abc')
        failed = self.directory / 'failed.obj'
        failed.write_bytes(b'keep')
        single_failure = self.run_export(bad, failed)
        self.assertEqual(single_failure.returncode, 3, single_failure.stderr)
        self.assertTrue(single_failure.stderr.strip())
        first = self.directory / 'first.obj'
        last = self.directory / 'last.fbx'
        result = self.run_export('--batch', self.source, first, bad, failed, self.source, last)
        self.assertEqual(result.returncode, 5, result.stderr)
        self.assertTrue(first.read_bytes().startswith(b'# Dust3D'))
        self.assertTrue(last.read_bytes().startswith(b'Kaydara'))
        self.assertEqual(failed.read_bytes(), b'keep')
        self.assertIn('2 committed, 1 failed', result.stderr)
        self.assertIn('Job 2/3 failed (exit 3)', result.stderr)
        self.assertIn(single_failure.stderr.strip(), result.stderr)
        committed = (first.read_bytes(), last.read_bytes())
        bad.write_bytes(self.original)
        retry = self.run_export(bad, failed)
        self.assertEqual(retry.returncode, 0, retry.stderr)
        self.assertTrue(failed.read_bytes().startswith(b'# Dust3D'))
        self.assertEqual((first.read_bytes(), last.read_bytes()), committed)
        self.assert_unchanged()

    def test_should_preflight_batch_collisions_before_any_output_is_written(self):
        output = self.directory / 'output.obj'
        source_alias = self.directory / 'source-alias.obj'
        os.link(self.source, source_alias)
        nested = self.directory / 'nested'
        nested.mkdir()
        for paths in [(self.source, output, self.source, output),
                      (self.source, output, self.source, nested / '..' / 'output.obj'),
                      (self.source, output, source_alias, source_alias),
                      (self.source, source_alias, source_alias, output)]:
            result = self.run_export('--batch', *paths)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertFalse(output.exists())
            self.assertEqual(source_alias.read_bytes(), self.original)
        self.assert_unchanged()

    def test_should_protect_other_job_inputs_and_hardlinked_output_identities(self):
        other_source = self.directory / 'other.obj'
        other_source.write_bytes(self.original)
        final = self.directory / 'final.fbx'
        result = self.run_export('--batch', self.source, other_source, other_source, final)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(other_source.read_bytes(), self.original)
        self.assertFalse(final.exists())
        first = self.directory / 'first.obj'
        first.write_bytes(b'keep')
        second = self.directory / 'second.fbx'
        os.link(first, second)
        result = self.run_export('--batch', self.source, first, self.source, second)
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertEqual(first.read_bytes(), b'keep')
        self.assertEqual(second.read_bytes(), b'keep')
        self.assert_unchanged()

    def test_should_continue_after_missing_input_and_bound_pair_count(self):
        first = self.directory / 'missing.obj'
        last = self.directory / 'last.obj'
        result = self.run_export('--batch', self.directory / 'absent.ds3', first, self.source, last)
        self.assertEqual(result.returncode, 5, result.stderr)
        self.assertFalse(first.exists())
        self.assertTrue(last.exists())
        self.assertIn('1 committed, 1 failed', result.stderr)
        self.assertIn('Job 1/2 failed (exit 4)', result.stderr)
        self.assertIn('Open source archive', result.stderr)
        self.assertIn('errno', result.stderr)
        result = self.run_export('--batch', *([self.source, first] * 65))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('at most64', result.stderr)
        self.assertFalse(first.exists())
        self.assert_unchanged()

    def test_should_commit_successful_batch_and_reject_incomplete_pairs(self):
        first = self.directory / 'one.obj'
        second = self.directory / 'two.fbx'
        result = self.run_export('--batch', self.source, first, self.source, second)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(first.exists() and second.exists())
        self.assertIn('2 committed, 0 failed', result.stderr)
        for paths in [(), (self.source,), (self.source, first, self.source)]:
            result = self.run_export('--batch', *paths)
            self.assertEqual(result.returncode, 2, result.stderr)
        self.assert_unchanged()

    def test_should_respect_filesystem_identity_for_case_variant_outputs(self):
        probe = self.directory / 'CaseProbe'
        probe.write_bytes(b'owned case-sensitivity probe')
        is_case_insensitive = (self.directory / 'caseprobe').exists()
        probe.unlink()
        expected = self.directory / 'expected.obj'
        single = self.run_export(self.source, expected)
        self.assertEqual(single.returncode, 0, single.stderr)
        first = self.directory / 'Result.obj'
        second = self.directory / 'result.OBJ'
        self.assertFalse(first.exists() or second.exists())
        result = self.run_export('--batch', self.source, first, self.source, second)
        if is_case_insensitive:
            self.assertEqual(result.returncode, 5, result.stderr)
            self.assertIn('Job 2/2 failed (exit 2)', result.stderr)
            self.assertIn('1 committed, 1 failed', result.stderr)
        else:
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('2 committed, 0 failed', result.stderr)
            self.assertFalse(first.samefile(second))
            self.assertEqual(second.read_bytes(), expected.read_bytes())
        self.assertEqual(first.read_bytes(), expected.read_bytes())
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

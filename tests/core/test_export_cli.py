"""Ordinary filesystem contracts for the Qt-free static FBX export command."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from xml.etree import ElementTree


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
        self.assertEqual(list(self.directory.rglob('.dust3d-export-*')), [])

    def test_should_emit_only_json_receipts_for_committed_geometry(self):
        obj = self.directory / 'export ü.obj'
        fbx = self.directory / 'export ü.fbx'
        result = self.run_export('--json', self.source, '-o', obj, '-output', fbx)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(result.stdout.endswith('\n'))
        records = [json.loads(line) for line in result.stdout.splitlines()]
        self.assertEqual(len(records), 2)
        for index, (record, output) in enumerate(zip(records, (obj, fbx)), 1):
            human_output = self.directory / ('human' + output.suffix)
            human = self.run_export(self.source, human_output)
            self.assertEqual(human.returncode, 0, human.stderr)
            self.assertEqual(output.read_bytes(), human_output.read_bytes())
            self.assertEqual(record, {
                'version': 1, 'jobIndex': index, 'jobCount': 2,
                'source': str(self.source), 'destination': str(output),
                'format': output.suffix[1:], 'mode': 'geometry',
                'committed': True, 'byteCount': output.stat().st_size, 'diagnostic': '',
            })
        self.assert_unchanged()

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
        expected = self.directory / 'expected.obj'
        single = self.run_export(self.source, expected)
        self.assertEqual(single.returncode, 0, single.stderr)
        for mode in ['batch', 'legacy']:
            with self.subTest(mode=mode):
                directory = self.directory / mode
                directory.mkdir()
                probe = directory / 'CaseProbe'
                probe.write_bytes(b'owned case-sensitivity probe')
                is_case_insensitive = (directory / 'caseprobe').exists()
                probe.unlink()
                first = directory / 'Result.obj'
                second = directory / 'result.OBJ'
                self.assertFalse(first.exists() or second.exists())
                if mode == 'batch':
                    result = self.run_export('--batch', self.source, first, self.source, second)
                else:
                    result = self.run_export(self.source, '-o', first, '-output', second)
                if is_case_insensitive:
                    self.assertEqual(result.returncode, 5 if mode == 'batch' else 1, result.stderr)
                    self.assertIn('Job 2/2 failed (exit 2)', result.stderr)
                    self.assertIn('1 committed, 1 failed', result.stderr)
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn('2 committed, 0 failed', result.stderr)
                    self.assertFalse(first.samefile(second))
                    self.assertEqual(second.read_bytes(), expected.read_bytes())
                self.assertEqual(first.read_bytes(), expected.read_bytes())
        self.assert_unchanged()

    def test_should_expand_upstream_output_flags_for_one_source(self):
        first = self.directory / 'first.obj'
        last = self.directory / 'last.fbx'
        result = self.run_export('-o', first, self.source, '-output', last)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(first.read_bytes().startswith(b'# Dust3D'))
        self.assertTrue(last.read_bytes().startswith(b'Kaydara'))
        self.assert_unchanged()

    def test_should_disclose_legacy_partial_failure_with_upstream_nonzero_status(self):
        first = self.directory / 'unsupported.format'
        first.write_bytes(b'keep')
        last = self.directory / 'last.obj'
        result = self.run_export(self.source, '-o', first, '-o', last)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(first.read_bytes(), b'keep')
        self.assertTrue(last.exists())
        self.assertIn('1 committed, 1 failed', result.stderr)
        self.assert_unchanged()

    def test_should_reject_ambiguous_or_incomplete_legacy_expansion_before_writing(self):
        target = self.directory / 'new.obj'
        for arguments in [('-o',), ('-output',), ('-o', target),
                          (self.source, self.source, '-o', target),
                          ('--batch', self.source, target, '-o', target)]:
            result = self.run_export(*arguments)
            self.assertEqual(result.returncode, 2, result.stderr)
            self.assertFalse(target.exists())
        self.assert_unchanged()

    def test_should_bound_legacy_target_expansion_before_any_generation(self):
        target = self.directory / 'new.obj'
        result = self.run_export(self.source, *(['-o', target] * 65))
        self.assertEqual(result.returncode, 2, result.stderr)
        self.assertIn('at most64targets', result.stderr)
        self.assertFalse(target.exists())
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

    def test_should_keep_model_chain_geometry_independent_of_endpoint_ids(self):
        first = '{11111111-1111-4111-8111-111111111111}'
        second = '{22222222-2222-4222-8222-222222222222}'
        part = '{33333333-3333-4333-8333-333333333333}'
        component = '{44444444-4444-4444-8444-444444444444}'
        edge = '{55555555-5555-4555-8555-555555555555}'

        def export(start, end, name):
            xml = f"""<canvas originX=".5" originY=".5" originZ=".5">
            <nodes><node id="{start}" partId="{part}" x=".3" y=".5" z=".5" radius=".1"/>
            <node id="{end}" partId="{part}" x=".7" y=".5" z=".5" radius=".12"/></nodes>
            <edges><edge id="{edge}" partId="{part}" from="{start}" to="{end}"/></edges>
            <parts><part id="{part}" cutRotation=".25"/></parts>
            <components><component id="{component}" linkDataType="partId" linkData="{part}"/></components>
            </canvas>""".encode()
            header = f'<ds3><model name="model.xml" offset="0" size="{len(xml)}"/></ds3>'.encode()
            prefix = b'DUST3D 1.0 xml '
            self.source.write_bytes(prefix + f'{len(prefix) + 12 + len(header):010d}\r\n'.encode() + header + xml)
            destination = self.directory / f'{name}.obj'
            result = self.run_export(self.source, destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            return destination.read_bytes()

        baseline = export(first, second, 'ascending')
        self.assertIn(b'\nf ', baseline)
        self.assertEqual(export(second, first, 'descending'), baseline)

    def test_should_keep_stitching_loop_geometry_independent_of_edge_ids(self):
        def identity(number):
            return f'{{{number:08d}-0000-4000-8000-{number:012d}}}'

        canvas = ElementTree.Element('canvas', originX='.5', originY='.5', originZ='.5')
        nodes, edges, parts, components = [ElementTree.SubElement(canvas, tag)
                                           for tag in ('nodes', 'edges', 'parts', 'components')]
        group = ElementTree.SubElement(components, 'component', id=identity(300),
                                       backClosed='true', backCloseDepthRatio='.5', backCloseSharpness='.4')
        for loop, (low, high, z) in enumerate((('.1', '.9', '.5'), ('.34', '.66', '.6'))):
            part = identity(100 + loop)
            ElementTree.SubElement(parts, 'part', id=part, target='StitchingLoop', fillLoopInterior='true')
            ElementTree.SubElement(group, 'component', id=identity(200 + loop), linkDataType='partId', linkData=part)
            for corner, (x, y) in enumerate(((high, '.5'), ('.5', high), (low, '.5'), ('.5', low))):
                node = identity(1000 + loop * 10 + corner)
                ElementTree.SubElement(nodes, 'node', id=node, partId=part, x=x, y=y, z=z, radius='.05')
                ElementTree.SubElement(edges, 'edge', id=identity(2000 + loop * 10 + corner), partId=part,
                                       attrib={'from': identity(1000 + loop * 10 + (corner - 1) % 4), 'to': node})

        def save_source():
            xml = ElementTree.tostring(canvas)
            header = f'<ds3><model name="model.xml" offset="0" size="{len(xml)}"/></ds3>'.encode()
            prefix = b'DUST3D 1.0 xml '
            self.source.write_bytes(prefix + f'{len(prefix) + 12 + len(header):010d}\r\n'.encode() + header + xml)

        def export(destination):
            save_source()
            result = self.run_export(self.source, destination)
            self.assertEqual(result.returncode, 0, result.stderr)
            return destination.read_bytes()

        baseline = export(self.directory / 'closing-edge-first.obj')
        edges[0].set('id', identity(2090))
        edges[4].set('id', identity(2091))
        reordered = export(self.directory / 'closing-edge-last.obj')
        self.assertEqual(reordered, baseline)
        edge = edges[1]
        start, end = edge.get('from'), edge.get('to')
        edge.set('from', end)
        edge.set('to', start)
        self.assertIn(b'\nf ', export(self.directory / 'repaired-loop.obj'))
        edge.set('from', start)
        edge.set('to', end)
        for node in nodes[4:]:
            node.set('partId', identity(100))
        for edge in edges[4:]:
            edge.set('partId', identity(100))
        save_source()
        invalid = self.directory / 'disconnected-loop.obj'
        result = self.run_export(self.source, invalid)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('disconnected edge chains', result.stderr)
        self.assertFalse(invalid.exists())

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

"""Acceptance checks for honest static-export timing receipts."""
import importlib.util
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'static_export_benchmark', Path(__file__).resolve().parents[2] / 'tools' / 'benchmark_static_exports.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class StaticExportBenchmarkTests(unittest.TestCase):
    def test_should_never_accept_success_without_nonempty_output(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable = root / 'encoder'
            executable.write_bytes(b'owned executable identity')
            fixture = root / 'fixture.ds3'
            fixture.write_bytes(b'owned fixture identity')
            result = MODULE.measure_case(executable, fixture, 'obj', root,
                                         runner=lambda *args, **kwargs: subprocess.CompletedProcess(args, 0, 'claimed success', ''))
            self.assertFalse(result['accepted'])
            self.assertIn('nonempty', result['issue'])

    def test_should_preserve_native_failure_and_reject_source_mutation(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable = root / 'encoder'
            executable.write_bytes(b'owned executable identity')
            fixture = root / 'fixture.ds3'
            fixture.write_bytes(b'owned fixture identity')
            result = MODULE.measure_case(executable, fixture, 'obj', root,
                                         runner=lambda *args, **kwargs: subprocess.CompletedProcess(args, 3, '', 'original typed rejection'))
            self.assertFalse(result['accepted'])
            self.assertEqual(result['stderr'], 'original typed rejection')
            self.assertEqual(result['exitCode'], 3)
            def changes_source(command, **kwargs):
                fixture.write_bytes(b'changed')
                Path(command[-1]).write_bytes(b'output')
                return subprocess.CompletedProcess(command, 0, '', '')
            result = MODULE.measure_case(executable, fixture, 'obj', root, runner=changes_source)
            self.assertFalse(result['accepted'])
            self.assertIn('identity', result['issue'])

    def test_should_accept_committed_output_and_record_baked_options(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable = root / 'encoder'
            executable.write_bytes(b'owned executable identity')
            fixture = root / 'fixture.ds3'
            fixture.write_bytes(b'owned fixture identity')
            def writes_output(command, **kwargs):
                self.assertEqual(kwargs.get('timeout'), 60)
                self.assertEqual(command[1:4], ['--baked', '--texture-size', '64'])
                self.assertEqual(command[-2], str(fixture))
                Path(command[-1]).write_bytes(b'committed output')
                return subprocess.CompletedProcess(command, 0, 'Exported', '')
            result = MODULE.measure_case(executable, fixture, 'baked-fbx', root, runner=writes_output)
            self.assertTrue(result['accepted'])
            self.assertEqual(result['outputBytes'], len(b'committed output'))
            self.assertEqual(len(result['outputSha256']), 64)
            self.assertGreaterEqual(result['wallSeconds'], 0)
            self.assertEqual(list(root.glob('sample-*')), [])

    def test_should_record_launch_errors_without_qualifying_a_sample(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable, fixture = root / 'encoder', root / 'fixture.ds3'
            executable.write_bytes(b'owned executable identity')
            fixture.write_bytes(b'owned fixture identity')
            def cannot_launch(command, **kwargs):
                raise OSError(2, 'owned launch error')
            result = MODULE.measure_case(executable, fixture, 'obj', root, runner=cannot_launch)
            self.assertFalse(result['accepted'])
            self.assertIsNone(result['exitCode'])
            self.assertIn('launch', result['issue'])
            self.assertIn('owned launch error', result['stderr'])
            self.assertEqual(list(root.glob('sample-*')), [])

    def test_should_record_timeout_diagnostics_without_native_timeout_probes(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable, fixture = root / 'encoder', root / 'fixture.ds3'
            executable.write_bytes(b'owned executable identity')
            fixture.write_bytes(b'owned fixture identity')
            def deadline_expired(command, **kwargs):
                # This injected callback starts no process and delivers no signal.
                raise subprocess.TimeoutExpired(command, 60, output=b'partial output', stderr=b'original timeout cause')
            result = MODULE.measure_case(executable, fixture, 'obj', root, runner=deadline_expired)
            self.assertFalse(result['accepted'])
            self.assertIsNone(result['exitCode'])
            self.assertEqual(result['stdout'], 'partial output')
            self.assertEqual(result['stderr'], 'original timeout cause')
            self.assertIn('deadline', result['issue'])

    def test_should_preserve_native_diagnostics_when_identity_recheck_fails(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable, fixture = root / 'encoder', root / 'fixture.ds3'
            executable.write_bytes(b'owned executable identity')
            fixture.write_bytes(b'owned fixture identity')
            def removes_owned_fixture(command, **kwargs):
                fixture.unlink()
                return subprocess.CompletedProcess(command, 3, '', 'original rejection before identity loss')
            result = MODULE.measure_case(executable, fixture, 'obj', root, runner=removes_owned_fixture)
            self.assertFalse(result['accepted'])
            self.assertEqual(result['exitCode'], 3)
            self.assertEqual(result['stderr'], 'original rejection before identity loss')
            self.assertIn('identity', result['issue'])
            self.assertTrue(result['identityIssue'])

    def test_should_reject_receipt_paths_that_escape_or_alias_protected_files(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            build_root = root / 'build'
            build_root.mkdir()
            executable, fixture = build_root / 'encoder', root / 'fixture.ds3'
            executable.write_bytes(b'owned executable identity')
            fixture.write_bytes(b'owned fixture identity')
            protected = (executable, fixture)
            receipt = build_root / 'receipt.json'
            self.assertEqual(MODULE.validate_receipt_path(receipt, build_root, protected), receipt.resolve())
            alias = build_root / 'fixture-alias.json'
            alias.hardlink_to(fixture)
            symbolic = build_root / 'fixture-symbolic.json'
            symbolic.symlink_to(fixture)
            for unsafe in [root / 'outside.json', build_root, executable, alias, symbolic]:
                with self.subTest(path=unsafe), self.assertRaises(ValueError):
                    MODULE.validate_receipt_path(unsafe, build_root, protected)
            self.assertEqual(fixture.read_bytes(), b'owned fixture identity')
            self.assertEqual(executable.read_bytes(), b'owned executable identity')

    def test_should_stop_after_first_rejected_sample_without_native_calls(self):
        with tempfile.TemporaryDirectory(dir=os.environ.get('DUST3D_BUILD_ROOT')) as directory:
            root = Path(directory)
            executable, receipt = root / 'encoder', root / 'receipt.json'
            executable.write_bytes(b'owned executable identity')
            arguments = ['benchmark', '--executable', str(executable), '--build-root', str(root),
                         '--configuration', 'Debug', '--receipt', str(receipt)]
            def git_metadata(command, **kwargs):
                self.assertEqual(command[0], 'git')
                return subprocess.CompletedProcess(command, 0, 'owned revision\n' if kwargs.get('text') else b'', '')
            rejected = {'accepted': False, 'issue': 'owned rejected sample'}
            with patch.object(sys, 'argv', arguments), patch.object(MODULE.subprocess, 'run', side_effect=git_metadata), \
                    patch.object(MODULE, 'measure_case', return_value=rejected) as measurement, \
                    contextlib.redirect_stdout(io.StringIO()):
                status = MODULE.main()
            self.assertEqual(status, 1)
            self.assertEqual(measurement.call_count, 1)
            result = json.loads(receipt.read_text())
            self.assertEqual(result['plannedSamples'], 6)
            self.assertTrue(result['stoppedAfterRejectedSample'])
            self.assertEqual(len(result['records']), 1)

# SPDX-License-Identifier: MIT
"""Pure staging tests; fake bytes are never executed or passed to a native parser."""
import importlib.util
from pathlib import Path
import plistlib
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('stage_worker_service', ROOT / 'tools/stage_worker_service.py')
STAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STAGE)
REQUIREMENT = 'cdhash H"0000000000000000000000000000000000000000"'


class WorkerServiceBundleTests(unittest.TestCase):
    def test_should_preserve_explicit_requirement_and_sandbox_only_policy(self):
        info = STAGE.service_info('org.dust3d.worker', REQUIREMENT)
        self.assertEqual(info['Dust3DWorkerHostRequirement'], REQUIREMENT)
        self.assertEqual(info['CFBundleExecutable'], 'dust3d-xpc-service')
        self.assertEqual(info['XPCService'], {'ServiceType': 'Application', 'RunLoopType': 'dispatch_main'})
        self.assertEqual(STAGE.service_entitlements(), {'com.apple.security.app-sandbox': True})

    def test_should_reject_invalid_metadata_without_security_calls(self):
        for identifier in ('../worker', '.org.worker', 'org..worker', 'worker', 'org.worker/' , 'a' * 256):
            with self.assertRaises(ValueError):
                STAGE.service_info(identifier, REQUIREMENT)
        for requirement in ('', ' \n', 'bad\0value', 'a' * 4097):
            with self.assertRaises(ValueError):
                STAGE.service_info('org.dust3d.worker', requirement)

    def test_should_stage_exact_owned_bytes_without_overwriting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'fresh-worker'
            binary.write_bytes(b'fake executable bytes never launched')
            bundle = root / 'org.dust3d.worker.xpc'
            entitlements = STAGE.stage(binary, bundle, 'org.dust3d.worker', REQUIREMENT)
            self.assertEqual((bundle / 'Contents/MacOS/dust3d-xpc-service').read_bytes(), binary.read_bytes())
            self.assertEqual(plistlib.loads((bundle / 'Contents/Info.plist').read_bytes()), STAGE.service_info('org.dust3d.worker', REQUIREMENT))
            self.assertEqual(plistlib.loads(entitlements.read_bytes()), STAGE.service_entitlements())
            with self.assertRaises(FileExistsError):
                STAGE.stage(binary, bundle, 'org.dust3d.worker', REQUIREMENT)
            self.assertEqual(binary.read_bytes(), b'fake executable bytes never launched')

    def test_should_reject_wrong_bundle_name_or_symlink_source_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'worker'
            binary.write_bytes(b'fake')
            wrong = root / 'wrong.xpc'
            with self.assertRaises(ValueError):
                STAGE.stage(binary, wrong, 'org.dust3d.worker', REQUIREMENT)
            self.assertFalse(wrong.exists())
            link = root / 'link'
            link.symlink_to(binary)
            with self.assertRaises(ValueError):
                STAGE.stage(link, root / 'org.dust3d.worker.xpc', 'org.dust3d.worker', REQUIREMENT)


if __name__ == '__main__':
    unittest.main()

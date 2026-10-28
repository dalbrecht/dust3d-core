# SPDX-License-Identifier: MIT
"""Pure staging tests; fake bytes are never executed or passed to a native parser."""
import importlib.util
import json
from pathlib import Path
import plistlib
import tempfile
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('stage_worker_service', ROOT / 'tools/stage_worker_service.py')
STAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STAGE)
POLICY = {'cpuSoftSeconds': '300', 'cpuHardSeconds': '600', 'fileSoftBytes': '67108864',
          'fileHardBytes': '134217728', 'descriptorSoftCeiling': '1024', 'descriptorHardCeiling': '2048',
          'minimumDescriptorCeiling': '256'}
REQUIREMENT = 'cdhash H"0000000000000000000000000000000000000000"'


class WorkerServiceBundleTests(unittest.TestCase):
    def test_should_require_exact_canonical_resource_fields_before_writes(self):
        invalid = [None, {}, dict(POLICY, cpuSoftSeconds=300), dict(POLICY, cpuSoftSeconds=True),
                   dict(POLICY, cpuSoftSeconds='0300'), dict(POLICY, cpuSoftSeconds='0'),
                   dict(POLICY, cpuHardSeconds='299'), dict(POLICY, unknown='1'),
                   dict(POLICY, minimumDescriptorCeiling='1025'),
                   dict(POLICY, fileHardBytes='18446744073709551616')]
        for policy in invalid:
            with self.subTest(policy=policy), self.assertRaises(ValueError):
                STAGE.service_info('org.dust3d.worker', REQUIREMENT, policy)

    def test_should_require_explicit_bounded_unique_policy_file_in_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'fresh-worker'
            binary.write_bytes(b'fake bytes never executed')
            requirement = root / 'requirement.txt'
            requirement.write_text(REQUIREMENT, encoding='utf-8')
            policy_file = root / 'policy.json'
            output = root / 'org.dust3d.worker.xpc'
            command = [sys.executable, '-B', str(ROOT / 'tools/stage_worker_service.py'),
                       '--binary', str(binary), '--service-identifier', 'org.dust3d.worker',
                       '--host-requirement-file', str(requirement), '--output', str(output)]
            absent = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertNotEqual(absent.returncode, 0)
            self.assertIn('--resource-policy-file', absent.stderr)
            for text, cause in [(' ' * 4097, 'exceeds 4096'),
                                ('{"cpuSoftSeconds":"300","cpuSoftSeconds":"400"}', 'Duplicate'),
                                (json.dumps(dict(POLICY, cpuSoftSeconds=300)), 'canonical decimal')]:
                policy_file.write_text(text, encoding='utf-8')
                result = subprocess.run(command + ['--resource-policy-file', str(policy_file)],
                                        capture_output=True, text=True, check=False)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(cause, result.stderr)
                self.assertFalse(output.exists())
            policy_file.write_text(json.dumps(POLICY), encoding='utf-8')
            subprocess.run(command + ['--resource-policy-file', str(policy_file)],
                           capture_output=True, text=True, check=True)
            info = plistlib.loads((output / 'Contents/Info.plist').read_bytes())
            self.assertEqual(info['Dust3DWorkerResourcePolicy'], POLICY)
            self.assertEqual((output / 'Contents/MacOS/dust3d-xpc-service').read_bytes(), binary.read_bytes())

    def test_should_preserve_explicit_requirement_and_sandbox_only_policy(self):
        info = STAGE.service_info('org.dust3d.worker', REQUIREMENT, POLICY)
        self.assertEqual(info['Dust3DWorkerHostRequirement'], REQUIREMENT)
        self.assertEqual(info['Dust3DWorkerConfigurationVersion'], '2')
        self.assertEqual(info['Dust3DWorkerResourcePolicy'], POLICY)
        self.assertEqual(info['CFBundleExecutable'], 'dust3d-xpc-service')
        self.assertEqual(info['XPCService'], {'ServiceType': 'Application', 'RunLoopType': 'dispatch_main'})
        self.assertEqual(STAGE.service_entitlements(), {'com.apple.security.app-sandbox': True})

    def test_should_reject_invalid_metadata_without_security_calls(self):
        for identifier in ('../worker', '.org.worker', 'org..worker', 'worker', 'org.worker/' , 'a' * 256):
            with self.assertRaises(ValueError):
                STAGE.service_info(identifier, REQUIREMENT, POLICY)
        for requirement in ('', ' \n', 'bad\0value', 'a' * 4097):
            with self.assertRaises(ValueError):
                STAGE.service_info('org.dust3d.worker', requirement, POLICY)

    def test_should_stage_exact_owned_bytes_without_overwriting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'fresh-worker'
            binary.write_bytes(b'fake executable bytes never launched')
            bundle = root / 'org.dust3d.worker.xpc'
            entitlements = STAGE.stage(binary, bundle, 'org.dust3d.worker', REQUIREMENT, POLICY)
            self.assertEqual((bundle / 'Contents/MacOS/dust3d-xpc-service').read_bytes(), binary.read_bytes())
            self.assertEqual(plistlib.loads((bundle / 'Contents/Info.plist').read_bytes()), STAGE.service_info('org.dust3d.worker', REQUIREMENT, POLICY))
            self.assertEqual(plistlib.loads(entitlements.read_bytes()), STAGE.service_entitlements())
            with self.assertRaises(FileExistsError):
                STAGE.stage(binary, bundle, 'org.dust3d.worker', REQUIREMENT, POLICY)
            self.assertEqual(binary.read_bytes(), b'fake executable bytes never launched')

    def test_should_reject_wrong_bundle_name_or_symlink_source_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / 'worker'
            binary.write_bytes(b'fake')
            wrong = root / 'wrong.xpc'
            with self.assertRaises(ValueError):
                STAGE.stage(binary, wrong, 'org.dust3d.worker', REQUIREMENT, POLICY)
            self.assertFalse(wrong.exists())
            link = root / 'link'
            link.symlink_to(binary)
            with self.assertRaises(ValueError):
                STAGE.stage(link, root / 'org.dust3d.worker.xpc', 'org.dust3d.worker', REQUIREMENT, POLICY)


if __name__ == '__main__':
    unittest.main()

"""Pure fixture signing-policy checks; no sandbox denial or native engine probes."""
import importlib.util
from pathlib import Path
import plistlib
import subprocess
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    'worker_xpc_fixture', Path(__file__).resolve().parents[1] / 'swift' / 'verify_worker_xpc_static.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class WorkerXPCEntitlementTests(unittest.TestCase):
    def test_should_grant_only_sandbox_and_exact_peer_file_read(self):
        config = Path('/private/tmp/owned-xpc-fixture/peers.json')
        value = MODULE.service_entitlements(config)
        self.assertEqual(set(value), {'com.apple.security.app-sandbox',
                                    'com.apple.security.temporary-exception.files.absolute-path.read-only'})
        self.assertIs(value['com.apple.security.app-sandbox'], True)
        self.assertEqual(value['com.apple.security.temporary-exception.files.absolute-path.read-only'], [str(config)])
        MODULE.verify_service_entitlements(value, config)
        value['com.apple.security.temporary-exception.files.absolute-path.read-only'].append('/private/tmp/')
        self.assertEqual(len(MODULE.service_entitlements(config)['com.apple.security.temporary-exception.files.absolute-path.read-only']), 1)

    def test_should_reject_extra_or_broadened_embedded_grants(self):
        config = Path('/private/tmp/owned-xpc-fixture/peers.json')
        changes = [
            {'com.apple.security.network.client': True},
            {'com.apple.security.temporary-exception.files.absolute-path.read-write': ['/private/tmp/']},
            {'com.apple.security.get-task-allow': True},
            {'com.apple.security.inherit': True},
            {'com.apple.security.app-sandbox': False},
            {'com.apple.security.app-sandbox': 1},
            {'com.apple.security.temporary-exception.files.absolute-path.read-only': ['/private/tmp/']},
            {'com.apple.security.temporary-exception.files.absolute-path.read-only': [str(config.parent / 'other')]},
        ]
        for change in changes:
            with self.subTest(change=change):
                value = MODULE.service_entitlements(config)
                value.update(change)
                with self.assertRaises(ValueError):
                    MODULE.verify_service_entitlements(value, config)
        with self.assertRaises(ValueError):
            MODULE.verify_service_entitlements({}, config)

    def test_should_request_xml_from_codesign_before_parsing(self):
        config = Path('/private/tmp/owned-xpc-fixture/peers.json')
        expected = {'com.apple.security.app-sandbox': True,
                    'com.apple.security.temporary-exception.files.absolute-path.read-only': [str(config)]}
        bundle = Path('/private/tmp/owned-xpc-fixture/StaticWorkerHost.app/Contents/XPCServices/service.xpc')
        result = subprocess.CompletedProcess([], 0, plistlib.dumps(expected).decode(), '')
        with patch.object(MODULE, 'checked', return_value=result) as command:
            self.assertEqual(MODULE.read_service_entitlements(bundle), expected)
        self.assertIn('--xml', command.call_args.args[0])
        with patch.object(MODULE, 'checked', return_value=subprocess.CompletedProcess([], 0, '[Dict]\n', '')):
            with self.assertRaises(plistlib.InvalidFileException):
                MODULE.read_service_entitlements(bundle)

    def test_should_reject_relative_or_directory_peer_configuration_grants(self):
        for config in (Path('peers.json'), Path('/private/tmp/owned-xpc-fixture/'), Path('/private/tmp/other.json')):
            with self.subTest(config=config), self.assertRaises(ValueError):
                MODULE.service_entitlements(config)


if __name__ == '__main__':
    unittest.main()

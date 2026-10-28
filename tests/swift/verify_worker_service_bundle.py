# SPDX-License-Identifier: MIT
"""Build/stage actual product and import its metadata; never launch the unsigned service."""
import os
import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3 or sys.platform != 'darwin':
        raise ValueError('Expected repository and existing shared build root on macOS')
    repository, shared = (Path(value).resolve() for value in sys.argv[1:])
    if not shared.is_dir():
        raise ValueError('Shared build root is missing')
    environment = dict(os.environ, CLANG_MODULE_CACHE_PATH=str(shared / 'clang'), SWIFT_MODULECACHE_PATH=str(shared / 'clang'))
    command = ['xcrun', 'swift', 'build', '--build-system', 'native', '--disable-sandbox', '--package-path', str(repository),
               '--scratch-path', str(shared / 'sdk-core'), '--cache-path', str(shared / 'swiftpm'),
               '--config-path', str(shared / 'config'), '--security-path', str(shared / 'security'), '-Xswiftc', '-warnings-as-errors']
    subprocess.run(command + ['--product', 'dust3d-xpc-service', '-j', '2'], env=environment, check=True)
    subprocess.run(command + ['--product', 'Dust3DWorkerService', '-j', '2'], env=environment, check=True)
    products = Path(subprocess.check_output(command + ['--show-bin-path'], env=environment, text=True).strip())
    with tempfile.TemporaryDirectory(prefix='worker-service-bundle-', dir=shared) as directory:
        root = Path(directory)
        requirement = root / 'explicit-host-requirement.txt'
        requirement.write_text('cdhash H"0000000000000000000000000000000000000000"', encoding='utf-8')
        resource_policy = root / 'resource-policy.json'
        resource_policy.write_text(json.dumps({
            'cpuSoftSeconds': '300', 'cpuHardSeconds': '600', 'fileSoftBytes': '67108864',
            'fileHardBytes': '134217728', 'descriptorSoftCeiling': '1024', 'descriptorHardCeiling': '2048',
            'minimumDescriptorCeiling': '256'}), encoding='utf-8')
        bundle = root / 'org.dust3d.worker.bundle-qa.xpc'
        subprocess.run([sys.executable, '-B', str(repository / 'tools/stage_worker_service.py'),
                        '--binary', str(products / 'dust3d-xpc-service'), '--service-identifier', 'org.dust3d.worker.bundle-qa',
                        '--host-requirement-file', str(requirement), '--resource-policy-file', str(resource_policy), '--output', str(bundle)], check=True)
        if (bundle / 'Contents/MacOS/dust3d-xpc-service').read_bytes() != (products / 'dust3d-xpc-service').read_bytes():
            raise ValueError('Staged executable differs from fresh actual product')
        entitlements = plistlib.loads(bundle.with_suffix('.entitlements.plist').read_bytes())
        if entitlements != {'com.apple.security.app-sandbox': True} or type(entitlements['com.apple.security.app-sandbox']) is not bool:
            raise ValueError('Expected exact sandbox-only entitlement policy')
        consumer = root / 'metadata-consumer'
        subprocess.run(['xcrun', 'swiftc', '-swift-version', '6', '-strict-concurrency=complete', '-warnings-as-errors',
                        '-parse-as-library', '-module-cache-path', str(shared / 'clang'),
                        '-I', str(products / 'Modules'), '-I', str(repository / 'native/WorkerRuntime/include'),
                        '-I', str(repository / 'native/Engine/include'),
                        str(repository / 'tests/swift/WorkerServiceBundleSmoke.swift'), str(products / 'libDust3DWorkerService.a'),
                        '-framework', 'Security', '-framework', 'ImageIO', '-framework', 'CoreGraphics', '-lc++', '-lz',
                        '-o', str(consumer)], env=environment, check=True)
        subprocess.run([str(consumer), str(bundle)], check=True)
        print('Actual unsigned bundle metadata/bytes/entitlements verified; service was not launched or signed')


if __name__ == '__main__':
    main()

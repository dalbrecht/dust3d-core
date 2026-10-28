# SPDX-License-Identifier: MIT
"""Verify the public Swift client package in the caller's shared build/cache root."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) != 3:
        raise ValueError('Expected repository and existing shared build root')
    repository, build = (Path(value).resolve() for value in sys.argv[1:])
    if not (repository / 'Package.swift').is_file() or not build.is_dir():
        raise ValueError('Missing package or shared build root')
    environment = dict(os.environ, CLANG_MODULE_CACHE_PATH=str(build / 'clang'),
                       SWIFT_MODULECACHE_PATH=str(build / 'clang'))
    subprocess.run([
        'xcrun', 'swift', 'test', '--build-system', 'native', '--package-path', str(repository), '--disable-sandbox',
        '--scratch-path', str(build / 'sdk-core'), '--cache-path', str(build / 'swiftpm'),
        '--config-path', str(build / 'config'), '--security-path', str(build / 'security'),
        '--filter', 'Worker(ClientPackage|XPC.*)Tests', '-j', '2',
    ], env=environment, check=True)


if __name__ == '__main__':
    main()

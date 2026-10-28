# SPDX-License-Identifier: MIT
"""Healthy Capsule through the actual public preview product and fresh owned XPC service."""
import os
from pathlib import Path
import subprocess
import sys


def main():
    if len(sys.argv) != 4 or sys.platform != 'darwin':
        raise ValueError('Expected repository, existing shared root and fresh service executable on macOS')
    repository, shared, service = (Path(value).resolve() for value in sys.argv[1:])
    if not shared.is_dir() or not service.is_file():
        raise ValueError('Shared build root or fresh service missing')
    environment = dict(os.environ, CLANG_MODULE_CACHE_PATH=str(shared / 'clang'), SWIFT_MODULECACHE_PATH=str(shared / 'clang'))
    command = ['xcrun', 'swift', 'build', '--build-system', 'native', '--disable-sandbox', '--package-path', str(repository),
               '--scratch-path', str(shared / 'sdk-core'), '--cache-path', str(shared / 'swiftpm'),
               '--config-path', str(shared / 'config'), '--security-path', str(shared / 'security')]
    subprocess.run(command + ['--product', 'Dust3DWorkerPreview', '-j', '2'], env=environment, check=True)
    products = Path(subprocess.check_output(command + ['--show-bin-path'], env=environment, text=True).strip())
    host = shared / 'worker-preview-executor-host'
    sources = [repository / 'tests/swift/WorkerPreviewExecutorHost.swift', repository / 'tests/swift/WorkerXPCCommon.swift']
    subprocess.run(['xcrun', 'swift-format', 'lint', '--strict', *(str(path) for path in sources)], check=True)
    subprocess.run(['xcrun', 'swiftc', '-swift-version', '6', '-strict-concurrency=complete', '-warnings-as-errors',
                    '-parse-as-library', '-module-cache-path', str(shared / 'clang'),
                    '-I', str(products / 'Modules'), '-I', str(repository / 'native/WorkerRuntime/include'),
                    '-I', str(repository / 'native/Engine/include'), *(str(path) for path in sources),
                    str(products / 'libDust3DWorkerPreview.a'), '-framework', 'Security', '-lc++', '-lz', '-o', str(host)],
                   env=environment, check=True)
    subprocess.run([sys.executable, '-B', str(repository / 'tests/swift/verify_worker_xpc_static.py'),
                    str(host), str(service), str(repository / 'tests/core/fixtures/Capsule.ds3'), str(shared),
                    'preview-executor', 'socket'], check=True)


if __name__ == '__main__':
    main()

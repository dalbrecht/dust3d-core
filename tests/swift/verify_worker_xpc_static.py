#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run one healthy static mesh job through freshly signed, owned XPC bundles.

No installed application, launchd registration, credential or forced termination.
Failure artifacts/live processes are preserved; cleanup requires observed host exit and service PID disappearance.
"""
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import sys
import tempfile
import time


def checked(arguments):
    result = subprocess.run(arguments, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f'{arguments[0]} exited {result.returncode}: {result.stdout}{result.stderr}')
    return result


class SandboxEntitlementError(ValueError):
    """The owned fixture signature grants more than its declared development policy."""


def service_entitlements(config):
    if not config.is_absolute() or config.name != 'peers.json':
        raise SandboxEntitlementError('Expected one absolute controller peer configuration file')
    return {'com.apple.security.app-sandbox': True,
            'com.apple.security.temporary-exception.files.absolute-path.read-only': [str(config)]}


def verify_service_entitlements(observed, config):
    expected = service_entitlements(config)
    if (not isinstance(observed, dict)
            or type(observed.get('com.apple.security.app-sandbox')) is not bool
            or observed != expected):
        raise SandboxEntitlementError('Embedded service entitlements differ from the exact sandbox/read-only-file policy')


def read_service_entitlements(bundle):
    embedded = checked(['/usr/bin/codesign', '-d', '--entitlements', '-', '--xml', str(bundle)])
    return plistlib.loads(embedded.stdout.encode())


def code_hash(bundle):
    result = checked(['/usr/bin/codesign', '-d', '--verbose=4', str(bundle)])
    match = re.search(r'^CDHash=([0-9a-f]{40})$', result.stderr, re.MULTILINE)
    if not match:
        raise RuntimeError('Fresh fixture signature lacks a supported CDHash')
    return match.group(1)


def write_bundle(bundle, binary, identifier, package_type):
    contents = bundle / 'Contents'
    executable = contents / 'MacOS' / binary.name
    executable.parent.mkdir(parents=True)
    shutil.copy2(binary, executable)
    values = {'CFBundleIdentifier': identifier, 'CFBundleExecutable': binary.name,
              'CFBundlePackageType': package_type, 'CFBundleVersion': '1',
              'CFBundleInfoDictionaryVersion': '6.0'}
    if package_type == 'XPC!':
        values['XPCService'] = {'ServiceType': 'Application', 'RunLoopType': 'dispatch_main'}
    else:
        values['LSBackgroundOnly'] = True
    (contents / 'Info.plist').write_bytes(plistlib.dumps(values))
    return executable


def main():
    if len(sys.argv) not in (5, 6, 7) or sys.platform != 'darwin':
        raise RuntimeError('Expected fresh host/service executables, licensed Capsule and shared root on macOS')
    host_binary, service_binary, fixture, shared = map(lambda value: Path(value).resolve(), sys.argv[1:5])
    mode = sys.argv[5] if len(sys.argv) >= 6 else 'mesh'
    channel_kind = sys.argv[6] if len(sys.argv) == 7 else 'file'
    if channel_kind not in ('file', 'socket'):
        raise RuntimeError('Expected file or socket channel kind')
    if mode not in ('mesh', 'uv', 'session', 'procedural'):
        raise RuntimeError('Expected base or UV fixture mode')
    if mode in ('session', 'procedural') and channel_kind != 'file':
        raise RuntimeError('Session cache fixture uses regular-file channels')
    if not all(path.is_file() for path in (host_binary, service_binary, fixture)):
        raise RuntimeError('Fresh fixture inputs are missing')
    directory = Path(tempfile.mkdtemp(prefix='worker-xpc-static-', dir=shared))
    complete = False
    try:
        app = directory / 'StaticWorkerHost.app'
        service = app / 'Contents' / 'XPCServices' / 'org.dust3d.core.xpc-static-service.qa.xpc'
        host = write_bundle(app, host_binary, 'org.dust3d.core.xpc-static-host.qa', 'APPL')
        write_bundle(service, service_binary, 'org.dust3d.core.xpc-static-service.qa', 'XPC!')
        config = directory / 'peers.json'
        entitlements = directory / 'service-entitlements.plist'
        entitlements.write_bytes(plistlib.dumps(service_entitlements(config)))
        checked(['/usr/bin/codesign', '--force', '--sign', '-', '--entitlements', str(entitlements), str(service)])
        checked(['/usr/bin/codesign', '--force', '--sign', '-', str(app)])
        checked(['/usr/bin/codesign', '--verify', '--strict', '--deep', str(app)])
        verify_service_entitlements(read_service_entitlements(service), config)
        print('Embedded service sandbox and exact read-only peer-file entitlements verified')
        # Explicit controller-provisioned DEVELOPMENT pins outside the sealed bundles.
        # This private sidecar is a test fixture, never production trust distribution.
        descriptor = os.open(config, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'w', encoding='ascii') as output:
            json.dump({'host': code_hash(app), 'service': code_hash(service)}, output)
        process = subprocess.Popen([str(host), str(fixture), str(directory), mode, channel_kind], stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        deadline = time.monotonic() + 35
        while process.poll() is None and time.monotonic() < deadline:
            time.sleep(0.1)
        if process.poll() is None:
            raise RuntimeError(f'Fixture host {process.pid} still live; preserved without termination')
        output, errors = process.communicate()
        print(output, end='')
        if process.returncode:
            raise RuntimeError(f'Fixture host exited {process.returncode}: {errors}')
        prefix = ('Isolated XPC procedural PNG bake passed: ' if mode == 'procedural' else
                  'Isolated XPC session cache reuse passed: ' if mode == 'session' else
                  'Isolated XPC ordinary UV mesh passed: 240 vertices; 80 UV triangles; ' if mode == 'uv' else
                  'Isolated XPC ordinary mesh passed: 240 vertices; ')
        match = re.search(re.escape(prefix) + r'servicePID=(\d+); hostPID=(\d+)', output)
        if not match or int(match.group(2)) != process.pid or int(match.group(1)) == process.pid:
            raise RuntimeError('Missing independent service process and ordinary mesh evidence')
        if 'XPC service runtime sandbox entitlement verified' not in output:
            raise RuntimeError('Missing service self-entitlement evidence')
        if channel_kind == 'socket' and 'Concurrent descriptor writer, actual EOF decoder and control receipt joined' not in output:
            raise RuntimeError('Missing concurrent stream completion evidence')
        if 'Service one-slot native admission accepted' not in output:
            raise RuntimeError('Missing service admission evidence')
        if 'Queue-authoritative service completion admitted' not in output:
            raise RuntimeError('Missing queue-authoritative current-source admission evidence')
        if 'Persistent service C admission accepted after actual EOF and OS peer observation' not in output:
            raise RuntimeError('Missing persistent service completion admission evidence')
        if mode == 'session':
            if ('Segmented XPC preview admitted after actual EOF and independent OS peer observation' not in output
                    or 'Actual session cache reuse observed across two requests' not in output
                    or 'Actual session cache rebuild observed after authenticated reset' not in output
                    or 'Session cache closed before connection invalidation' not in output):
                raise RuntimeError('Missing retained-session reuse, reset/rebuild or explicit close evidence')
        peer = int(match.group(1))
        deadline = time.monotonic() + 10
        while True:
            observation = subprocess.run(['/bin/ps', '-p', str(peer), '-o', 'stat=,command='],
                                         capture_output=True, text=True, check=False)
            if observation.returncode == 1 and not observation.stdout.strip():
                break
            if observation.returncode or time.monotonic() >= deadline:
                raise RuntimeError(f'Service {peer} PID disappearance not established; preserved without termination: {observation.stdout}{observation.stderr}')
            time.sleep(0.1)
        complete = True
        print('Owned XPC service PID no longer present; temporary bundles removed')
    finally:
        if complete:
            shutil.rmtree(directory)
        else:
            print(f'Preserved owned XPC fixture artifacts: {directory}', file=sys.stderr)


if __name__ == '__main__':
    try:
        main()
    except Exception as cause:
        print(f'Static XPC integration failed: {cause}', file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage an unsigned XPC bundle; never sign, install, launch or overwrite it."""
import argparse
from pathlib import Path
import plistlib
import shutil


def service_info(identifier, host_requirement):
    """Return explicit supported Info.plist values; raise ValueError before writes on invalid metadata."""
    segments = identifier.split('.')
    if (len(identifier.encode('utf-8')) > 255 or len(segments) < 2 or not all(segments)
            or not all(character.isascii() and (character.isalnum() or character in '.-') for character in identifier)):
        raise ValueError('Expected a reverse-domain ASCII service identifier')
    if (not host_requirement.strip() or len(host_requirement.encode('utf-8')) > 4096 or '\0' in host_requirement):
        raise ValueError('Expected an explicit nonempty host requirement of at most 4096 UTF-8 bytes')
    return {'CFBundleIdentifier': identifier, 'CFBundleExecutable': 'dust3d-xpc-service',
            'CFBundlePackageType': 'XPC!', 'CFBundleVersion': '1', 'CFBundleInfoDictionaryVersion': '6.0',
            'XPCService': {'ServiceType': 'Application', 'RunLoopType': 'dispatch_main'},
            'Dust3DWorkerConfigurationVersion': '1', 'Dust3DWorkerHostRequirement': host_requirement}


def service_entitlements():
    """Return sandbox-only policy without development filesystem or network exceptions."""
    return {'com.apple.security.app-sandbox': True}


def stage(binary, output, identifier, host_requirement):
    """Copy fresh binary and metadata to new output; return sibling entitlement path.

    Raises ValueError for metadata/source mismatch and FileExistsError rather than replacing artifacts.
    Staging does not validate code provenance, compile Security requirements or establish release trust.
    """
    info = service_info(identifier, host_requirement)
    binary, output = Path(binary), Path(output)
    if output.name != identifier + '.xpc':
        raise ValueError('Output basename must match the explicit service identifier plus .xpc')
    if binary.is_symlink() or not binary.is_file():
        raise ValueError('Expected a fresh regular executable input, not a symlink')
    entitlements = output.with_suffix('.entitlements.plist')
    if output.exists() or output.is_symlink() or entitlements.exists() or entitlements.is_symlink():
        raise FileExistsError('Output bundle or entitlement file already exists; preserved')
    output.mkdir()
    executable = output / 'Contents/MacOS/dust3d-xpc-service'
    executable.parent.mkdir(parents=True)
    shutil.copyfile(binary, executable)
    executable.chmod(0o755)
    (output / 'Contents/Info.plist').write_bytes(plistlib.dumps(info, sort_keys=True))
    with entitlements.open('xb') as stream:
        plistlib.dump(service_entitlements(), stream, sort_keys=True)
    return entitlements


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True, help='Freshly built dust3d-xpc-service product')
    parser.add_argument('--service-identifier', required=True)
    parser.add_argument('--host-requirement-file', type=Path, required=True, help='Explicit UTF-8 host designated requirement')
    parser.add_argument('--output', type=Path, required=True, help='New <service-identifier>.xpc path; parent must exist')
    arguments = parser.parse_args()
    with arguments.host_requirement_file.open('rb') as stream:
        encoded = stream.read(4097)
    if len(encoded) > 4096:
        raise ValueError('Host requirement file exceeds 4096 bytes')
    requirement = encoded.decode('utf-8')
    entitlements = stage(arguments.binary, arguments.output, arguments.service_identifier, requirement)
    print(f'Unsigned XPC bundle: {arguments.output}\nSandbox entitlement file: {entitlements}')
    print('Signing, host embedding and release trust verification are still required.')


if __name__ == '__main__':
    main()

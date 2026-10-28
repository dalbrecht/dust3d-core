#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage an unsigned XPC bundle; never sign, install, launch or overwrite it."""
import argparse
import json
from pathlib import Path
import plistlib
import shutil


def resource_policy(fields):
    """Copy explicit canonical uint64 lifetime requests; never inspect or change process limits."""
    pairs = [('cpuSoftSeconds', 'cpuHardSeconds'), ('fileSoftBytes', 'fileHardBytes'),
             ('descriptorSoftCeiling', 'descriptorHardCeiling')]
    keys = {key for pair in pairs for key in pair} | {'minimumDescriptorCeiling'}
    if type(fields) is not dict or set(fields) != keys:
        raise ValueError('Expected exactly seven explicit resource fields')
    numbers = {}
    for key, text in fields.items():
        if (type(text) is not str or not 1 <= len(text) <= 20 or not text.isascii() or not text.isdecimal()
                or (len(text) > 1 and text[0] == '0')):
            raise ValueError(f'Expected canonical decimal string for {key}')
        numbers[key] = int(text)
        if not 0 < numbers[key] <= 18446744073709551615:
            raise ValueError(f'Resource value outside positive uint64 range: {key}')
    if any(numbers[soft] > numbers[hard] for soft, hard in pairs):
        raise ValueError('Resource soft request exceeds hard request')
    if numbers['minimumDescriptorCeiling'] > numbers['descriptorSoftCeiling']:
        raise ValueError('Descriptor request is below explicit startup floor')
    return dict(fields)


def service_info(identifier, host_requirement, resources, job_wall_milliseconds):
    """Return explicit supported Info.plist values; raise ValueError before writes on invalid metadata."""
    if (type(job_wall_milliseconds) is not str or not 1 <= len(job_wall_milliseconds) <= 7
            or not job_wall_milliseconds.isascii() or not job_wall_milliseconds.isdecimal()
            or job_wall_milliseconds[0] == '0' or not 1 <= int(job_wall_milliseconds) <= 3600000):
        raise ValueError('Expected explicit canonical job wall milliseconds in 1...3600000')
    policy = resource_policy(resources)
    segments = identifier.split('.')
    if (len(identifier.encode('utf-8')) > 255 or len(segments) < 2 or not all(segments)
            or not all(character.isascii() and (character.isalnum() or character in '.-') for character in identifier)):
        raise ValueError('Expected a reverse-domain ASCII service identifier')
    if (not host_requirement.strip() or len(host_requirement.encode('utf-8')) > 4096 or '\0' in host_requirement):
        raise ValueError('Expected an explicit nonempty host requirement of at most 4096 UTF-8 bytes')
    return {'CFBundleIdentifier': identifier, 'CFBundleExecutable': 'dust3d-xpc-service',
            'CFBundlePackageType': 'XPC!', 'CFBundleVersion': '1', 'CFBundleInfoDictionaryVersion': '6.0',
            'XPCService': {'ServiceType': 'Application', 'RunLoopType': 'dispatch_main'},
            'Dust3DWorkerConfigurationVersion': '3', 'Dust3DWorkerHostRequirement': host_requirement,
            'Dust3DWorkerResourcePolicy': policy, 'Dust3DWorkerJobWallMilliseconds': job_wall_milliseconds}


def service_entitlements():
    """Return sandbox-only policy without development filesystem or network exceptions."""
    return {'com.apple.security.app-sandbox': True}


def stage(binary, output, identifier, host_requirement, resources, job_wall_milliseconds):
    """Copy fresh binary and metadata to new output; return sibling entitlement path.

    Raises ValueError for metadata/source mismatch and FileExistsError rather than replacing artifacts.
    Staging does not validate code provenance, compile Security requirements or establish release trust.
    """
    info = service_info(identifier, host_requirement, resources, job_wall_milliseconds)
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
    parser.add_argument('--resource-policy-file', type=Path, required=True, help='Explicit canonical JSON resource requests')
    parser.add_argument('--job-wall-milliseconds', required=True, help='Explicit canonical per-job wall budget in 1...3600000ms')
    parser.add_argument('--output', type=Path, required=True, help='New <service-identifier>.xpc path; parent must exist')
    arguments = parser.parse_args()
    with arguments.host_requirement_file.open('rb') as stream:
        encoded = stream.read(4097)
    if len(encoded) > 4096:
        raise ValueError('Host requirement file exceeds 4096 bytes')
    requirement = encoded.decode('utf-8')
    with arguments.resource_policy_file.open('rb') as stream:
        encoded_policy = stream.read(4097)
    if len(encoded_policy) > 4096:
        raise ValueError('Resource policy file exceeds 4096 bytes')
    def unique_fields(pairs):
        fields = {}
        for key, value in pairs:
            if key in fields:
                raise ValueError('Duplicate resource policy field')
            fields[key] = value
        return fields
    resources = json.loads(encoded_policy.decode('utf-8'), object_pairs_hook=unique_fields)
    entitlements = stage(arguments.binary, arguments.output, arguments.service_identifier, requirement, resources, arguments.job_wall_milliseconds)
    print(f'Unsigned XPC bundle: {arguments.output}\nSandbox entitlement file: {entitlements}')
    print('Signing, host embedding and release trust verification are still required.')


if __name__ == '__main__':
    main()

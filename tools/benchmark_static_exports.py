#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Serial known-positive static export timings; not a stress or parity test.

Requires a fresh source-audited command build. Records wall time, not peak RSS,
GPU latency, hard cancellation or production budgets. No native fault probes.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import tempfile
import time

PROCESS_TIMEOUT_SECONDS = 60
EXPORT_MODES = ('obj', 'fbx', 'baked-fbx')


def validate_receipt_path(receipt, build_root, protected_paths):
    """Resolve a regular receipt within the shared root, rejecting protected aliases.

    Return the resolved Path. Raise ValueError for unsafe destinations; metadata
    errors retain their original OSError. This preflight never creates a file.
    """
    if receipt.is_symlink():
        raise ValueError('receipt must not be a symbolic link')
    shared_root = build_root.resolve(strict=True)
    resolved = receipt.resolve()
    if not resolved.is_relative_to(shared_root) or resolved == shared_root:
        raise ValueError('receipt must be inside the shared build root')
    if not resolved.parent.is_dir() or (resolved.exists() and not resolved.is_file()):
        raise ValueError('receipt requires an existing directory and regular-file destination')
    if resolved.exists():
        for protected in protected_paths:
            if protected.exists() and resolved.samefile(protected):
                raise ValueError('receipt aliases an executable or input fixture')
    return resolved


def _diagnostic_text(value):
    return value.decode('utf-8', errors='replace') if isinstance(value, bytes) else value or ''


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(65536), b''):
            result.update(chunk)
    return result.hexdigest()


def measure_case(executable, fixture, mode, output_directory, runner=subprocess.run):
    """Measure one ordinary process, accepting only stable inputs and nonempty committed output."""
    if mode not in EXPORT_MODES:
        raise ValueError('unsupported benchmark mode')
    before = (digest(executable), digest(fixture))
    extension = 'obj' if mode == 'obj' else 'fbx'
    with tempfile.TemporaryDirectory(prefix='sample-', dir=output_directory) as directory:
        output = Path(directory) / ('output.' + extension)
        command = [str(executable)]
        if mode == 'baked-fbx':
            command += ['--baked', '--texture-size', '64']
        command += [str(fixture), str(output)]
        started = time.perf_counter()
        exit_code, stdout, stderr, process_issue = None, '', '', None
        try:
            result = runner(command, capture_output=True, text=True, check=False,
                            timeout=PROCESS_TIMEOUT_SECONDS)
            exit_code, stdout, stderr = result.returncode, result.stdout, result.stderr
        except subprocess.TimeoutExpired as cause:
            process_issue = 'export measurement deadline exceeded: ' + str(cause)
            stdout, stderr = _diagnostic_text(cause.stdout), _diagnostic_text(cause.stderr)
        except OSError as cause:
            process_issue = 'could not launch export: ' + str(cause)
            stderr = str(cause)
        elapsed = time.perf_counter() - started
        identity_issue = None
        try:
            if before != (digest(executable), digest(fixture)):
                identity_issue = 'executable or fixture identity changed'
        except OSError as cause:
            identity_issue = 'could not recheck executable/fixture identity: ' + str(cause)
        issue = process_issue or identity_issue
        if issue is None and exit_code != 0:
            issue = 'export failed; original diagnostic retained'
        output_bytes, output_hash, output_issue = 0, None, None
        try:
            if not output.is_symlink() and output.is_file():
                output_bytes = output.stat().st_size
            if issue is None:
                if output_bytes == 0:
                    issue = 'claimed success without nonempty regular output'
                else:
                    output_hash = digest(output)
        except OSError as cause:
            output_issue = 'could not inspect output: ' + str(cause)
            issue = issue or output_issue
        accepted = issue is None
        return {
            'fixture': fixture.name, 'fixtureSha256': before[1], 'executableSha256': before[0],
            'mode': mode, 'wallSeconds': elapsed, 'exitCode': exit_code,
            'accepted': accepted, 'issue': issue, 'identityIssue': identity_issue,
            'outputIssue': output_issue,
            'stdout': stdout, 'stderr': stderr, 'outputBytes': output_bytes,
            'outputSha256': output_hash if accepted else None,
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, required=True)
    parser.add_argument('--build-root', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--configuration', choices=['Debug', 'Release'], required=True,
                        help='caller-reported build configuration; retain the build log separately')
    parser.add_argument('--repetitions', type=int, choices=range(1, 6), default=1)
    arguments = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    executable = arguments.executable.resolve(strict=True)
    build_root = arguments.build_root.resolve(strict=True)
    if not executable.is_file() or not build_root.is_dir():
        parser.error('a freshly built command and existing shared build root are required')
    fixtures = tuple(root / 'tests' / 'core' / 'fixtures' / name for name in ('Capsule.ds3', 'Sword.ds3'))
    protected_paths = (executable, *fixtures)
    try:
        receipt_path = validate_receipt_path(arguments.receipt, build_root, protected_paths)
    except (ValueError, OSError, RuntimeError) as cause:
        parser.error(str(cause))
    head = subprocess.run(['git', '-C', str(root), 'rev-parse', 'HEAD'], check=True,
                          capture_output=True, text=True).stdout.strip()
    changes = subprocess.run(['git', '-C', str(root), 'diff', '--binary', '--no-ext-diff', '--no-textconv', 'HEAD'], check=True,
                             capture_output=True).stdout
    records = []
    planned_samples = arguments.repetitions * len(fixtures) * len(EXPORT_MODES)
    is_stopped = False
    for repetition in range(arguments.repetitions):
        for fixture in fixtures:
            for mode in EXPORT_MODES:
                record = measure_case(executable, fixture, mode, build_root)
                record['repetition'] = repetition + 1
                records.append(record)
                if not record['accepted']:
                    is_stopped = True
                    break
            if is_stopped:
                break
        if is_stopped:
            break
    receipt = {
        'schemaVersion': 1, 'sourceRevision': head,
        'trackedDiffSha256': hashlib.sha256(changes).hexdigest(), 'hasTrackedChanges': bool(changes),
        'configurationReportedByCaller': arguments.configuration,
        'plannedSamples': planned_samples, 'stoppedAfterRejectedSample': is_stopped,
        'machineArchitecture': platform.machine(), 'system': platform.system(), 'systemRelease': platform.release(),
        'pythonVersion': platform.python_version(), 'records': records,
        'limits': 'Serial process wall time including startup and IO; no peak RSS, GPU, large-model, cross-hardware or parity acceptance.',
    }
    try:
        receipt_path = validate_receipt_path(receipt_path, build_root, protected_paths)
    except (ValueError, OSError, RuntimeError) as cause:
        parser.error(str(cause))
    receipt_path.write_text(json.dumps(receipt, indent=2) + '\n')
    accepted = sum(record['accepted'] for record in records)
    print(f'{accepted}/{planned_samples} accepted samples; {len(records)} measured; receipt: {arguments.receipt}')
    return 0 if accepted == planned_samples else 1


if __name__ == '__main__':
    raise SystemExit(main())

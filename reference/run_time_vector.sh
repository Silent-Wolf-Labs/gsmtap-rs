#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 - "$script_dir" "${1:-all}" <<'PY'
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

here = Path(sys.argv[1])
mode = sys.argv[2]
if mode not in ('all', '--verify', '--audit'):
    sys.exit('Usage: run_time_vector.sh [all|--verify|--audit]')
root = here.parent.parent / 'libosmocore-rs' / 'libosmocore'
reference_bytes = (here / 'time_provenance.json').read_bytes()
reference = json.loads(reference_bytes)
if reference['upstream_git_revision'] != 'unknown' or reference['evidence'] != 'exact-source':
    sys.exit('Unexpected provenance policy')
for directory, entries in ((root, reference['sha256']), (Path('/'), reference['system_sha256'])):
    for name, expected in entries.items():
        file = directory / name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            sys.exit('Reference missing or content mismatch: ' + str(file))
reference_digest = hashlib.sha256(reference_bytes).hexdigest()
generator_bytes = (here / 'time_generator_provenance.json').read_bytes()
generator = json.loads(generator_bytes)
if generator['reference_manifest_sha256'] != reference_digest:
    sys.exit('Reference manifest differs from pinned generator provenance')
for name, expected in generator['sha256'].items():
    file = here / name
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        sys.exit('Generator missing or content mismatch: ' + name)
generator_digest = hashlib.sha256(generator_bytes).hexdigest()
units = [here / 'time_vector.c'] + [root / name for name in reference['translation_units']]
cc = ['cc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
      '-I' + str(root), '-isystem', str(root / 'include')]
env = dict(os.environ)
for key in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
            'LD_PRELOAD', 'LD_AUDIT', 'LD_LIBRARY_PATH'):
    env.pop(key, None)
deps = subprocess.check_output(cc + ['-M'] + [str(p) for p in units], text=True, env=env)
for token in deps.replace('\\\n', ' ').split():
    p = Path(token)
    if not p.is_absolute():
        continue
    if p.is_relative_to(root):
        if str(p.relative_to(root)) not in reference['sha256']:
            sys.exit('Unpinned project include: ' + str(p))
    elif p != here / 'time_vector.c' and str(p).lstrip('/') not in reference['system_sha256']:
        sys.exit('Unpinned platform include: ' + str(p))
if mode == '--verify':
    print('Time exact-source content verified: ' + reference_digest, file=sys.stderr)
    sys.exit(0)
with tempfile.TemporaryDirectory(prefix='time-evidence-') as tmp:
    binary = Path(tmp) / 'time-vector'
    if mode == '--audit':
        cc += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    subprocess.run(cc + [str(p) for p in units] + ['-o', str(binary)], check=True, env=env)
    result = subprocess.run([str(binary), reference_digest, generator_digest],
                            capture_output=True, timeout=30, env=env)
    if result.returncode or result.stderr:
        sys.exit('Unexpected golden-domain diagnostic: ' + result.stderr.decode())
    # Deliberately separate pathological nested-peer dispatch from golden data.
    probe = subprocess.run([str(binary), 'peer-reentrant'], capture_output=True, timeout=5, env=env)
    trace = [json.loads(line) for line in probe.stdout.decode().splitlines()]
    repeats = sum(r['event'] == 'callback' and r['who'] == 1 and r['depth'] == 0 for r in trace)
    if probe.returncode != 77 or probe.stderr or repeats != 10:
        sys.exit('STOP AND REVIEW: nested-peer dispatch changed: ' + repr((probe.returncode, repeats, probe.stderr)))
    print('Excluded nested-peer probe: exit=77 after 12 callbacks; peer repeats 10 times after nested return', file=sys.stderr)
    if mode == '--audit':
        for name in ('add', 'sub', 'carry', 'borrow', 'fraction'):
            diagnostic = subprocess.run([str(binary), 'overflow-' + name],
                                        capture_output=True, timeout=5, env=env)
            if not diagnostic.returncode or diagnostic.stdout or b'signed integer overflow' not in diagnostic.stderr:
                sys.exit('Expected signed-overflow diagnostic missing: ' + name)
        print('Time golden domain and bounded nested-peer probe: ASan+UBSan clean; five signed-overflow probes diagnosed (never golden); LSan disabled under ptrace', file=sys.stderr)
    else:
        sys.stdout.buffer.write(result.stdout)
PY

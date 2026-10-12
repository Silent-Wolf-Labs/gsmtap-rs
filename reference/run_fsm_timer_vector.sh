#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 - "$script_dir" "${1:-all}" <<'PY'
import hashlib
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
import tempfile

here = Path(sys.argv[1])
mode = sys.argv[2]
if mode not in ('all', '--verify', '--audit', '--diagnostics'):
    sys.exit('Usage: run_fsm_timer_vector.sh [all|--verify|--audit|--diagnostics]')
root = here.parent.parent / 'libosmocore-rs' / 'libosmocore'
reference_bytes = (here / 'fsm_timer_provenance.json').read_bytes()
reference = json.loads(reference_bytes)
if reference['upstream_git_revision'] != 'unknown' or reference['evidence'] != 'exact-source':
    sys.exit('Unexpected provenance policy')
for directory, entries in ((root, reference['sha256']), (Path('/'), reference['system_sha256'])):
    for name, expected in entries.items():
        file = directory / name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            sys.exit('Reference missing or content mismatch: ' + str(file))
reference_digest = hashlib.sha256(reference_bytes).hexdigest()
generator_bytes = (here / 'fsm_timer_generator_provenance.json').read_bytes()
generator = json.loads(generator_bytes)
if generator['reference_manifest_sha256'] != reference_digest:
    sys.exit('Reference manifest differs from pinned generator provenance')
for name, expected in generator['sha256'].items():
    file = here / name
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        sys.exit('Generator missing or content mismatch: ' + name)
generator_digest = hashlib.sha256(generator_bytes).hexdigest()
units = [here / 'fsm_timer_vector.c'] + [root / name for name in reference['translation_units']]
cc = ['cc', '-std=gnu11', '-O2', '-flto', '-g', '-Wall', '-Wextra', '-Werror',
      '-Wno-unused-parameter', '-Wno-deprecated-declarations', '-Wno-sign-compare', '-ffunction-sections', '-fdata-sections', '-I' + str(root), '-isystem', str(root / 'include')]
env = dict(os.environ)
for key in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
            'LD_PRELOAD', 'LD_AUDIT', 'LD_LIBRARY_PATH', 'GCC_EXEC_PREFIX', 'COMPILER_PATH'):
    env.pop(key, None)
for flag, expected in reference['compiler'].items():
    if subprocess.check_output(['cc', flag], text=True, env=env).strip() != expected:
        sys.exit('Compiler identity mismatch: ' + flag)
deps = subprocess.check_output(cc + ['-M'] + [str(p) for p in units], text=True, env=env)
for token in deps.replace('\\\n', ' ').split():
    p = Path(token)
    if not p.is_absolute():
        continue
    if p.is_relative_to(root):
        if str(p.relative_to(root)) not in reference['sha256']:
            sys.exit('Unpinned project include: ' + str(p))
    elif p != here / 'fsm_timer_vector.c' and str(p).lstrip('/') not in reference['system_sha256']:
        sys.exit('Unpinned platform include: ' + str(p))
if mode == '--verify':
    print('fsm_timer exact-source content verified: ' + reference_digest, file=sys.stderr)
    sys.exit(0)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix='fsm_timer-evidence-') as tmp:
    binary = Path(tmp) / 'fsm_timer-vector'
    audit = mode in ('--audit', '--diagnostics')
    if audit:
        cc += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    subprocess.run(cc + [str(p) for p in units] + ['-Wl,--gc-sections', '/usr/lib/x86_64-linux-gnu/libtalloc.so', '-Wl,-rpath,/usr/lib/x86_64-linux-gnu', '-pthread', '-o', str(binary)], check=True, env=env)
    def run(name):
        return subprocess.run([str(binary), name], capture_output=True, timeout=5, env=env)
    result = subprocess.run([str(binary), reference_digest, generator_digest],
                            capture_output=True, timeout=30, env=env)
    if result.returncode or result.stderr:
        sys.exit('Unexpected golden-domain diagnostic: ' + result.stderr.decode())
    reports = []
    if audit:
        for name, marker in [('invalid-unit', b'division by zero'), ('fallback-overflow', b'signed integer overflow'), ('null-map', b'AddressSanitizer: SEGV')]:
            probe = run(name)
            if not probe.returncode or marker not in probe.stderr:
                sys.exit('Expected UB diagnostic missing: ' + name)
            reports.append({'probe':name,'category':'undefined-behavior'})
        for name, marker in [('missing-tdef',b'default_timeout >= 0'),('map32',b'state < 32')]:
            probe=run(name)
            if probe.returncode != -signal.SIGABRT or marker not in probe.stderr:
                sys.exit('Missing assertion: ' + name)
            reports.append({'probe':name,'category':'assertion'})
        print('fsm_timer successful domain ASan+UBSan clean; three UB probes and two assertions verified',file=sys.stderr)
    if mode == '--diagnostics':
        print(json.dumps({'schema':1, 'reference':reference_digest, 'generator':generator_digest, 'probes':reports}, indent=2))
    elif mode == 'all':
        sys.stdout.buffer.write(result.stdout)
PY

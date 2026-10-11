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
    sys.exit('Usage: run_tdef_vector.sh [all|--verify|--audit|--diagnostics]')
root = here.parent.parent / 'libosmocore-rs' / 'libosmocore'
reference_bytes = (here / 'tdef_provenance.json').read_bytes()
reference = json.loads(reference_bytes)
if reference['upstream_git_revision'] != 'unknown' or reference['evidence'] != 'exact-source':
    sys.exit('Unexpected provenance policy')
for directory, entries in ((root, reference['sha256']), (Path('/'), reference['system_sha256'])):
    for name, expected in entries.items():
        file = directory / name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            sys.exit('Reference missing or content mismatch: ' + str(file))
reference_digest = hashlib.sha256(reference_bytes).hexdigest()
generator_bytes = (here / 'tdef_generator_provenance.json').read_bytes()
generator = json.loads(generator_bytes)
if generator['reference_manifest_sha256'] != reference_digest:
    sys.exit('Reference manifest differs from pinned generator provenance')
for name, expected in generator['sha256'].items():
    file = here / name
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        sys.exit('Generator missing or content mismatch: ' + name)
generator_digest = hashlib.sha256(generator_bytes).hexdigest()
units = [here / 'tdef_vector.c'] + [root / name for name in reference['translation_units']]
cc = ['cc', '-std=gnu11', '-O2', '-flto', '-g', '-Wall', '-Wextra', '-Werror',
      '-ffunction-sections', '-fdata-sections', '-I' + str(root), '-isystem', str(root / 'include')]
env = dict(os.environ)
for key in ('CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH',
            'LD_PRELOAD', 'LD_AUDIT', 'LD_LIBRARY_PATH', 'GCC_EXEC_PREFIX', 'COMPILER_PATH'):
    env.pop(key, None)
deps = subprocess.check_output(cc + ['-M'] + [str(p) for p in units], text=True, env=env)
for token in deps.replace('\\\n', ' ').split():
    p = Path(token)
    if not p.is_absolute():
        continue
    if p.is_relative_to(root):
        if str(p.relative_to(root)) not in reference['sha256']:
            sys.exit('Unpinned project include: ' + str(p))
    elif p != here / 'tdef_vector.c' and str(p).lstrip('/') not in reference['system_sha256']:
        sys.exit('Unpinned platform include: ' + str(p))
if mode == '--verify':
    print('TDef exact-source content verified: ' + reference_digest, file=sys.stderr)
    sys.exit(0)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory(prefix='tdef-evidence-') as tmp:
    binary = Path(tmp) / 'tdef-vector'
    audit = mode in ('--audit', '--diagnostics')
    if audit:
        cc += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
               '-fno-omit-frame-pointer', '-fno-pie', '-no-pie']
        env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    subprocess.run(cc + [str(p) for p in units] + ['-Wl,--gc-sections', '-o', str(binary)], check=True, env=env)
    def run(name):
        return subprocess.run([str(binary), name], capture_output=True, timeout=5, env=env)
    result = subprocess.run([str(binary), reference_digest, generator_digest],
                            capture_output=True, timeout=30, env=env)
    if result.returncode or result.stderr:
        sys.exit('Unexpected golden-domain diagnostic: ' + result.stderr.decode())
    reports = []
    for name, values in [('reset-invalid', '5 92 93'), ('reset-low', '5 7 93'), ('reset-inverted', '91 92 93')]:
        probe = run(name)
        snapshot = json.loads(probe.stdout)
        if probe.returncode != 77 or snapshot != {'type':'panic_snapshot', 'values':values} or b'not in range' not in probe.stderr:
            sys.exit('STOP AND REVIEW: reset failure changed: ' + name)
        reports.append({'probe': name, 'category':'invalid-default', 'exit':probe.returncode, 'values':snapshot['values']})
    for name, text in [('missing-negative', b'val_if_not_present >= 0'), ('state-32', b'state < 32'), ('state-max', b'state < 32')]:
        probe = run(name)
        if probe.returncode != -signal.SIGABRT or text not in probe.stderr:
            sys.exit('STOP AND REVIEW: expected C assertion missing: ' + name)
        reports.append({'probe':name, 'category':'assertion', 'signal':signal.SIGABRT})
    if audit:
        unknown = [-1, 5, -2147483648, 2147483647]
        known = [0, 1, 2, 4]
        pairs = [(a,b) for a in unknown for b in known] + [(a,b) for a in known for b in unknown]
        pairs += [(a,b) for a in unknown for b in unknown if a != b]
        for a,b in pairs:
            for setting in (0,1):
                name = 'divide-%d-%d-%d' % (a,b,setting)
                probe = run(name)
                if not probe.returncode or probe.stdout or b'division by zero' not in probe.stderr:
                    sys.exit('Expected division-by-zero diagnostic missing: ' + name)
                reports.append({'probe':name,'category':'division-by-zero','from':a,'to':b,'setting':setting})
        probe = run('format-null')
        if not probe.returncode or probe.stdout or b'null pointer' not in probe.stderr:
            sys.exit('Expected null-buffer diagnostic missing')
        reports.append({'probe':'format-null','category':'null-buffer-write'})
        print('TDef golden domain ASan+UBSan clean; 88 divide-by-zero and one null-buffer probe diagnosed; six failure probes verified; LSan disabled under ptrace', file=sys.stderr)
    if mode == '--diagnostics':
        print(json.dumps({'schema':1, 'reference':reference_digest, 'generator':generator_digest, 'probes':reports}, indent=2))
    elif mode == 'all':
        sys.stdout.buffer.write(result.stdout)
PY

#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
# No build command ever writes in the required source snapshot.
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
if mode not in ('all', 'linked-dispatch', 'scalar', 'sse', 'avx', '--audit', '--verify'):
    sys.exit('Usage: run_conv_vector.sh [all|linked-dispatch|scalar|sse|avx|--audit|--verify]')
root = here.parent.parent / 'libosmocore-rs' / 'libosmocore'
manifest_path = here / 'conv_provenance.json'
manifest_bytes = manifest_path.read_bytes()
manifest = json.loads(manifest_bytes)
if manifest['upstream_git_revision'] != 'unknown':
    sys.exit('Unexpected provenance policy')
for name, expected in manifest['sha256'].items():
    file = root / name
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        sys.exit('Reference missing or content mismatch: ' + name)
library = root / manifest['library']
if not library.is_file():
    sys.exit('Required existing local library is missing; no fallback/build allowed')
digest = hashlib.sha256(manifest_bytes).hexdigest()
library_digest = manifest['sha256'][manifest['library']]
for alias in ('libosmocore.so', 'libosmocore.so.22'):
    file = library.parent / alias
    if (not file.is_file() or not file.resolve().is_relative_to(root.resolve()) or
            hashlib.sha256(file.read_bytes()).hexdigest() != library_digest):
        sys.exit('Local linked-library alias missing or mismatched: ' + alias)
generator_bytes = (here / 'conv_generator_provenance.json').read_bytes()
generator = json.loads(generator_bytes)
if generator['reference_manifest_sha256'] != digest:
    sys.exit('Reference manifest differs from pinned generator provenance')
for name, expected in generator['sha256'].items():
    file = here / name
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
        sys.exit('Convolutional generator missing or content mismatch: ' + name)
producer = [generator['sha256']['conv_vector.c'], generator['sha256']['run_conv_vector.sh'],
            hashlib.sha256(generator_bytes).hexdigest()]
env = dict(os.environ, LD_LIBRARY_PATH=str(library.parent), LD_PRELOAD='', LD_AUDIT='')
env.pop('LD_DEBUG', None)
env.pop('LD_PROFILE', None)
symbols = {}
for line in subprocess.check_output(['nm', '-a', str(library)], text=True).splitlines():
    fields = line.split()
    if len(fields) == 3:
        symbols[fields[2]] = fields[0]
names = ['osmo_conv_metrics_k5_n2', 'osmo_conv_gen_metrics_k5_n2',
         'osmo_conv_sse_metrics_k5_n2', 'osmo_conv_sse_avx_metrics_k5_n2']
if any(name not in symbols for name in names):
    sys.exit('Pinned linked library lacks runtime-inspection symbols')
offsets = [symbols[name] for name in names]
if mode == '--verify':
    print('Convolutional reference content verified: ' + digest, file=sys.stderr)
    sys.exit(0)

with tempfile.TemporaryDirectory(prefix='conv-evidence-') as tmp:
    work = Path(tmp)
    cc = ['cc', '-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
          '-I' + str(work), '-I' + str(root), '-isystem', str(root / 'include'),
          '-I' + str(root / 'src/core')]
    # Check transitive project includes against the closed manifest, too.
    units = [here / 'conv_vector.c'] + [root / ('src/core/' + name) for name in
            ('conv.c', 'conv_acc.c', 'conv_acc_generic.c', 'conv_acc_sse.c', 'conv_acc_sse_avx.c')]
    deps = subprocess.check_output(cc + ['-M'] + [str(p) for p in units], text=True)
    for token in deps.replace('\\\n', ' ').split():
        p = Path(token)
        if p.is_absolute() and p.is_relative_to(root):
            if str(p.relative_to(root)) not in manifest['sha256']:
                sys.exit('Unpinned transitive source include: ' + str(p))
    if mode == '--audit':
        cc += ['-DCONV_AUDIT', '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    backends = ['linked-dispatch', 'scalar', 'sse', 'avx'] if mode == 'all' else (
        ['scalar', 'sse', 'avx'] if mode == '--audit' else [mode])
    results = []
    comparable = {}
    for backend in backends:
        binary = work / backend
        command = cc + [str(here / 'conv_vector.c')]
        if backend != 'linked-dispatch':
            # Existing upstream warnings, not harness warnings; no source edits.
            command += ['-Wno-error=sign-compare', '-Wno-error=maybe-uninitialized']
            command += ['-DCONV_ISOLATED', '-mssse3', '-msse4.1', '-mavx2']
            command += [str(root / ('src/core/' + name)) for name in
                        ('conv.c', 'conv_acc_generic.c', 'conv_acc_sse.c', 'conv_acc_sse_avx.c')]
        command += [str(library), '-Wl,-rpath,' + str(library.parent), '-ldl', '-o', str(binary)]
        subprocess.run(command, check=True, env=env)
        evidence = work / (backend + '.ndjson')
        with evidence.open('wb') as output:
            subprocess.run([str(binary), backend, digest, library_digest] + offsets + producer,
                           check=True, env=env, stdout=subprocess.DEVNULL if mode == '--audit' else output)
        if mode != '--audit':
            results.append(evidence)
            comparable[backend] = {}
            for line in evidence.open():
                record = json.loads(line)
                if record.get('api') in ('decode', 'decode_acc'):
                    comparable[backend][(record['case'], record['api'])] = (
                        record['ret'], record['input_after'], record['output_after'])
        if mode == '--audit' and backend == 'scalar':
            for probe, diagnostic in [('diagnostic-shift', 'shift exponent'),
                                      ('diagnostic-overflow', 'signed integer overflow')]:
                result = subprocess.run([str(binary), probe, digest, library_digest] + offsets + producer,
                                        env=env, capture_output=True, text=True)
                if result.returncode == 0 or result.stdout or diagnostic not in result.stderr:
                    sys.exit('Expected isolated UB diagnostic missing: ' + probe + '\n' + result.stderr)
                print('Excluded probe (never golden): ' + probe + '\n' + result.stderr, file=sys.stderr)
    if mode == '--audit':
        print('Convolutional golden domain: scalar/SSE/AVX ASan+UBSan clean; linked dispatcher is not instrumented', file=sys.stderr)
    else:
        if mode == 'all':
            scalar = comparable['scalar']
            for backend, observations in comparable.items():
                for key, observation in observations.items():
                    if observation != scalar[key]:
                        sys.exit('STOP AND REVIEW: backend difference for ' + repr((backend, key)))
        # No partial evidence on compilation, execution, or backend mismatch.
        for evidence in results:
            with evidence.open('rb') as source:
                while chunk := source.read(65536):
                    sys.stdout.buffer.write(chunk)
PY
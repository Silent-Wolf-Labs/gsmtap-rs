#!/usr/bin/env bash
set -euo pipefail
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 - "$script_dir" "${1:-all}" <<'PY'
import hashlib,json,os,resource,signal,subprocess,sys,tempfile
from pathlib import Path
here=Path(sys.argv[1]);mode=sys.argv[2]
if mode not in ('all','--verify','--audit','--diagnostics','--native'):
    sys.exit('Usage: run_rate_ctr_group_vector.sh [all|--verify|--audit|--diagnostics|--native]')
root=here.parent.parent.parent/'libosmocore-rs'/'libosmocore'
ref_bytes=(here/'rate_ctr_group_provenance.json').read_bytes();ref=json.loads(ref_bytes)
if ref['upstream_git_revision']!='unknown' or ref['evidence']!='exact-source':sys.exit('Unexpected provenance policy')
for base,entries in ((root,ref['sha256']),(Path('/'),ref['system_sha256'])):
    for name,expected in entries.items():
        p=base/name
        if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:sys.exit('Reference missing or changed: '+str(p))
ref_sha=hashlib.sha256(ref_bytes).hexdigest()
gen_bytes=(here/'rate_ctr_group_generator_provenance.json').read_bytes();gen=json.loads(gen_bytes)
if gen['reference_manifest_sha256']!=ref_sha:sys.exit('Reference manifest differs from generator provenance')
for name,expected in gen['sha256'].items():
    if hashlib.sha256((here/name).read_bytes()).hexdigest()!=expected:sys.exit('Generator changed: '+name)
gen_sha=hashlib.sha256(gen_bytes).hexdigest()
env=dict(os.environ)
for key in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH'):
    env.pop(key,None)
for flag,expected in ref['compiler'].items():
    if subprocess.check_output(['cc',flag],text=True,env=env).strip()!=expected:sys.exit('Compiler identity mismatch: '+flag)
expand=lambda flags:[x.replace('{reference}',str(root)) for x in flags]
cc=['cc']+expand(ref['compiler_flags'])
units=[here/'rate_ctr_group_vector.c']+[root/x for x in ref['translation_units']]
deps=subprocess.check_output(cc+['-M']+list(map(str,units)),text=True,env=env)
for token in deps.replace('\\\n',' ').split():
    p=Path(token)
    if not p.is_absolute():continue
    if p.is_relative_to(root):
        if str(p.relative_to(root)) not in ref['sha256']:sys.exit('Unpinned local include: '+str(p))
    elif p!=units[0] and str(p).lstrip('/') not in ref['system_sha256']:sys.exit('Unpinned platform include: '+str(p))
macros=subprocess.check_output(cc+['-dM','-E',str(units[0])],env=env)
if hashlib.sha256(macros).hexdigest()!=ref['preprocessor_defines_sha256']:sys.exit('Preprocessing defines changed')
if mode=='--verify':
    print('rate_ctr_group exact-source/flags/defines verified: '+ref_sha,file=sys.stderr);sys.exit(0)
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
with tempfile.TemporaryDirectory(prefix='rate-ctr-group-') as tmp:
    binary=Path(tmp)/'vector';audit=mode in ('--audit','--diagnostics')
    if audit:
        cc+=ref['audit_flags'];env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    subprocess.run(cc+list(map(str,units))+expand(ref['linker_flags'])+['-o',str(binary)],check=True,env=env)
    golden=subprocess.run([str(binary),ref_sha,gen_sha],capture_output=True,timeout=30,env=env)
    if golden.returncode or golden.stderr:sys.exit('Golden domain failed: '+golden.stderr.decode())
    native=[]
    for name in ('init-ok','init-create-fail','init-schedule-fail'):
        p=subprocess.run([str(binary),name],capture_output=True,timeout=5,env=env)
        if p.returncode or p.stderr:sys.exit('Native initialization probe failed: '+name+' '+p.stderr.decode())
        native.append(p.stdout)
    smoke=subprocess.run([str(binary),'native-smoke'],capture_output=True,timeout=5,env=env)
    if smoke.returncode or smoke.stdout or smoke.stderr:sys.exit('Native timerfd/select smoke failed')
    reports=[]
    if audit:
        probes=[('free-current','heap-use-after-free'),('unlink-current','SEGV'),('counter-bounds','heap-buffer-overflow'),('null-descriptor','null pointer'),('short-read','Assert failed')]
        for name,marker in probes:
            p=subprocess.run([str(binary),name],capture_output=True,timeout=5,env=env)
            if not p.returncode or p.stdout or marker.encode() not in p.stderr:sys.exit('Expected diagnostic absent: '+name+' '+p.stderr.decode())
            reports.append({'probe':name,'category':marker})
        for name in ('zero-expirations','unlink-current-init'):
            try:subprocess.run([str(binary),name],capture_output=True,timeout=1,env=env)
            except subprocess.TimeoutExpired:reports.append({'probe':name,'category':'bounded-timeout','seconds':1})
            else:sys.exit('Expected bounded nontermination absent: '+name)
        print('rate_ctr_group successful domain and native probes ASan+UBSan clean; seven isolated diagnostics verified; LSan disabled under ptrace',file=sys.stderr)
    if mode=='all':sys.stdout.buffer.write(golden.stdout)
    elif mode=='--native':
        print('meta\t1\texact-source\tunknown\t'+ref_sha+'\t'+gen_sha+'\t64\t64\t32');sys.stdout.flush()
        for row in native:sys.stdout.buffer.write(row)
    elif mode=='--diagnostics':print(json.dumps({'schema':1,'reference':ref_sha,'generator':gen_sha,'probes':reports},indent=2))
PY

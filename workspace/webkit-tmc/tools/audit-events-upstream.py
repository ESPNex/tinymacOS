#!/usr/bin/env python3
"""Pinned-source requirement audit ONLY. Does not compile or link WebKit."""
from pathlib import Path
import argparse,hashlib,json
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path);args=p.parse_args()
lock=json.loads((r/'upstream-reference/events-reference-lock.json').read_text());pin=json.loads((r/'upstream.lock.json').read_text())
assert lock['commit']==pin['commit']
for name,expected in lock['sha256'].items():assert hashlib.sha256((r/'upstream-reference'/name).read_bytes()).hexdigest()==expected,name
thread=r/'upstream-reference/Source/WTF/wtf/posix/ThreadingPOSIX.cpp'
loop=r/'upstream-reference/Source/WTF/wtf/generic/RunLoopGeneric.cpp'
findings=[]
for file,needle,meaning in [
 (thread,'bool ThreadCondition::timedWait(Mutex& mutex, WallTime absoluteTime)','The existing POSIX WTF condition path expects WALL/REALTIME; native MONOTONIC conditions are NOT a transparent replacement. Requires an explicit upstream/platform adaptation or a validated realtime implementation.'),
 (thread,'pthread_cond_timedwait(&m_condition','New source API covers an explicitly configured MONOTONIC condition only. Default realtime still fails ENOTSUP; do not flip a full pthread/WTF gate.'),
 (thread,'nanosleep(&minimalSleep','Native yield-shaped nanosleep can link with the source time header, at conservative 100Hz granularity; not a 50us/high-resolution scheduler.'),
 (loop,'RedBlackTree<ScheduledTask, MonotonicTime>','Real generic WTF uses monotonic timers, refcounted objects and its own red-black tree, not the new native queue directly.'),
 (loop,'m_readyToRun.waitUntil(m_loopLock, sleepUntil','Real WTF condition/time/locking adaptation and C++/WTF dependency closure remain required.'),
 (loop,'RunLoop::TimerBase::start(Seconds interval, bool repeating)','Recurring timer, TimerBase lifetime, nested run modes and main-thread integration are NOT implemented by the native one-shot queue.'),
]:
 lines=file.read_text().splitlines();hits=[i+1 for i,line in enumerate(lines) if needle in line];assert hits,needle
 findings.append({'file':str(file.relative_to(r/'upstream-reference')),'lines':hits,'needle':needle,'finding':meaning})
record={'passed':True,'kind':'source requirement/hash audit, NOT a build','upstream_commit':lock['commit'],'verified_source_hashes':lock['sha256'],'findings':findings,'actual_WTF_compiled':False,'actual_WTF_RunLoop_integrated':False,'full_pthread_or_clock_realtime':False,'browser_acceptance':False}
if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n')
print('PASS pinned upstream requirement audit: '+str(len(findings))+' checked findings; WallTime vs MONOTONIC gap explicitly retained; NO WTF build claimed')

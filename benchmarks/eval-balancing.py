#!/usr/bin/env python3

import sys
import babeltrace
from collections import defaultdict

def print_line(time, threads):
    print("{0: <10} {1}".format(time, " ".join("{0: <10}".format(tid) for tid in threads)))


def generate_diagram(tracefile):
    # a trace collection holds one to many traces
    col = babeltrace.TraceCollection()

    # add the trace provided by the user
    # (LTTng traces always have the 'ctf' format)
    if col.add_trace(sys.argv[1], 'ctf') is None:
        raise RuntimeError('Cannot add trace')

    base = None
    threads = set()
    switches = []

    for event in col.events:
        if base is None and event.name == 'atlas_job_submit':
            base = event.timestamp

        if base is None:
            continue

        if event.name == 'atlas_job_submit':
            threads.add(event['tid'])

        if event.name != 'sched_switch':
            continue

        cpu = event['cpu_id']
        time = event.timestamp - base

        switches.append({ 'time': time, 'reason': 'D', 'tid': event['prev_tid'], 'cpu': cpu})
        switches.append({ 'time': time, 'reason': 'S', 'tid': event['next_tid'], 'cpu': cpu})
    
    switches = [switch for switch in switches if switch['tid'] in  threads]

    # header
    print_line("time", threads)
    last = dict((thread, float('nan')) for thread in threads)

    for cs in switches:
        time = cs['time']
        tid = cs['tid']
        last[tid] = cs['cpu']
        # print(time, cs)
        print_line(time, [last[tid] for tid in threads])

        if cs['reason'] == 'D':
            last[tid] = float('nan')
            print_line(time, [last[tid] for tid in threads])

if __name__ == '__main__':
    if len(sys.argv) < 2:
        msg = 'Usage: {} TRACEPATH'.format(sys.argv[0])
        raise ValueError(msg)
    
    generate_diagram(sys.argv[1])


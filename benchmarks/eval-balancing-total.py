#!/usr/bin/env python3

import sys
import babeltrace
from collections import defaultdict

def print_line(time, cpus):
    print("{0: <10} {1: <10}".format(time, cpus))

def generate_diagram(tracefile):
    # a trace collection holds one to many traces
    col = babeltrace.TraceCollection()

    # add the trace provided by the user
    # (LTTng traces always have the 'ctf' format)
    if col.add_trace(sys.argv[1], 'ctf') is None:
        raise RuntimeError('Cannot add trace')

    base = None
    timeline = []
    active_cpus = set()
        
    print_line("time", "cpus")

    atlas_threads = set([e['tid'] for e in col.events if e.name == 'atlas_job_submit'])

    for event in col.events:
        if base is None and event.name == 'atlas_job_submit':
            base = event.timestamp
            print_line(0, 0)
            continue

        if base is None:
            continue

        if event.name != 'sched_switch':
            continue

        time = event.timestamp
        cpu = event['cpu_id']

        if event['prev_tid'] in atlas_threads:
            # deselect
            if cpu in active_cpus:
                active_cpus.remove(cpu)

        if event['next_tid'] in atlas_threads:
            # select
            active_cpus.add(event['cpu_id'])
       
        if event['next_tid'] in atlas_threads or event['prev_tid'] in atlas_threads:
            timeline.append({'time': time, 'num_cpus': len(active_cpus)})

    timeline = [{'time': e['time'] - base, 'num_cpus': e['num_cpus']} for e in timeline if e['time'] > base]
    for e in timeline:
        print_line(e['time'], e['num_cpus'])


if __name__ == '__main__':
    if len(sys.argv) < 2:
        msg = 'Usage: {} TRACEPATH'.format(sys.argv[0])
        raise ValueError(msg)
    
    generate_diagram(sys.argv[1])



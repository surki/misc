#!/usr/bin/python

# Utility to trace various golang GC events using BCC/eBPF.
#
# For now, it can trace GC allocations (and can generate flamegraph out of
# it)

from __future__ import print_function
from bcc import BPF
import argparse
import sys
import os
import errno
from time import sleep

args = None

def parse_args():
    global args
    examples = """examples:
    """

    parser = argparse.ArgumentParser(
        description="Trace Go lang heap operatoins",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument("-p", "--pid", type=int, help="Trace only this PID")
    parser.add_argument("-e", "--exe",
                        help="Trace processes matching this exe. if exe is not in PATH, it should point to full path")
    parser.add_argument("-a", "--trace-allocations", dest='trace_allocations', action='store_true',
                        help="trace heap allocations")
    parser.add_argument("-g", "--trace-gc", dest='trace_gc', action='store_true',
                        help="trace gc operations")
    parser.add_argument("-f", "--folded", action="store_true",
                        help="output folded format, one line per stack (for flame graphs)")
    parser.add_argument("-d", "--debug", action="store_true",
                        help="print BPF program before starting (for debugging purposes)")
    parser.set_defaults(trace_allocations=True)
    args = parser.parse_args()

    if not args.pid and not args.exe:
        raise Exception("Either the executable name (--exe option) or pid (--pid option) must be set")

    if not args.pid:
        args.exepath = BPF.find_exe(args.exe)
    else:
        args.exepath = os.path.realpath("/proc/%d/exe" % args.pid)

    if args.exepath == None or not os.path.isfile(args.exepath):
        raise Exception("exe \"%s\" doesn't exist, make sure you specify full path or it is in PATH env variable" % args.exe)

def create_heap_allocation_probes():
    global bpf
    global args
    global matched

    # load BPF program
    bpf_text = """
#include <uapi/linux/ptrace.h>
#include <linux/sched.h>

struct key_t {
    u32 pid;
    int stackid;
    char name[TASK_COMM_LEN];
};

BPF_HASH(counts, struct key_t);
BPF_STACK_TRACE(stack_traces, 4096);

int trace_count(void *ctx) {
    struct key_t key = {};

    key.pid = bpf_get_current_pid_tgid() >> 32;
    key.stackid = stack_traces.get_stackid(ctx, BPF_F_REUSE_STACKID| BPF_F_USER_STACK);
    bpf_get_current_comm(&key.name, sizeof(key.name));

    u64 zero = 0;
    u64 *val = counts.lookup_or_init(&key, &zero);
    (*val)++;

    return 0;
}
        """

    bpf = BPF(text=bpf_text)
    heap_alloc_sym_pattern="^runtime.mallocgc$"

    if args.debug:
        print(bpf_text)

    bpf.attach_uprobe(name=args.exepath,
                      sym_re=heap_alloc_sym_pattern,
                      fn_name="trace_count",
                      pid=args.pid or -1)
    matched = bpf.num_open_uprobes()

    if matched == 0:
        raise Exception("No functions matched by pattern %s" %
                        heap_alloc_sym_pattern)

def print_stacks():
    counts = bpf["counts"]
    stack_traces = bpf["stack_traces"]
    missing_stacks = 0
    has_enomem = False

    for k, v in sorted(counts.items(),
                       key=lambda counts: counts[1].value):

        if k.stackid < 0 and k.stackid != -errno.EFAULT:
            missing_stacks += 1

        if k.stackid == -errno.ENOMEM:
            has_enomem = True

        stack = [] if k.stackid < 0 else stack_traces.walk(k.stackid)

        if args.folded:
            stack = list(stack)
            line = [k.name.decode()] + \
                   [bpf.sym(addr, k.pid) for addr in reversed(stack)]
            print("%s %d" % (";".join(line), v.value))
        else:
            for addr in stack:
                print("    %s" % bpf.sym(addr, k.pid))
            print("    -- %s (%d)" % (k.name, k.pid))
            print("    %d\n" % v.value)

def heap_allocation_event_loop():
    print("Press Ctrl-C to display/exit", file=sys.stderr)
    while True:
        try:
            sleep(100)
        except KeyboardInterrupt:
            print_stacks()
            exit()

def check_kernel_support():
    Release, Revision, Name = os.uname()[2].split("-")
    Version = map(int, Release.split(".") + [Revision])

    if not (Version[0] >= 4 and Version[1] >= 5):
        print()
        print("Kernel version 4.5 or above required (that has BPF stacktrace count feature)"
              "you are on %s" % Release)
        print()

        print("You can use \"perf probe\" to trace gc allocations")
        print("  perf probe -x %s runtime.mallocgc")
        print("  perf record -a -g -e probe_name_printed_from_above_command")
        print()
        print()

        raise Exception("Kernel version 4.5 or above required")

parse_args()

check_kernel_support()

if args.trace_allocations:
    create_heap_allocation_probes()
    heap_allocation_event_loop()
elif args.trace_gc:
    raise Exception("GC operation tracing is not yet supported")
else:
    print("No options given, please see --help")

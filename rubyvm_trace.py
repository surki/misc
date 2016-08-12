#!/usr/bin/python -u

# Utility to trace various Ruby VM events using BCC/eBPF.
#
# For now, it can display GC mark phase latency, method cache hit/miss rate
# and summarize objects created by their type

from __future__ import print_function
from bcc import BPF, USDT
from time import strftime
import ctypes as ct
import argparse
import sys
from time import sleep

def parse_args():
    global args
    examples = """examples:

    rubyvm_trace --trace-gc --pid=1234                # trace GC timings for pid 1234
    rubyvm_trace --trace-object-creation --pid=1234   # trace object creations for pid 1234
    rubyvm_trace --trace-method-cache --pid=1234      # trace method cache hit/miss rate for pid 1234
    rubyvm_trace --trace-method-cache                 # trace method cache hit/miss rate for all ruby processes
    """

    parser = argparse.ArgumentParser(
        description="Trace Ruby VM operations",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument("-p", "--pid", type=int, default='-1',
                        help="trace this PID only")
    parser.add_argument("-g", "--trace-gc", dest='tracegc', action='store_true',
                        help="trace GC operations")
    parser.add_argument("-o", "--trace-object-creation", dest='traceobjectcreation', action='store_true',
                        help="trace object creations")
    parser.add_argument("-m", "--trace-method-cache", dest='tracemethodcache', action='store_true',
                        help="trace method cache usage")

    if len(sys.argv[1:])==0:
        parser.print_help()
        parser.exit()

    args = parser.parse_args()

args = None
TASK_COMM_LEN = 16    # linux/sched.h

class Data(ct.Structure):
    _fields_ = [
        ("pid", ct.c_ulonglong),
        ("ts", ct.c_ulonglong),
        ("delta", ct.c_ulonglong),
        ("comm", ct.c_char * TASK_COMM_LEN)
    ]

def do_gc_probes():
    # load BPF program
    bpf_text = """
    #include <uapi/linux/ptrace.h>
    #include <linux/sched.h>
     
    struct val_t {
        u32 pid;
        char comm[TASK_COMM_LEN];
        u64 ts;
    };
     
    struct data_t {
        u32 pid;
        u64 ts;
        u64 delta;
        char comm[TASK_COMM_LEN];
    };
     
    BPF_HASH(start, u32, struct val_t);
    BPF_PERF_OUTPUT(events);
     
    int do_entry(struct pt_regs *ctx) {
        if (!PT_REGS_PARM1(ctx))
            return 0;

        struct val_t val = {};
        u32 pid = bpf_get_current_pid_tgid();

        if (bpf_get_current_comm(&val.comm, sizeof(val.comm)) == 0) {
            val.pid = bpf_get_current_pid_tgid();
            val.ts = bpf_ktime_get_ns();
            start.update(&pid, &val);
        }

        return 0;
    }
     
    int do_return(struct pt_regs *ctx) {
        struct val_t *valp;
        struct data_t data = {};
        u64 delta;
        u32 pid = bpf_get_current_pid_tgid();
     
        u64 tsp = bpf_ktime_get_ns();
     
        valp = start.lookup(&pid);
        if (valp == 0)
            return 0;       // missed start
     
        bpf_probe_read(&data.comm, sizeof(data.comm), valp->comm);
        data.pid = valp->pid;
        data.delta = tsp - valp->ts;
        data.ts = tsp / 1000;
        if (data.delta > 0)
            events.perf_submit(ctx, &data, sizeof(data));
        start.delete(&pid);
        return 0;
    }
    """

    u = USDT(pid=args.pid)
    u.enable_probe(probe="gc__mark__begin", fn_name="do_entry")
    u.enable_probe(probe="gc__mark__end", fn_name="do_return")

    b = BPF(text=bpf_text ,usdt=u)
    # b.attach_uprobe(name="ruby", sym="gc_start_internal", fn_name="do_entry", pid=args.pid)
    # b.attach_uretprobe(name="ruby", sym="gc_start_internal", fn_name="do_return", pid=args.pid)

    print("%-9s %-6s %-16s %10s" % ("TIME", "PID", "COMM", "LATms"))
    b["events"].open_perf_buffer(print_gc_event)

    try:
        while 1:
            b.kprobe_poll()
    except:
        pass

    # b.detach_uprobe(name="ruby", sym="gc_start_internal")
    # b.detach_uprobe(name="ruby", sym="gc_start_internal")

def print_gc_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    print("%-9s %-6d %-16s %10.2f" % (strftime("%H:%M:%S"), event.pid,
                                      event.comm.decode(), (event.delta / 1000000)))

def do_object_creation_probes():
    # load BPF program
    bpf_text = """
    #include <uapi/linux/ptrace.h>

    struct key_t {
        char objectname[128];
    };

    BPF_HASH(objmap, struct key_t, u64);

    int do_count(struct pt_regs *ctx) {
        struct key_t key = {};

        bpf_probe_read(&key.objectname, sizeof(key.objectname), (void *)(unsigned long)ctx->r13);
        objmap.increment(key);

        return 0;
    }
    """

    u = USDT(pid=args.pid)
    u.enable_probe(probe="object__create", fn_name="do_count")

    b = BPF(text=bpf_text, usdt=u)

    print("Press Ctrl-C to display/exit")
    try:
        sleep(99999999)
    except KeyboardInterrupt:
        pass

    data = b.get_table("objmap")
    data = sorted(data.items(), key=lambda kv: kv[1].value)
    for key, value in data:
        print("\t%-10s %s" % \
              (str(value.value), str(key.objectname.decode())))

class Data(ct.Structure):
    _fields_ = [
        ("pid", ct.c_ulonglong),
        ("ts", ct.c_ulonglong),
        ("delta", ct.c_ulonglong),
        ("comm", ct.c_char * TASK_COMM_LEN)
    ]
    
def do_method_cache_probes():
    # load BPF program
    bpf_text = """
    #include <uapi/linux/ptrace.h>
    #include <linux/sched.h>

    struct key_t {
        u32 pid;
    };

    struct val_t {
        u32 pid;
        u64 method_get;
        u64 method_get_without_cache;
    };

    BPF_HASH(methodstat, struct key_t, struct val_t);

    int method_get_entry(struct pt_regs *ctx) {
        struct key_t key = {};
        struct val_t val = {};
        struct val_t *valp;
        u32 pid;

        key.pid = bpf_get_current_pid_tgid();
        valp = methodstat.lookup_or_init(&key, &val);
        valp->method_get++;

        return 0;
    }

    int method_get_entry_without_cache(struct pt_regs *ctx) {
        struct key_t key = {};
        struct val_t val = {};
        struct val_t *valp;
        u32 pid;

        key.pid = bpf_get_current_pid_tgid();
        valp = methodstat.lookup_or_init(&key, &val);
        valp->method_get_without_cache++;

        return 0;
    }
    """

    b = BPF(text=bpf_text)

    b.attach_uprobe(name="ruby", sym="rb_method_entry", fn_name="method_get_entry", pid=args.pid)
    b.attach_uprobe(name="ruby", sym="rb_method_entry_get_without_cache", fn_name="method_get_entry_without_cache", pid=args.pid)

    print("Press Ctrl-C to display/exit")
    try:
        sleep(99999999)
    except KeyboardInterrupt:
        pass

    data = b.get_table("methodstat")
    print("Total items = %d" % len(data.items()))
    print("\n%-6s %-15s %-15s %-15s" % ("PID", "METHOD HIT", "METHOD MISS", "HIT RATE"))
    for key, value in data.items():
        method_hit = value.method_get - value.method_get_without_cache
        method_miss = value.method_get_without_cache
        hit_rate = (float(method_hit) / float(method_hit + method_miss)) * 100
        print("%-6d %-15d %-15d %-15.2f" % (key.pid, method_hit, method_miss, hit_rate))

    b.detach_uprobe(name="ruby", sym="rb_method_entry")
    b.detach_uprobe(name="ruby", sym="rb_method_entry_get_without_cache")

parse_args()

if args.traceobjectcreation:
    do_object_creation_probes()
elif args.tracemethodcache:
    do_method_cache_probes()
elif args.tracegc:
    do_gc_probes()

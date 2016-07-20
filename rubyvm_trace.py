#!/usr/bin/python

# Utility to trace various Ruby VM events using BCC/eBPF.
#
# For now, it can display GC mark phase latency and summarize objects
# created by their type

from __future__ import print_function
from bcc import BPF, USDTReader
from time import strftime
import ctypes as ct
import argparse
import sys
from time import sleep

args = None
TASK_COMM_LEN = 16    # linux/sched.h

gc_mark_begin_probe = None
gc_mark_end_probe = None
gc_sweep_begin_probe = None
gc_sweep_end_probe = None

class Data(ct.Structure):
    _fields_ = [
        ("pid", ct.c_ulonglong),
        ("ts", ct.c_ulonglong),
        ("delta", ct.c_ulonglong),
        ("comm", ct.c_char * TASK_COMM_LEN)
    ]

def parse_args():
    global args
    examples = """examples:
    """

    parser = argparse.ArgumentParser(
        description="Trace Ruby VM operations",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
    parser.add_argument("-p", "--pid", type=int, default='-1',
                        help="trace this PID only")
    parser.add_argument("-g", "--trace-gc", dest='tracegc', action='store_true',
                        help="trace GC operations")
    parser.add_argument("-o", "--trace-objcreation", dest='traceobjcreation', action='store_true',
                        help="trace object creations")
    parser.set_defaults(tracegc=True)
    args = parser.parse_args()

def create_gc_probes():
    global b
    global gc_mark_begin_probe
    global gc_mark_end_probe
    global gc_sweep_begin_probe
    global gc_sweep_end_probe

    reader = USDTReader(pid=args.pid)

    for probe in reader.probes:
        if probe.name == "gc__mark__begin":
            gc_mark_begin_probe = probe
        elif probe.name == "gc__mark__end":
            gc_mark_end_probe = probe
        elif probe.name == "gc__sweep__begin":
            gc_sweep_begin_probe = probe
        elif probe.name == "gc__sweep__end":
            gc_sweep_end_probe = probe

    if None in (gc_mark_begin_probe, gc_mark_end_probe, gc_sweep_begin_probe, gc_sweep_end_probe):
        raise ValueError("Probes not found, is dtrace enabled in the ruby?")

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
    b = BPF(text=bpf_text)

    if gc_mark_begin_probe.need_enable():
        gc_mark_begin_probe.enable(args.pid)

    if gc_mark_end_probe.need_enable():
        gc_mark_end_probe.enable(args.pid)

    if gc_sweep_begin_probe.need_enable():
        gc_sweep_begin_probe.enable(args.pid)

    if gc_sweep_end_probe.need_enable():
        gc_sweep_end_probe.enable(args.pid)

    b.attach_uprobe(name=gc_mark_begin_probe.bin_path, addr=gc_mark_begin_probe.locations[0].address, fn_name="do_entry", pid=args.pid)
    b.attach_uprobe(name=gc_mark_end_probe.bin_path, addr=gc_mark_end_probe.locations[0].address, fn_name="do_return", pid=args.pid)

    # b.attach_uprobe(name="ruby", sym="garbage_collect", fn_name="do_entry", pid=args.pid)
    # b.attach_uretprobe(name="ruby", sym="garbage_collect", fn_name="do_return", pid=args.pid)
    # b.attach_uprobe(name=gc_sweep_begin_probe.bin_path, addr=gc_sweep_begin_probe.locations[0].address, fn_name="do_entry", pid=args.pid)
    # b.attach_uprobe(name=gc_sweep_end_probe.bin_path, addr=gc_sweep_end_probe.locations[0].address, fn_name="do_return", pid=args.pid)

def create_object_creation_probes():
    global b
    global gc_object_create_probe

    reader = USDTReader(pid=args.pid)

    for probe in reader.probes:
        if probe.name == "object__create":
            gc_object_create_probe = probe

    if gc_object_create_probe is None:
        raise ValueError("Probes not found, is dtrace enabled in the ruby?")

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
    b = BPF(text=bpf_text)

    if gc_object_create_probe.need_enable():
        gc_object_create_probe.enable(args.pid)

    b.attach_uprobe(name=gc_object_create_probe.bin_path, addr=gc_object_create_probe.locations[0].address, fn_name="do_count", pid=args.pid)
    
def print_gc_event(cpu, data, size):
    event = ct.cast(data, ct.POINTER(Data)).contents
    print("%-9s %-6d %-16s %10.2f" % (strftime("%H:%M:%S"), event.pid,
                                      event.comm.decode(), (event.delta / 1000000)))

def objcreation_event_loop():
    print("Press Ctrl-C to display/exit")
    while True:
        try:
            sleep(100)
        except KeyboardInterrupt:
            data = b.get_table("objmap")
            data = sorted(data.items(), key=lambda kv: kv[1].value)
            for key, value in data:
                # Print some nice values if the user didn't
                # specify an expression to probe
                print("\t%-10s %s" % \
                      (str(value.value), str(key.objectname.decode())))
            exit()
    gc_object_create_probe.disable(args.pid)
    
def gc_event_loop():
    print("%-9s %-6s %-16s %10s" % ("TIME", "PID", "COMM", "LATms"))
    b["events"].open_perf_buffer(print_gc_event)
    try:
        while 1:
            b.kprobe_poll()
    except:
        pass

    gc_mark_begin_probe.disable(args.pid)
    gc_mark_end_probe.disable(args.pid)
    gc_sweep_begin_probe.disable(args.pid)
    gc_sweep_end_probe.disable(args.pid)

parse_args()

if args.traceobjcreation:
    create_object_creation_probes()
    objcreation_event_loop()
elif args.tracegc:
    create_gc_probes()
    gc_event_loop()

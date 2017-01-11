from __future__ import print_function
import re
import sys

goobjfile = gdb.current_objfile() or gdb.objfiles()[0]
goobjfile.pretty_printers = []

goobjfile.pretty_printers.extend([makematcher(var) for var in vars().values() if hasattr(var, 'pattern')])

class SliceValue:
    "Wrapper for slice values."

    def __init__(self, val):
        self.val = val

    @property
    def len(self):
        return int(self.val['len'])

    @property
    def cap(self):
        return int(self.val['cap'])

    def __getitem__(self, i):
        if i < 0 or i >= self.len:
            raise IndexError(i)
        ptr = self.val["array"]
        return (ptr + i).dereference()

def print_stack(pc, sp, ptr, pat=None):
    save_frame = gdb.selected_frame()
    gdb.parse_and_eval('$save_sp = $sp')
    gdb.parse_and_eval('$save_pc = $pc')
    gdb.parse_and_eval('$sp = {0}'.format(str(sp)))
    gdb.parse_and_eval('$pc = {0}'.format(str(pc)))
    printed = False
    try:
        if pat:
            val = gdb.execute("bt", to_string=True)
            if val.lower().find(pat.lower()) == -1:
                return
            print('==== goid :', ptr['goid'])
            print(val)
            printed = True
        else:
            gdb.execute("bt")
            printed = True
    finally:
        gdb.parse_and_eval('$sp = $save_sp')
        gdb.parse_and_eval('$pc = $save_pc')
        save_frame.select()
    return printed

class GoroutinesBtCmd(gdb.Command):
    "Print all goroutine backtraces"

    def __init__(self):
        gdb.Command.__init__(self, "btgoroutines", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        allgs = gdb.parse_and_eval("'runtime.allgs'")
        vp = gdb.lookup_type('void').pointer()
        c=0
        for ptr in SliceValue(allgs):
            if ptr['atomicstatus'] == 6:  # 'gdead'
                continue
            pc, sp = (ptr['sched'][x].cast(vp) for x in ('pc', 'sp'))
            try:
                pc = int(pc)
            except gdb.error:
                pc = int(str(pc).split(None, 1)[0], 16)
            if print_stack(pc, sp, ptr, arg):
                c+=1
        print("total goroutines (or matched):", c)

GoroutinesBtCmd()

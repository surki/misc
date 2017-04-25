from __future__ import print_function
import re
import sys

class AnalyzeSegFaultCmd(gdb.Command):
    "Analyze ruby segfault info"

    def __init__(self):
        gdb.Command.__init__(self, "analyze_ruby_fault", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        for i,inferior in enumerate(gdb.inferiors()):
            if not inferior.is_valid():
                continue

            found_segfault = False

            for j,thread in enumerate(inferior.threads()):
                thread.switch()
                #print("Processing thread %s" % thread.ptid[1])
                frame = gdb.selected_frame()
                while ((frame is not None) and (frame.type() != gdb.SIGTRAMP_FRAME)):
                    frame = frame.older()

                if frame is None:
                    continue

                sigsegv_frame = frame.newer()
                if (sigsegv_frame is None) or sigsegv_frame.name() != "sigsegv":
                    continue

                found_segfault = True
                print("Found segfault frame for pid %s" % inferior.pid)
                sigsegv_frame.select()

                fault_addr = gdb.parse_and_eval("info->si_addr")
                reg_err = gdb.parse_and_eval("((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]")
                reg_err = reg_err.cast(gdb.lookup_type('long long'))

                print("Faulting address : %s" % fault_addr)
                sys.stdout.write("Fault reason : %s (" % hex(int(reg_err)))
                if reg_err & 0x1:
                    sys.stdout.write("ProtectionFault")
                else:
                    sys.stdout.write("NoPageFound")

                if reg_err & 0x2:
                    sys.stdout.write(" WriteAccess")
                else:
                    sys.stdout.write(" ReadAccess")    

                if reg_err & 0x4:
                    sys.stdout.write(" UserMode")
                else:
                    sys.stdout.write(" KernelMode")
                print(")")

            if not found_segfault:
                print("Couldn't find segfault frame for pid %s" % inferior.pid)

class RubyStackTraceCmd(gdb.Command):
    "Print Ruby stacktrace"                

    def __init__(self):
        gdb.Command.__init__(self, "bt_ruby", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        argv = gdb.string_to_argv(arg)
        folded = False
        if len(argv) > 0 and argv[0] == 'folded':
            folded = True
        get_ruby_stacktrace(folded=folded)

class RubyLocalVariablesCmd(gdb.Command):
    "Print Ruby local variables, by walking stack"

    def __init__(self):
        gdb.Command.__init__(self, "bt_ruby_locals", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        get_ruby_localvariables()

control_frame_t = gdb.lookup_type('rb_control_frame_t')
string_t = gdb.lookup_type('struct RString')
ary_t = gdb.lookup_type('struct RArray')

def get_rstring(addr):
  s = addr.cast(string_t.pointer())
  if s['basic']['flags'] & (1 << 13):
    return s['as']['heap']['ptr'].string()
  else:
    return s['as']['ary'].string()

def get_lineno(iseq, pos):
  if pos != 0:
    pos -= 1
  t = iseq['line_info_table']
  t_size = iseq['line_info_size']

  if t_size == 0:
    return 0
  elif t_size == 1:
    return t[0]['line_no']

  for i in range(0, int(t_size)):
    if pos == t[i]['position']:
      return t[i]['line_no']
    elif t[i]['position'] > pos:
      return t[i-1]['line_no']

  return t[t_size-1]['line_no']

def get_ruby_stacktrace(th=None, folded=False):
  if th == None:
    th = gdb.parse_and_eval('ruby_current_thread')
  else:
    th = gdb.parse_and_eval('(rb_thread_t *) %s' % th)

  last_cfp = th['cfp']
  start_cfp = (th['stack'] + th['stack_size']).cast(control_frame_t.pointer()) - 2
  size = start_cfp - last_cfp + 1
  cfp = start_cfp
  call_stack = []
  for i in range(0, int(size)):
    if cfp['iseq'].dereference().address != 0:
      if cfp['pc'].dereference().address != 0:
        s = "{}:{}:in `{}'".format(get_rstring(cfp['iseq']['location']['path']),
          get_lineno(cfp['iseq'], cfp['pc'] - cfp['iseq']['iseq_encoded']),
          get_rstring(cfp['iseq']['location']['label']))
        call_stack.append(s)

    cfp -= 1

  for i in reversed(call_stack):
      if folded:
          sys.stdout.write("%s;" % i)
      else:
          print(i)

  if folded:
      print()

ID_ENTRY_STR = 0
ID_ENTRY_SYM = 1
ID_ENTRY_SIZE = 2

ID_ENTRY_UNIT = 512

FL_USHIFT   = 12
FL_USER0    = 1<<FL_USHIFT+0
FL_USER1    = 1<<FL_USHIFT+1
FL_USER2    = 1<<FL_USHIFT+2
FL_USER3    = 1<<FL_USHIFT+3
FL_USER4    = 1<<FL_USHIFT+4
FL_USER5    = 1<<FL_USHIFT+5
FL_USER6    = 1<<FL_USHIFT+6
FL_USER7    = 1<<FL_USHIFT+7
FL_USER8    = 1<<FL_USHIFT+8
FL_USER9    = 1<<FL_USHIFT+9
FL_USER10   = 1<<FL_USHIFT+10
FL_USER11   = 1<<FL_USHIFT+11
FL_USER12   = 1<<FL_USHIFT+12
FL_USER13   = 1<<FL_USHIFT+13
FL_USER14   = 1<<FL_USHIFT+14
FL_USER15   = 1<<FL_USHIFT+15
FL_USER16   = 1<<FL_USHIFT+16
FL_USER17   = 1<<FL_USHIFT+17
FL_USER18   = 1<<FL_USHIFT+18
FL_USER19   = 1<<FL_USHIFT+19

RARRAY_EMBED_FLAG = FL_USER1
RARRAY_EMBED_LEN_SHIFT = FL_USHIFT+3
RARRAY_EMBED_LEN_MASK = (FL_USER4|FL_USER3)

tLAST_OP_ID = gdb.parse_and_eval('tLAST_OP_ID')
ID_SCOPE_SHIFT = gdb.parse_and_eval('RUBY_ID_SCOPE_SHIFT')
RUBY_Qfalse = gdb.parse_and_eval('RUBY_Qfalse')
RUBY_Qtrue = gdb.parse_and_eval('RUBY_Qtrue')
RUBY_Qnil = gdb.parse_and_eval('RUBY_Qnil')
RUBY_Qundef = gdb.parse_and_eval('RUBY_Qundef')

def _rb_id2str(id):
    str = _lookup_id_str(id)
    return str

def _lookup_id_str(id):
    return _get_id_entry(_id_to_serial(id), ID_ENTRY_STR);

def _id_to_serial(id):
    if _is_notop_id(id):
        return id >> ID_SCOPE_SHIFT
    else:
        return id

def _is_notop_id(id):
    return id > tLAST_OP_ID

def _get_id_entry(num, id_entry_type):
    global_symbols = gdb.parse_and_eval('global_symbols')
    if (num != 0 and num <= global_symbols['last_id']):
        idx = num / ID_ENTRY_UNIT
        ids = global_symbols['ids']
        ary = _rb_ary_entry(ids, idx)
        if (idx < _RARRAY_LEN(ids) and (ary != RUBY_Qnil)):
            ary = _rb_ary_entry(ary, (num % ID_ENTRY_UNIT) * ID_ENTRY_SIZE + id_entry_type)
            if (ary != RUBY_Qnil):
                return ary

    return None

def _RARRAY_LEN(ary):
    a = ary.cast(ary_t.pointer())
    if a['basic']['flags'] & RARRAY_EMBED_FLAG:
        return (a['basic']['flags'] >> RARRAY_EMBED_LEN_SHIFT) & (RARRAY_EMBED_LEN_MASK >> RARRAY_EMBED_LEN_SHIFT)
    else:
        return a['as']['heap']['len']

def _rb_ary_entry(ary, offset):
    if (offset < 0):
        offset += _RARRAY_LEN(ary)

    return _rb_ary_elt(ary, offset)


def _rb_ary_elt(ary, offset):
    len = _RARRAY_LEN(ary)
    if len == 0:
        return None
    if offset < 0 or len <= offset:
        return None

    return _RARRAY_AREF(ary, offset);

def _RARRAY_AREF(a, i):
    return _RARRAY_CONST_PTR(a)[i]

def _RARRAY_CONST_PTR(ary):
    a = ary.cast(ary_t.pointer())
    if a['basic']['flags'] & RARRAY_EMBED_FLAG:
        return a['basic']['ary']
    else:
        return a['as']['heap']['ptr']

def get_ruby_localvariables(th=None):
  if th == None:
    th = gdb.parse_and_eval('ruby_current_thread')
  else:
    th = gdb.parse_and_eval('(rb_thread_t *) %s' % th)

  last_cfp = th['cfp']
  start_cfp = (th['stack'] + th['stack_size']).cast(control_frame_t.pointer()) - 2
  size = start_cfp - last_cfp + 1
  cfp = start_cfp
  for i in range(0, int(size)):
    if cfp['iseq'].dereference().address != 0:
      if cfp['pc'].dereference().address != 0:
        s = "{}:{}:in `{}'".format(get_rstring(cfp['iseq']['location']['path']),
          get_lineno(cfp['iseq'], cfp['pc'] - cfp['iseq']['iseq_encoded']),
                                   get_rstring(cfp['iseq']['location']['label']))
        print(s)
        local_table_size = (cfp['iseq']['local_table_size'])
        print("local table size = %s" % local_table_size)
        for j in range(0, int(local_table_size)):
            id = cfp['iseq']['local_table'][j]
            l = _rb_id2str(id)
            if l is not None:
                print(get_rstring(l))
    cfp -= 1

AnalyzeSegFaultCmd()       
RubyStackTraceCmd()
RubyLocalVariablesCmd()

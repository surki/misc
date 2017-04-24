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

            for j,thread in enumerate(inferior.threads()):
                thread.switch()
                print("Processing thread %s" % thread.ptid[1])
                frame = gdb.selected_frame()
                while ((frame is not None) and (frame.type() != gdb.SIGTRAMP_FRAME)):
                    frame = frame.older()

                if frame is None:
                    print("\tCouldn't find segfault frame")
                    continue

                sigsegv_frame = frame.newer()
                if (sigsegv_frame is None) or sigsegv_frame.name() != "sigsegv":
                    print("\tFound signal frame, but is not segfault")
                    continue

                print("\tFound segfault frame")
                sigsegv_frame.select()

                fault_addr = gdb.parse_and_eval("info->si_addr")
                reg_err = gdb.parse_and_eval("((ucontext_t *)ctx)->uc_mcontext.gregs[REG_ERR]")
                reg_err = reg_err.cast(gdb.lookup_type('long long'))

                print("\tFaulting address : %s" % fault_addr)
                sys.stdout.write("\tFault reason : %s (" % hex(int(reg_err)))
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

class RubyStackTraceCmd(gdb.Command):
    "Print Ruby stacktrace"                

    string_t = None

    def __init__(self):
        gdb.Command.__init__(self, "bt_ruby", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        argv = gdb.string_to_argv(arg)
        folded = False
        if len(argv) > 0 and argv[0] == 'folded':
            folded = True
        get_ruby_stacktrace(folded=folded)

# Based on https://gist.github.com/csfrancis/11376304        
string_t = None

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
  global string_t

  try:
    control_frame_t = gdb.lookup_type('rb_control_frame_t')
    string_t = gdb.lookup_type('struct RString')
  except gdb.error:
    raise gdb.GdbError ("ruby extension requires symbols")

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

AnalyzeSegFaultCmd()       
RubyStackTraceCmd()

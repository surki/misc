from __future__ import print_function
import re
import sys
import ctypes

VALUE_t =  gdb.lookup_type('VALUE')
control_frame_t = gdb.lookup_type('rb_control_frame_t')
RString_t = gdb.lookup_type('struct RString')
RArray_t = gdb.lookup_type('struct RArray')
RBasic_t = gdb.lookup_type('struct RBasic')
RHash_t = gdb.lookup_type('struct RHash')
RObject_t = gdb.lookup_type('struct RObject')
RClass_t = gdb.lookup_type('struct RClass')
global_entry_t = gdb.lookup_type('struct rb_global_entry')
rb_thread_struct_t = gdb.lookup_type('struct rb_thread_struct')

try:
    ID_ENTRY_STR = gdb.parse_and_eval('ID_ENTRY_STR')
    ID_ENTRY_SYM = gdb.parse_and_eval('ID_ENTRY_SYM')
    ID_ENTRY_SIZE = gdb.parse_and_eval('ID_ENTRY_SIZE')
    ID_ENTRY_UNIT = gdb.parse_and_eval('ID_ENTRY_UNIT')
except:
    ID_ENTRY_STR = 0
    ID_ENTRY_SYM = 1
    ID_ENTRY_SIZE = 2
    ID_ENTRY_UNIT =512

try:
    RUBY_FL_USHIFT = gdb.parse_and_eval('RUBY_FL_USHIFT')
    RUBY_FL_USER0  = gdb.parse_and_eval('RUBY_FL_USER0')
    RUBY_FL_USER1  = gdb.parse_and_eval('RUBY_FL_USER1')
    RUBY_FL_USER2  = gdb.parse_and_eval('RUBY_FL_USER2')
    RUBY_FL_USER3  = gdb.parse_and_eval('RUBY_FL_USER3')
    RUBY_FL_USER4  = gdb.parse_and_eval('RUBY_FL_USER4')
    RUBY_FL_USER5  = gdb.parse_and_eval('RUBY_FL_USER5')
    RUBY_FL_USER6  = gdb.parse_and_eval('RUBY_FL_USER6')
    RUBY_FL_USER7  = gdb.parse_and_eval('RUBY_FL_USER7')
    RUBY_FL_USER8  = gdb.parse_and_eval('RUBY_FL_USER8')
    RUBY_FL_USER9  = gdb.parse_and_eval('RUBY_FL_USER9')
    RUBY_FL_USER10 = gdb.parse_and_eval('RUBY_FL_USER10')
    RUBY_FL_USER11 = gdb.parse_and_eval('RUBY_FL_USER11')
    RUBY_FL_USER12 = gdb.parse_and_eval('RUBY_FL_USER12')
    RUBY_FL_USER13 = gdb.parse_and_eval('RUBY_FL_USER13')
    RUBY_FL_USER14 = gdb.parse_and_eval('RUBY_FL_USER14')
    RUBY_FL_USER15 = gdb.parse_and_eval('RUBY_FL_USER15')
    RUBY_FL_USER16 = gdb.parse_and_eval('RUBY_FL_USER16')
    RUBY_FL_USER17 = gdb.parse_and_eval('RUBY_FL_USER17')
    RUBY_FL_USER18 = gdb.parse_and_eval('RUBY_FL_USER18')
    RUBY_FL_USER19 = gdb.parse_and_eval('RUBY_FL_USER19')
    RUBY_FL_SINGLETON = gdb.parse_and_eval('RUBY_FL_SINGLETON')
except:
    RUBY_FL_USHIFT = 12
    RUBY_FL_USER0 = (1<<RUBY_FL_USHIFT+0)
    RUBY_FL_USER1 = (1<<RUBY_FL_USHIFT+1)
    RUBY_FL_USER2 = (1<<RUBY_FL_USHIFT+2)
    RUBY_FL_USER3 = (1<<RUBY_FL_USHIFT+3)
    RUBY_FL_USER4 = (1<<RUBY_FL_USHIFT+4)
    RUBY_FL_USER5 = (1<<RUBY_FL_USHIFT+5)
    RUBY_FL_USER6 = (1<<RUBY_FL_USHIFT+6)
    RUBY_FL_USER7 = (1<<RUBY_FL_USHIFT+7)
    RUBY_FL_USER8 = (1<<RUBY_FL_USHIFT+8)
    RUBY_FL_USER9 = (1<<RUBY_FL_USHIFT+9)
    RUBY_FL_USER10 = (1<<RUBY_FL_USHIFT+10)
    RUBY_FL_USER11 = (1<<RUBY_FL_USHIFT+11)
    RUBY_FL_USER12 = (1<<RUBY_FL_USHIFT+12)
    RUBY_FL_USER13 = (1<<RUBY_FL_USHIFT+13)
    RUBY_FL_USER14 = (1<<RUBY_FL_USHIFT+14)
    RUBY_FL_USER15 = (1<<RUBY_FL_USHIFT+15)
    RUBY_FL_USER16 = (1<<RUBY_FL_USHIFT+16)
    RUBY_FL_USER17 = (1<<RUBY_FL_USHIFT+17)
    RUBY_FL_USER18 = (1<<RUBY_FL_USHIFT+18)
    RUBY_FL_USER19 = (1<<RUBY_FL_USHIFT+19)
    RUBY_FL_SINGLETON = RUBY_FL_USER0

RARRAY_EMBED_FLAG = RUBY_FL_USER1
RARRAY_EMBED_LEN_SHIFT = RUBY_FL_USHIFT+3
RARRAY_EMBED_LEN_MASK = (RUBY_FL_USER4|RUBY_FL_USER3)

tLAST_OP_ID = gdb.parse_and_eval('tLAST_OP_ID')

try:
    ID_SCOPE_SHIFT = gdb.parse_and_eval('RUBY_ID_SCOPE_SHIFT')
except:
    ID_SCOPE_SHIFT = 4

RUBY_Qfalse = gdb.parse_and_eval('RUBY_Qfalse')
RUBY_Qtrue = gdb.parse_and_eval('RUBY_Qtrue')
RUBY_Qnil = gdb.parse_and_eval('RUBY_Qnil')
RUBY_Qundef = gdb.parse_and_eval('RUBY_Qundef')

RUBY_FIXNUM_FLAG = gdb.parse_and_eval('RUBY_FIXNUM_FLAG')
RUBY_SYMBOL_FLAG = gdb.parse_and_eval('RUBY_SYMBOL_FLAG')

RUBY_FLONUM_MASK = gdb.parse_and_eval('RUBY_FLONUM_MASK')
RUBY_FLONUM_FLAG = gdb.parse_and_eval('RUBY_FLONUM_FLAG')

RUBY_SPECIAL_SHIFT = gdb.parse_and_eval('RUBY_SPECIAL_SHIFT')
RUBY_IMMEDIATE_MASK = gdb.parse_and_eval('RUBY_IMMEDIATE_MASK')
RUBY_T_MASK = gdb.parse_and_eval('RUBY_T_MASK')
RUBY_T_NONE = gdb.parse_and_eval('RUBY_T_NONE')
RUBY_T_NIL = gdb.parse_and_eval('RUBY_T_NIL')
RUBY_T_OBJECT = gdb.parse_and_eval('RUBY_T_OBJECT')
RUBY_T_CLASS = gdb.parse_and_eval('RUBY_T_CLASS')
RUBY_T_ICLASS = gdb.parse_and_eval('RUBY_T_ICLASS')
RUBY_T_MODULE = gdb.parse_and_eval('RUBY_T_MODULE')
RUBY_T_FLOAT = gdb.parse_and_eval('RUBY_T_FLOAT')
RUBY_T_STRING = gdb.parse_and_eval('RUBY_T_STRING')
RUBY_T_REGEXP = gdb.parse_and_eval('RUBY_T_REGEXP')
RUBY_T_FIXNUM = gdb.parse_and_eval('RUBY_T_FIXNUM')
RUBY_T_ARRAY = gdb.parse_and_eval('RUBY_T_ARRAY')
RUBY_T_HASH = gdb.parse_and_eval('RUBY_T_HASH')
RUBY_T_STRUCT = gdb.parse_and_eval('RUBY_T_STRUCT')
RUBY_T_BIGNUM = gdb.parse_and_eval('RUBY_T_BIGNUM')
RUBY_T_RATIONAL = gdb.parse_and_eval('RUBY_T_RATIONAL')
RUBY_T_COMPLEX = gdb.parse_and_eval('RUBY_T_COMPLEX')
RUBY_T_FILE = gdb.parse_and_eval('RUBY_T_FILE')
RUBY_T_TRUE = gdb.parse_and_eval('RUBY_T_TRUE')
RUBY_T_FALSE = gdb.parse_and_eval('RUBY_T_FALSE')
RUBY_T_DATA = gdb.parse_and_eval('RUBY_T_MATCH')
RUBY_T_SYMBOL = gdb.parse_and_eval('RUBY_T_SYMBOL')
RUBY_T_UNDEF = gdb.parse_and_eval('RUBY_T_UNDEF')
RUBY_T_NODE = gdb.parse_and_eval('RUBY_T_NODE')
RUBY_T_ZOMBIE = gdb.parse_and_eval('RUBY_T_ZOMBIE')
RUBY_T_NIL = gdb.parse_and_eval('RUBY_T_NIL')

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
    a = ary.cast(RArray_t.pointer())
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
    a = ary.cast(RArray_t.pointer())
    if a['basic']['flags'] & RARRAY_EMBED_FLAG:
        return a['as']['ary']
    else:
        return a['as']['heap']['ptr']

def print_ruby_id(v):
    l = _rb_id2str(v)
    if l is not None:
        sys.stdout.write(get_rstring(l))

def print_ruby_class(v):
    print("Class value print not yet supported")

def print_ruby_array(v):
    len = _RARRAY_LEN(v)
    for i in range(0, int(len)):
        print_ruby_value(_rb_ary_entry(v, i))
        sys.stdout.write(" ")

def PACKED_BINS(table):
    return table['as']['packed']['entries']

def PACKED_ENT(table, i):
    return PACKED_BINS(table)[i]

def PKEY(table, i):
    return PACKED_ENT(table, (i))['key']

def PVAL(table, i):
    return PACKED_ENT(table, (i))['val']

def PHASH(table, i):
    return PACKED_ENT(table, (i))['hash']

# Walk through the ruby hashtable and callback the lambda for each key,value
# pair
def st_foreach(table, callback, *args):
    if table == 0:
        return

    if table['entries_packed']:
        for i in range(0, int(table['as']['packed']['real_entries'])):
            key = PKEY(table, i)
            val = PVAL(table, i)
            callback(key, val, *args)
    else:
        ptr = table['as']['big']['head']

        while ptr != 0 and table['as']['big']['head'] != 0:
            key = ptr['key']
            val = ptr['record']
            callback(key, val, *args)
            ptr = ptr['fore']

    return

def print_ruby_hash(v):
    h = v.cast(RHash_t.pointer())
    sys.stdout.write("{ ")
    if h['ntbl']:
        f = lambda key,val: [print_ruby_value(key), sys.stdout.write(" => "), print_ruby_value(val), sys.stdout.write(", ")]
        st_foreach(h['ntbl'], f)
    sys.stdout.write("}")

ROBJECT_EMBED_LEN_MAX = 3

def ROBJECT_NUMIV(o):
    if (o['basic']['flags'] & RUBY_FL_USER1):
        return ROBJECT_EMBED_LEN_MAX
    else:
        return o['as']['heap']['numiv']

def ROBJECT_IVPTR(o):
    if (o['basic']['flags'] & RUBY_FL_USER1):
        return o['as']['ary']
    else:
        return o['as']['heap']['ivptr']

def ROBJECT_IV_INDEX_TBL(o):
    if (o['basic']['flags'] & RUBY_FL_USER1):
        return RCLASS_IV_INDEX_TBL(rb_obj_class(o))
    else:
        return o['as']['heap']['iv_index_tbl']

def RCLASS_IV_INDEX_TBL(c):
    return (RCLASS_EXT(c)['iv_index_tbl'])

def RCLASS_EXT(c):
    return c['ptr']

def RCLASS_IV_TBL(c):
    return RCLASS_EXT(c)['iv_tbl']

def RCLASS_CONST_TBL(c):
    return RCLASS_EXT(c)['const_tbl']

def rb_obj_class(o):
    return rb_class_real(CLASS_OF(o))

def CLASS_OF(v):
    return rb_class_of(v)

def IMMEDIATE_P(x):
    return (x & RUBY_IMMEDIATE_MASK)

def rb_class_of(obj):
    return obj['basic']['klass']
#    if (IMMEDIATE_P(obj)):
#       if (FIXNUM_P(obj)) return rb_cFixnum;
#       if (FLONUM_P(obj)) return rb_cFloat;
#       if (obj == Qtrue)  return rb_cTrueClass;
#       if (STATIC_SYM_P(obj)) return rb_cSymbol;
#     }
#     else if (!RTEST(obj)) {
#       if (obj == Qnil)   return rb_cNilClass;
#       if (obj == Qfalse) return rb_cFalseClass;
#     }
#     return RBASIC(obj)->klass;
# }

def rb_class_real(cl):
    cl = cl.cast(RClass_t.pointer())
    while ((cl != 0) and
           ((cl['basic']['flags'] & RUBY_FL_SINGLETON) or (cl['basic']['flags'] & RUBY_T_MASK) == RUBY_T_ICLASS)):
        cl = cl['super']

    return cl

# Print a ruby object's variables
def print_ruby_object(v):
    obj = v.cast(RObject_t.pointer())
    len = ROBJECT_NUMIV(obj)
    ptr = ROBJECT_IVPTR(obj)
    num = 0;

    # Print instance variables.
    #
    # The instance variable values are stored inside the RObject itself but
    # the names are in class's rb_classext_struct.iv_index_tbl variable. So
    # we will iterate that hash table and print key from it and value from
    # this object's variable
    sys.stdout.write("{{ ")
    f = lambda key,val: [ print_ruby_id(key), sys.stdout.write(" => "),
                          print_ruby_value(ptr[val]), sys.stdout.write(", ")]
    st_foreach(ROBJECT_IV_INDEX_TBL(obj), f)

    # Print class variable (@@myvar).
    f = lambda key,val: [ print_ruby_id(key), sys.stdout.write(" => "),
                          print_ruby_value(val), sys.stdout.write(", ")]
    st_foreach(RCLASS_IV_TBL(rb_obj_class(obj)), f)

    # Print the constants in this object
    st_foreach(RCLASS_CONST_TBL(rb_obj_class(obj)), f)
    sys.stdout.write("}}")

class FLONUM(ctypes.Union):
    _fields_ = [("d", ctypes.c_double),
                ("v", ctypes.c_ulong)]

def print_ruby_value(v):
    if ((v & ~((~0)<<RUBY_SPECIAL_SHIFT)) == RUBY_SYMBOL_FLAG):
        print_ruby_id(v >> RUBY_SPECIAL_SHIFT)
        return

    if v & RUBY_FIXNUM_FLAG:
        sys.stdout.write("%ld" % (v >> 1))
        flags = v.cast(RBasic_t.pointer())['flags']
        return

    if v == RUBY_Qfalse:
        sys.stdout.write("false")
        return

    if v == RUBY_Qtrue:
        sys.stdout.write("true")
        return

    if v == RUBY_Qnil:
        sys.stdout.write("nil")
        return

    if v == RUBY_Qundef:
        sys.stdout.write("undef")
        return

    if (v & RUBY_IMMEDIATE_MASK):
        if (v & RUBY_FLONUM_MASK == RUBY_FLONUM_FLAG):
            if (v != 0x8000000000000002):
                b63 = (v >> 63)
                nv = (2 - b63) | (v & ~0x03)
                nv = ((nv >> 3) | (nv << (8 * 8) - 3))
                f = FLONUM(v=nv)
                sys.stdout.write("%s" % f.d)
        else:
            sys.stdout.write("immediate")
        return

    flags = v.cast(RBasic_t.pointer())['flags']

    if flags & RUBY_T_MASK == RUBY_T_NONE:
        sys.stdout.write("T_NONE : (struct RBasic *)%s" % v)
        return

    if flags & RUBY_T_MASK == RUBY_T_NIL:
        sys.stdout.write("T_NIL : (struct RBasic *)%s" % v)
        return

    if flags & RUBY_T_MASK == RUBY_T_OBJECT:
        #sys.stdout.write("T_OBJECT : (struct RObject *)%s TODO" % v)
        print_ruby_object(v)
        return

    if flags & RUBY_T_MASK == RUBY_T_CLASS:
        s = "*" if (flags & RUBY_FL_SINGLETON) else ""
        sys.stdout.write("T_CLASS %s : (struct RObject *)%s  TODO" % (s, v))
        return

    if flags & RUBY_T_MASK == RUBY_T_ICLASS:
        sys.stdout.write("T_ICLASS : TODO ")
        print_ruby_class(v)
        return

    if flags & RUBY_T_MASK == RUBY_T_MODULE:
        sys.stdout.write("T_MODULE : TODO")
        print_ruby_class(v)
        return

    if flags & RUBY_T_MASK == RUBY_T_FLOAT:
        v.cast(RFloat_t.pointer())['float_value']
        sys.stdout.write("T_FLOAT : %f (struct RFloat *)%s" % (v))
        return

    if flags & RUBY_T_MASK == RUBY_T_STRING:
        sys.stdout.write("\"" + get_rstring(v) + "\"")
        return

    if flags & RUBY_T_MASK == RUBY_T_REGEXP:
        sys.stdout.write("T_REGEX : (struct RRegex *)%s  TODO" % v)
        return

    if flags & RUBY_T_MASK == RUBY_T_ARRAY:
        print_ruby_array(v)
        return

    if flags & RUBY_T_MASK == RUBY_T_HASH:
        print_ruby_hash(v)
        return

    sys.stdout.write("unknown (struct RBasic *)%s" % v)
    # gdb.execute("rp %s" % v)

def try_print_variable(key, val):
    # TODO: fix it
    try:
        global_entry = val.cast(global_entry_t.pointer())
        v = (global_entry['var']['data']).cast(VALUE_t)
        print_ruby_id(key)
        sys.stdout.write(" = ")
        print_ruby_value(v)
    except:
        return
    finally:
        sys.stdout.write("\n")

def print_global_variables():
    rb_global_tbl = gdb.parse_and_eval('rb_global_tbl')
    st_foreach(rb_global_tbl, try_print_variable)

def get_ruby_localvariables(th=None, varname=None):
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
              call_stack.append(cfp)
      cfp -= 1

  for cfp in reversed(call_stack):
      print(get_func_info(cfp))
      local_table = get_local_table(cfp)
      local_table_size = get_local_table_size(cfp)
      print("local table size = %s" % local_table_size)
      for j in range(0, int(local_table_size)):
          id = local_table[j]
          l = _rb_id2str(id)
          if varname is not None:
              if l is not None or varname is get_rstring(l):
                  continue

          if l is not None:
              sys.stdout.write("%s = " % get_rstring(l))
              v = cfp['ep'] - (local_table_size - j - 1 + 2)
              try:
                  print_ruby_value(v.dereference())
              except:
                  continue
              finally:
                  sys.stdout.write("\n")

def get_rstring(addr):
  if addr is None:
    return None

  s = addr.cast(RString_t.pointer())
  if s['basic']['flags'] & (1 << 13):
    return s['as']['heap']['ptr'].string()
  else:
    return s['as']['ary'].string()

def get_lineno(iseq, pos):
  if pos != 0:
    pos -= 1

  try:
      t = iseq['body']['line_info_table']
      t_size = iseq['body']['line_info_size']
  except:
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

def get_func_info(cfp):
    try:
        path = get_rstring(cfp['iseq']['location']['path'])
        label = get_rstring(cfp['iseq']['location']['label'])
        lineno = get_lineno(cfp['iseq'], cfp['pc'] - cfp['iseq']['iseq_encoded'])
    except:
        # Ruby 2.3+
        path = get_rstring(cfp['iseq']['body']['location']['path'])
        label = get_rstring(cfp['iseq']['body']['location']['label'])
        lineno = get_lineno(cfp['iseq'], cfp['pc'] - cfp['iseq']['body']['iseq_encoded'])

    return "{}:{}:in `{}'".format(path, lineno, label)

def get_local_table(cfp):
    try:
        local_table = (cfp['iseq']['local_table'])
    except:
        local_table = (cfp['iseq']['body']['local_table'])
    return local_table

def get_local_table_size(cfp):
    try:
        local_table_size = (cfp['iseq']['local_table_size'])
    except:
        local_table_size = (cfp['iseq']['body']['local_table_size'])
    return local_table_size

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
                call_stack.append(get_func_info(cfp))
        cfp -= 1

    for i in reversed(call_stack):
        if folded:
            sys.stdout.write("%s;" % i)
        else:
            print(i)

    if folded:
        print()

def get_rb_thread_for_current_thread():
    # We will get the current thread id from gdb, iterate over all ruby
    # threads and find the corresponding rb_thread_t

    # TODO: Can't we directly pull the thread id out of gdb.selected_thread()?
    t = gdb.selected_thread()
    if t is None:
        print("No current thread")
        return None

    gdb_th_num = t.num
    curr_th_id = re.search('Thread ([0-9a-zx]+)', gdb.execute('info thread %d' % gdb_th_num, to_string=True)).group(1);
    curr_th_id = int(curr_th_id, 16)
    #curr_th_id=gdb.parse_and_eval('(pthread_t)pthread_self()')

    head = gdb.parse_and_eval('&ruby_current_thread->vm->living_threads.n')
    t = gdb.parse_and_eval('ruby_current_thread->vm->living_threads.n.next')
    while (t != head):
        ti = t.cast(rb_thread_struct_t.pointer())
        if ti['thread_id'] == curr_th_id:
            #print("found matching ruby thread")
            return ti
        t = t['next']

    print("Thread {0} (0x{1:x}) is not a ruby thread!".format(gdb_th_num,curr_th_id))

    return None

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
    "Print Ruby callstack of current GDB thread (which may/may not be current Ruby thread holding GVL)"

    def __init__(self):
        gdb.Command.__init__(self, "ruby_bt", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        ti = get_rb_thread_for_current_thread()
        if ti is None:
            print("Could not find ruby thread info for current gdb thread")
            return

        argv = gdb.string_to_argv(arg)
        folded = False
        if len(argv) > 0 and argv[0] == 'folded':
            folded = True
        get_ruby_stacktrace(th=ti, folded=folded)

class RubyStackTraceCurrCmd(gdb.Command):
    "Print stacktrace of current Ruby thread (which may/may not be current GDB thread)"

    def __init__(self):
        gdb.Command.__init__(self, "ruby_bt_curr", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        get_ruby_stacktrace()

class RubyLocalVariablesCmd(gdb.Command):
    "Print Ruby local variables, by walking current GDB thread's Ruby stack. Pass variable name to filter"

    def __init__(self):
        gdb.Command.__init__(self, "ruby_locals", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        ti = get_rb_thread_for_current_thread()
        if ti is None:
            print("Could not find ruby thread info for current gdb thread")
            return

        argv = gdb.string_to_argv(arg)
        varname = None
        if len(argv) > 0:
            varname = argv[0]

        get_ruby_localvariables(th=ti, varname=varname)

class RubyLocalVariablesCurrCmd(gdb.Command):
    "Print Ruby local variables, by walking current (holding GVL) Ruby thread's stack. Pass variable name to filter"

    def __init__(self):
        gdb.Command.__init__(self, "ruby_locals_curr", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        argv = gdb.string_to_argv(arg)
        varname = None
        if len(argv) > 0:
            varname = argv[0]

        get_ruby_localvariables(varname=varname)

class RubyGlovalVariablesCmd(gdb.Command):
    "Print Ruby global variables"

    def __init__(self):
        gdb.Command.__init__(self, "ruby_globals", gdb.COMMAND_DATA, gdb.COMPLETE_SYMBOL)

    def invoke(self, arg, _from_tty):
        print_global_variables()

AnalyzeSegFaultCmd()
RubyStackTraceCmd()
RubyStackTraceCurrCmd()
RubyLocalVariablesCmd()
RubyLocalVariablesCurrCmd()
RubyGlovalVariablesCmd()

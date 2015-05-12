/*
 * Helper library for computing bytes:flops ratios
 * (core functions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "byfl.h"
#include "byfl-common.h"
#include "callstack.h"
#ifdef HAVE_BACKTRACE
# include <execinfo.h>
#endif
#ifdef USE_BFD
# include "findsrc.h"
#endif

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

const unsigned int HDR_COL_WIDTH = 20;

namespace bytesflops {
// Keep track of basic-block counters on a per-function basis, being
// careful to work around the "C++ static initialization order fiasco"
// (cf. the C++ FAQ).
key2bfc_t& per_func_totals (void)
{
  static key2bfc_t* mapping = new key2bfc_t();
  return *mapping;
}
typedef CachedUnorderedMap<KeyType_t, uint64_t> key2num_t;
static key2num_t& func_call_tallies (void)
{
  static key2num_t* mapping = new key2num_t();
  return *mapping;
}

typedef CachedUnorderedMap<KeyType_t, std::string> key2name_t;
static key2name_t& key_to_func (void)
{
    static key2name_t* mapping = new key2name_t();
    return *mapping;
}

typedef std::map<std::string, uint64_t> str2num_t;
static str2num_t& final_call_tallies (void)
{
  static str2num_t* mapping = new str2num_t();
  return *mapping;
}

static uint32_t& bf_cnt (void)
{
    static uint32_t* cnt = new uint32_t(0);
    return *cnt;
}

// Keep track of basic-block counters on a user-defined basis, being
// careful to work around the "C++ static initialization order fiasco"
// (cf. the C++ FAQ).
str2bfc_t& user_defined_totals (void)
{
  static str2bfc_t* mapping = new str2bfc_t();
  return *mapping;
}

} // namespace bytesflops



// Associate a function name (which will not be unique across files)
// with a unique key.  Abort if duplicate keys are detected.  (This
// should be exceedingly unlikely.)
static void bf_record_key (const char* funcname, KeyType_t keyID)
{
  auto & map = key_to_func();
  auto iter = map.find(keyID);
  if (iter != map.end() && iter->second != funcname) {
    std::cerr << "Fatal Error: duplicate keys found for " << funcname << std::endl;
    bf_abend();
  }
  map[keyID] = std::move(std::string(funcname));
}

// bf_categorize_counters() is intended to be overridden by a
// user-defined function.
extern "C" {
  const char* bf_categorize_counters_original (void)
  {
    return NULL;
  }
  const char* bf_categorize_counters (void) __attribute__((weak, alias("bf_categorize_counters_original")));
}

KeyType_t bf_categorize_counters_id = 10; // Should be unlikely that this is a duplicate.
extern char** environ;

// Define a mapping from an instruction's opcode to its arguments' opcodes to a
// tally.  We ignore instructions with more than two arguments.
uint64_t bf_inst_deps_histo[NUM_LLVM_OPCODES_POW2][NUM_LLVM_OPCODES_POW2][NUM_LLVM_OPCODES_POW2][2] = {0};

namespace bytesflops {

const char* bf_func_and_parents; // Top of the complete_call_stack stack
KeyType_t bf_func_and_parents_id; // Top of the complete_call_stack stack
KeyType_t bf_current_func_key;
string bf_output_prefix;         // String to output before "BYFL" on every line
ostream* bfout;                  // Stream to which to send textual output
BinaryOStream* bfbin;            // Stream to which to send binary output
ofstream *bfbin_file;            // Underlying file for the above
bool bf_abnormal_exit = false;   // false=exit normally; true=get out fast
#ifdef USE_BFD
ProcessSymbolTable* procsymtab = NULL;  // The calling process's symbol table
#endif
static CallStack* call_stack = NULL;    // The calling process's current call stack

// As a kludge, set a global variable indicating that all of the
// constructors in this file have been called.  Because of the "C++
// static initialization order fiasco" (cf. the C++ FAQ) we otherwise
// have no guarantee that, in particular, cout (from iostream) has
// been initialized.  If cout hasn't been called, we can force
// suppress_output() to return true until it is.
static bool all_constructors_called = false;
static class CheckConstruction {
public:
  CheckConstruction() {
    all_constructors_called = true;
  }
} check_construction;

// Initialize some of our variables at first use.
void initialize_byfl (void) {
  bf_func_and_parents = "-";
  bf_func_and_parents_id = KeyType_t(0);
  bf_current_func_key = KeyType_t(0);
  call_stack = new CallStack();
#ifdef USE_BFD
  procsymtab = new ProcessSymbolTable();
#endif
  const char* partition = bf_categorize_counters();
  if (partition != NULL)
    bf_record_key(partition, bf_categorize_counters_id);
}

// Initialize on first use all top-level variables in all files.  This
// is a kludge to work around the "C++ static initialization order
// fiasco" (cf. the C++ FAQ).  bf_initialize_if_necessary() can safely
// be called multiple times.
extern "C"
void bf_initialize_if_necessary (void)
{
  static bool initialized = false;
  if (!__builtin_expect(initialized, true)) {
    initialize_byfl();
    initialize_bblocks();
    initialize_reuse();
    initialize_symtable();
    initialize_threading();
    initialize_ubytes();
    initialize_tallybytes();
    initialize_vectors();
    initialize_data_structures();
    initialize_cache();
    initialized = true;
  }
}

// Exit the program abnormally.
void bf_abend (void)
{
  bf_abnormal_exit = true;
  std::exit(1);
}

// Tally the number of calls to each function.
extern "C"
void bf_incr_func_tally (KeyType_t keyID)
{
  func_call_tallies()[keyID]++;
}

extern "C"
void bf_record_funcs2keys(uint32_t cnt, const uint64_t* keys,
                          const char** fnames)
{
  for (unsigned int i = 0; i < cnt; i++)
    bf_record_key(fnames[i], keys[i]);
}

// Push a function name onto the call stack.  Increment the invocation
// count of the call stack as a whole, and ensure the individual
// function name also exists in the hash table.
extern "C"
void bf_push_function (const char* funcname, KeyType_t key)
{
  bf_current_func_key = key;
  bf_func_and_parents =  call_stack->push_function(funcname, key);
  uint64_t depth = 1 << call_stack->depth();
  bf_func_and_parents_id = bf_func_and_parents_id ^ depth ^ key;
  bf_record_key(bf_func_and_parents, bf_func_and_parents_id);
  func_call_tallies()[bf_func_and_parents_id]++;
  func_call_tallies()[key] += 0;
}

// Pop the top function name from the call stack.
extern "C"
void bf_pop_function (void)
{
  uint64_t depth = 1 << call_stack->depth();
  CallStack::StackItem_t item = call_stack->pop_function();
  bf_func_and_parents = item.first;
  bf_func_and_parents_id = bf_func_and_parents_id ^ depth ^ bf_current_func_key;
  bf_current_func_key = item.second;
}

// Expand a string like a POSIX shell would do.
static string shell_expansion(const char *str, const char *strname)
{
  string result;
  wordexp_t expansion;
  if (wordexp(str, &expansion, 0)) {
    cerr << "Failed to expand " << strname << "(\"" << str << "\")\n";
    bf_abend();
  }
  for (size_t i = 0; i < expansion.we_wordc; i++) {
    if (i > 0)
      result += string(" ");
    result += string(expansion.we_wordv[i]);
  }
  wordfree(&expansion);
  return result;
}

// Determine if we should suppress output from this process.
bool suppress_output (void)
{
  if (!all_constructors_called)
    // If cout hasn't been constructed, force all output to be suppressed.
    return true;
  static enum {UNKNOWN, SUPPRESS, SHOW} output = UNKNOWN;
  if (output == UNKNOWN) {
    // First invocation -- we can begin outputting.
    bfout = &cout;
    output = SHOW;

    // If the BF_BINOUT environment variable is set, expand it, treat it
    // as a filename, create the file, and write a magic pattern to it.
    char *binout_raw = getenv("BF_BINOUT");
    string binout;
    if (binout_raw)
      // BF_BINOUT was specified.  Use it as is.
      binout = binout_raw;
    else {
      // BF_BINOUT was not specified.  Append ".byfl" to the name of
      // the executable.
      binout = "a.out";
      vector<string> cmdline = parse_command_line();
      if (cmdline.size() > 0 &&
          cmdline[0].length() > 0 &&
          cmdline[0].compare(0, 7, "[failed") != 0)
        binout = cmdline[0];
      binout += ".byfl";
    }
    string bfbin_filename = shell_expansion(binout.c_str(), "BF_BINOUT");
    if (bfbin_filename == "")
      // Empty string (as opposed to unspecified environment
      // variable): discard all binary data.
      bfbin = new BinaryOStream();
    else {
      // Non-empty string: write to the named file.
      bfbin_file = new ofstream(bfbin_filename, ios_base::out | ios_base::trunc | ios_base::binary);
      if (bfbin_file->fail()) {
        cerr << "Failed to create output file " << bfbin_filename << '\n';
        bf_abend();
      }
      bfbin = new BinaryOStreamReal(*bfbin_file);
      *bfbin << uint8_t('B') << uint8_t('Y') << uint8_t('F') << uint8_t('L')
             << uint8_t('B') << uint8_t('I') << uint8_t('N');
    }

    // If the BF_PREFIX environment variable is set, expand it and
    // output it before each line of output.
    char *prefix = getenv("BF_PREFIX");
    if (prefix) {
      // Perform shell expansion on BF_PREFIX.
      bf_output_prefix = shell_expansion(prefix, "BF_PREFIX");

      // If the prefix starts with "/" or "./", treat it as a filename
      // and write all textual output there.
      if ((bf_output_prefix.size() >= 1 && bf_output_prefix[0] == '/')
          || (bf_output_prefix.size() >= 2 && bf_output_prefix[0] == '.' && bf_output_prefix[1] == '/')) {
        bfout = new ofstream(bf_output_prefix, ios_base::out | ios_base::trunc);
        if (bfout->fail()) {
          cerr << "Failed to create output file " << bf_output_prefix << '\n';
          bf_abend();
        }
        bf_output_prefix = "";
      }
    }

    // Log the Byfl command line to help users reproduce their results.
    *bfout << "BYFL_INFO: Byfl command line: " << bf_option_string << '\n';

    // Number warning messages.
    int num_warnings = 0;

    // Warn the user if he defined bf_categorize_counters() but didn't
    // compile with -bf-every-bb.
    if (bf_categorize_counters != bf_categorize_counters_original && !bf_every_bb)
      *bfout << "BYFL_WARNING: (" << ++num_warnings << ") bf_categorize_counters() has no effect without -bf-every-bb;\n"
             << "BYFL_WARNING:     consider using -bf-every-bb -bf-merge-bb="
             << uint64_t(-1) << ".\n";

    // Warn the user if he specified -bf-data-structs but lacks the
    // backtrace() function and/or the BFD library.
    if (bf_data_structs) {
#ifndef USE_BFD
      *bfout << "BYFL_WARNING: (" << ++num_warnings << ") Byfl failed to find the BFD development files at\n"
             << "BYFL_WARNING:     configuration time.  -bf-data-structs will therefore be\n"
             << "BYFL_WARNING:     unable to identify statically allocated data structures or\n"
             << "BYFL_WARNING:     to report source-code locations for dynamically allocated\n"
             << "BYFL_WARNING:     data structures.\n";
#endif
#ifndef HAVE_BACKTRACE
      *bfout << "BYFL_WARNING: (" << ++num_warnings << ") Byfl failed to find a backtrace_symbols() function at\n"
             << "BYFL_WARNING:     configuration time.  -bf-data-structs will therefore be\n"
             << "BYFL_WARNING:     unable to identify dynamically allocated data structures.\n";
#endif
    }
  }
  return output == SUPPRESS;
}

#ifdef HAVE_BACKTRACE
// Return the address of the user code that invoked us.
void* bf_find_caller_address (void)
{
  const size_t DEPTH_UNINITIALIZED = (size_t)(-1);   // We haven't yet calibrarated our stack depth
  const size_t DEPTH_UNKNOWN = (size_t)(-2);         // We were unable to determine our stack depth
  static size_t discard_num = DEPTH_UNINITIALIZED;   // Number of stack entries to discard
  const size_t max_stack_depth = 32;   // Maximum stack depth before we expect to find a user function
  void* stack_addrs[max_stack_depth];

  switch (discard_num) {
    case DEPTH_UNKNOWN:
      // If we gave up once, give up forever after.
      return NULL;
      break;

    case DEPTH_UNINITIALIZED:
      // On our first invocation, determine the call depth of the
      // Byfl library.
      {
        size_t stack_depth = backtrace(stack_addrs, max_stack_depth);
        char** symnames = backtrace_symbols(stack_addrs, stack_depth);
        discard_num = DEPTH_UNKNOWN;   // Assume we don't find anything.
        for (size_t i = 0; i < stack_depth; i++) {
          char* name = symnames[i];
          if (!strstr(name, "bf_") &&
              !strstr(name, "bytesflops") &&
              !strstr(name, "libbyfl")) {
            discard_num = i;
            break;
          }
        }
        free(symnames);
        if (discard_num == DEPTH_UNKNOWN)
          return NULL;
      }
      // No break

    default:
      // On the first and all subsequent invocations, return the
      // first non-Byfl stack address.
      {
        size_t stack_depth = backtrace(stack_addrs, discard_num + 1);
        return stack_addrs[stack_depth - 1];
      }
      break;
  }
}

// Return a string version of an instruction address (e.g., "foo.c:123").
const char* bf_address_to_location_string (void* addrp)
{
  typedef map<uintptr_t, const char*> addr2string_t;
  static addr2string_t address_map;
  uintptr_t addr = uintptr_t(addrp);
  addr2string_t::iterator addr_iter = address_map.find(addr);
  if (addr_iter == address_map.end()) {
    // This is the first time we've seen this address.
    char** locstrs = backtrace_symbols(&addrp, 1);
    const char* location_string = locstrs == NULL ? "??:0" : strdup(locstrs[0]);
    free(locstrs);
    address_map[addr] = location_string;
    return location_string;
  }
  else
    // Return the string we found the previous time.
    return addr_iter->second;
}
#endif

// At the end of the program, report what we measured.
static class RunAtEndOfProgram {
private:
  string separator;    // Horizontal rule to output between sections

  static void aggregate_call_tallies() {
    key2num_t & fmap = func_call_tallies();
    str2num_t & final_map = final_call_tallies();
    for (auto it = fmap.begin(); it != fmap.end(); it++) {
      auto kiter = key_to_func().find(it->first);
      if (kiter == key_to_func().end())
        std::cerr << "ERROR: key " << it->first
                  << " was not recorded." << std::endl;
      else {
        std::string & func = key_to_func()[it->first];
        final_map[func.c_str()] += it->second;
      }
    }
  }

  // Compare two strings.
  static bool compare_char_stars (const char* one, const char* two) {
    return strcmp(one, two) < 0;
  }

  static bool compare_keys_to_names (KeyType_t one, KeyType_t two) {
    auto& map = key_to_func();
    return map[one] < map[two];
  }

  // Compare two function names, reporting which was called more
  // times.  Break ties by comparing function names.
  static bool compare_func_totals (const std::string& one,
                                   const std::string& two) {
    uint64_t one_calls = final_call_tallies()[one];
    uint64_t two_calls = final_call_tallies()[two];
    if (one_calls != two_calls)
      return one_calls > two_calls;
    else
      return one < two;
  }

  // Compare two {name, tally} pairs, reporting which has the greater
  // tally.  Break ties by comparing names.
  typedef pair<const char*, uint64_t> name_tally;
  static bool compare_name_tallies (const name_tally& one, const name_tally& two) {
    if (one.second != two.second)
      return one.second > two.second;
    else
      return strcmp(one.first, two.first);
  }

  // Report per-function counter totals.
  void report_by_function (void) {
    // Aggregate and sort our function-call data.
    aggregate_call_tallies();
    vector<KeyType_t>* all_funcs = per_func_totals().sorted_keys(compare_keys_to_names);

    // Output a textual header line.
    *bfout << bf_output_prefix
           << "BYFL_FUNC_HEADER: "
           << setw(HDR_COL_WIDTH) << "LD_bytes" << ' '
           << setw(HDR_COL_WIDTH) << "ST_bytes" << ' '
           << setw(HDR_COL_WIDTH) << "LD_ops" << ' '
           << setw(HDR_COL_WIDTH) << "ST_ops" << ' '
           << setw(HDR_COL_WIDTH) << "Flops" << ' '
           << setw(HDR_COL_WIDTH) << "FP_bits" << ' '
           << setw(HDR_COL_WIDTH) << "Int_ops" << ' '
           << setw(HDR_COL_WIDTH) << "Int_op_bits";
    if (bf_unique_bytes)
      *bfout << ' '
             << setw(HDR_COL_WIDTH) << "Uniq_bytes";
    *bfout << ' '
           << setw(HDR_COL_WIDTH) << "Cond_brs" << ' '
           << setw(HDR_COL_WIDTH) << "Invocations" << ' '
           << "Function";
    if (bf_call_stack)
      for (size_t i=0; i<call_stack->max_depth-1; i++)
        *bfout << ' '
               << "Parent_func_" << i+1;
    *bfout << '\n';

    // Output a binary table header.
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Functions";
    *bfbin << uint8_t(BINOUT_COL_UINT64) << "Load operations"
           << uint8_t(BINOUT_COL_UINT64) << "Store operations"
           << uint8_t(BINOUT_COL_UINT64) << "Floating-point operations"
           << uint8_t(BINOUT_COL_UINT64) << "Integer operations"
           << uint8_t(BINOUT_COL_UINT64) << "Function-call operations (non-exception-throwing)"
           << uint8_t(BINOUT_COL_UINT64) << "Function-call operations (exception-throwing)"
           << uint8_t(BINOUT_COL_UINT64) << "Unconditional and direct branch operations (removable)"
           << uint8_t(BINOUT_COL_UINT64) << "Unconditional and direct branch operations (mandatory)"
           << uint8_t(BINOUT_COL_UINT64) << "Conditional branch operations (not taken)"
           << uint8_t(BINOUT_COL_UINT64) << "Conditional branch operations (taken)"
           << uint8_t(BINOUT_COL_UINT64) << "Unconditional but indirect branch operations"
           << uint8_t(BINOUT_COL_UINT64) << "Multi-target (switch) branch operations"
           << uint8_t(BINOUT_COL_UINT64) << "Function-return operations"
           << uint8_t(BINOUT_COL_UINT64) << "Other branch operations"
           << uint8_t(BINOUT_COL_UINT64) << "Floating-point operation bits"
           << uint8_t(BINOUT_COL_UINT64) << "Integer operation bits"
           << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded"
           << uint8_t(BINOUT_COL_UINT64) << "Bytes stored"
           << uint8_t(BINOUT_COL_UINT64) << "Calls to memset"
           << uint8_t(BINOUT_COL_UINT64) << "Bytes stored by memset"
           << uint8_t(BINOUT_COL_UINT64) << "Calls to memcpy and memmove"
           << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded and stored by memcpy and memmove";
    if (bf_unique_bytes)
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Unique bytes";
    *bfbin << uint8_t(BINOUT_COL_UINT64) << "Invocations";
    if (bf_call_stack)
      *bfbin << uint8_t(BINOUT_COL_STRING) << "Mangled call stack"
             << uint8_t(BINOUT_COL_STRING) << "Demangled call stack";
    else
      *bfbin << uint8_t(BINOUT_COL_STRING) << "Mangled function name"
             << uint8_t(BINOUT_COL_STRING) << "Demangled function name";
    *bfbin << uint8_t(BINOUT_COL_NONE);

    // Output the data by sorted function name in both textual and
    // binary formats.
    for (vector<KeyType_t>::iterator fn_iter = all_funcs->begin();
         fn_iter != all_funcs->end();
         fn_iter++) {
      // Gather information for the current function or call stack.
      const string funcname = key_to_func()[*fn_iter];
      const char* funcname_c = bf_string_to_symbol(funcname.c_str());
      ByteFlopCounters* func_counters = per_func_totals()[*fn_iter];
      uint64_t invocations = final_call_tallies()[funcname_c];
      uint64_t num_uniq_bytes;
      if (bf_unique_bytes)
        num_uniq_bytes = bf_mem_footprint ? bf_tally_unique_addresses_tb(funcname_c) : bf_tally_unique_addresses(funcname_c);

      // Group branches into various categories.
      uint64_t other_branches = func_counters->terminators[BF_END_BB_ANY];
      for (int i = 0; i < BF_END_BB_NUM; i++)
        if (i != BF_END_BB_ANY)
          other_branches -= func_counters->terminators[i];
      uint64_t unpred_branches =
        func_counters->terminators[BF_END_BB_COND_NT] +
        func_counters->terminators[BF_END_BB_COND_T] +
        func_counters->terminators[BF_END_BB_INDIRECT] +
        func_counters->terminators[BF_END_BB_SWITCH];

      // Output textual function data.
      *bfout << bf_output_prefix
             << "BYFL_FUNC:        "
             << setw(HDR_COL_WIDTH) << func_counters->loads << ' '
             << setw(HDR_COL_WIDTH) << func_counters->stores << ' '
             << setw(HDR_COL_WIDTH) << func_counters->load_ins << ' '
             << setw(HDR_COL_WIDTH) << func_counters->store_ins << ' '
             << setw(HDR_COL_WIDTH) << func_counters->flops << ' '
             << setw(HDR_COL_WIDTH) << func_counters->fp_bits << ' '
             << setw(HDR_COL_WIDTH) << func_counters->ops - func_counters->flops - func_counters->load_ins - func_counters->store_ins - func_counters->terminators[BF_END_BB_ANY] << ' '
             << setw(HDR_COL_WIDTH) << func_counters->op_bits;
      if (bf_unique_bytes)
        *bfout << ' ' << setw(HDR_COL_WIDTH) << num_uniq_bytes;
      *bfout << ' '
             << setw(HDR_COL_WIDTH) << unpred_branches << ' '
             << setw(HDR_COL_WIDTH) << invocations << ' '
             << funcname_c << '\n';

      // Output binary function data.
      *bfbin << uint8_t(BINOUT_ROW_DATA)
             << func_counters->load_ins
             << func_counters->store_ins
             << func_counters->flops
             << func_counters->ops - func_counters->flops - func_counters->load_ins - func_counters->store_ins - func_counters->terminators[BF_END_BB_ANY]
             << func_counters->call_ins
             << func_counters->terminators[BF_END_BB_INVOKE]
             << func_counters->terminators[BF_END_BB_UNCOND_FAKE]
             << func_counters->terminators[BF_END_BB_UNCOND_REAL]
             << func_counters->terminators[BF_END_BB_COND_NT]
             << func_counters->terminators[BF_END_BB_COND_T]
             << func_counters->terminators[BF_END_BB_INDIRECT]
             << func_counters->terminators[BF_END_BB_SWITCH]
             << func_counters->terminators[BF_END_BB_RETURN]
             << other_branches
             << func_counters->fp_bits
             << func_counters->op_bits
             << func_counters->loads
             << func_counters->stores
             << func_counters->mem_insts[BF_MEMSET_CALLS]
             << func_counters->mem_insts[BF_MEMSET_BYTES]
             << func_counters->mem_insts[BF_MEMXFER_CALLS]
             << func_counters->mem_insts[BF_MEMXFER_BYTES];
      if (bf_unique_bytes)
        *bfbin << num_uniq_bytes;
      *bfbin << invocations
             << funcname_c
             << demangle_func_name(funcname_c);
    }
    *bfbin << uint8_t(BINOUT_ROW_NONE);
    delete all_funcs;

    // Output, both textually and in binary, invocation tallies for
    // all called functions, not just instrumented functions.
    *bfout << bf_output_prefix
           << "BYFL_CALLEE_HEADER: "
           << setw(13) << "Invocations" << ' '
           << "Byfl" << ' '
           << "Function\n";
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Called functions";
    *bfbin << uint8_t(BINOUT_COL_UINT64) << "Invocations"
           << uint8_t(BINOUT_COL_BOOL) << "Byfl instrumented"
           << uint8_t(BINOUT_COL_BOOL) << "Exception throwing"
           << uint8_t(BINOUT_COL_STRING) << "Mangled function name"
           << uint8_t(BINOUT_COL_STRING) << "Demangled function name"
           << uint8_t(BINOUT_COL_NONE);
    vector<const char*> all_called_funcs;
    for (str2num_t::iterator sm_iter = final_call_tallies().begin();
         sm_iter != final_call_tallies().end();
         sm_iter++)
      all_called_funcs.push_back(sm_iter->first.c_str());
    sort(all_called_funcs.begin(), all_called_funcs.end(), compare_func_totals);
    for (vector<const char*>::iterator fn_iter = all_called_funcs.begin();
         fn_iter != all_called_funcs.end();
         fn_iter++) {
      const char* funcname = *fn_iter;   // Function name
      uint64_t tally = 0;                // Invocation count
      bool instrumented = true;          // Whether function was instrumented
      bool exception_throwing = false;   // Whether function can throw an exception
      if (funcname[0] == '+' || funcname[0] == '-') {
        exception_throwing = (funcname[0] == '-');
        const char* unique_name = bf_string_to_symbol(funcname+1);
        str2num_t::iterator tally_iter = final_call_tallies().find(unique_name);
        instrumented = (tally_iter != final_call_tallies().end());
        tally = final_call_tallies()[bf_string_to_symbol(funcname)];
        funcname = unique_name;
      }
      string funcname_demangled = demangle_func_name(funcname);
      if (tally > 0) {
        *bfout << bf_output_prefix
               << "BYFL_CALLEE: "
               << setw(HDR_COL_WIDTH) << tally << ' '
               << (instrumented ? "Yes " : "No  ") << ' '
               << funcname_demangled;
        if (funcname_demangled != funcname)
          *bfout << " [" << funcname << ']';
        *bfbin << uint8_t(BINOUT_ROW_DATA)
               << tally << instrumented << exception_throwing
               << funcname << funcname_demangled;
        *bfout << '\n';
      }
    }
    *bfbin << uint8_t(BINOUT_ROW_NONE);
  }

  // Report the number of times each instruction type was executed.  Return the
  // total number of instructions executed.
  uint64_t report_instruction_mix (const char* partition, ByteFlopCounters& counter_totals, string& tag) {
    // Sort the histogram by decreasing opcode tally.
    uint64_t total_insts = 0;
    vector<name_tally> sorted_inst_mix;
    size_t maxopnamelen = 0;
    for (uint64_t i = 0; i < NUM_LLVM_OPCODES; i++)
      if (counter_totals.inst_mix_histo[i] != 0) {
        sorted_inst_mix.push_back(name_tally(opcode2name[i],
                                             counter_totals.inst_mix_histo[i]));
        size_t opnamelen = strlen(opcode2name[i]);
        if (opnamelen > maxopnamelen)
          maxopnamelen = opnamelen;
      }
    sort(sorted_inst_mix.begin(), sorted_inst_mix.end(), compare_name_tallies);

    // Output the sorted results.
    string inst_mix_table_name("Instruction mix");
    if (partition)
      inst_mix_table_name += string(" for tag ") + string(partition);
    *bfbin << uint8_t(BINOUT_TABLE_KEYVAL) << inst_mix_table_name;
    for (auto ntiter = sorted_inst_mix.cbegin();
         ntiter != sorted_inst_mix.cend();
         ntiter++) {
      total_insts += ntiter->second;
      *bfout << tag << ": " << setw(25) << ntiter->second << ' '
             << setw(maxopnamelen) << left
             << ntiter->first << " instructions executed\n"
             << right;
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << ntiter->first << ntiter->second;
    }
    *bfout << tag << ": " << setw(25) << total_insts << ' '
           << setw(maxopnamelen) << left
           << "TOTAL" << " instructions executed\n"
           << right
           << tag << ": " << separator << '\n';
    *bfbin << uint8_t(BINOUT_COL_NONE);
    return total_insts;
  }

  // Report the number of times each {instruction, operand1, operand2} triple
  // type was executed.
  void report_instruction_deps (string& tag) {
    // Convert the matrix to a histogram and sort it in decreasing order of
    // tally.
    struct InstInfo {
      int opcodes[3];     // Opcodes for the instruction and its first two arguments
      bool more;          // true=more than two arguments; false=two or fewer
      string name;        // Pretty-printed version of opcodes[]
      uint64_t tally;     // Number of dynamic executions observed
      InstInfo(int op, int arg1, int arg2, bool more_args, uint64_t n) {
        opcodes[0] = op;
        opcodes[1] = arg1;
        opcodes[2] = arg2;
        more = more_args;
        tally = n;
      }
      bool operator<(const InstInfo& other) const {
        if (tally != other.tally)
          return tally > other.tally;
        for (int i = 0; i < 3; i++)
          if (opcodes[i] != other.opcodes[i])
            return opcodes[i] < other.opcodes[i];
        if (more != other.more)
          return more < other.more;
        return false;
      }
    };
    vector<InstInfo> deps_histo;   // Histogram of instruction-dependency tallies
    for (int i = 0; i < NUM_LLVM_OPCODES+2; i++)
      for (int j = 0; j < NUM_LLVM_OPCODES+2; j++)
        for (int k = 0; k < NUM_LLVM_OPCODES+2; k++)
          for (int l = 0; l < 2; l++)
            if (bf_inst_deps_histo[i][j][k][l] > 0)
              deps_histo.push_back(InstInfo(i, j, k, bool(l), bf_inst_deps_histo[i][j][k][l]));
    if (deps_histo.size() == 0)
      return;   // No work to do
    sort(deps_histo.begin(), deps_histo.end());

    // Store a pretty-printed version of each dependency.  As side effects,
    // keep track of the maximum length of those and the total tally.
    size_t max_str_len = 0;
    uint64_t total_deps = 0;
    for (auto iter = deps_histo.begin(); iter != deps_histo.end(); iter++) {
      InstInfo& info = *iter;
      stringstream depstr;
      depstr << opcode2name[info.opcodes[0]] << '(';
      if (info.opcodes[1] != BF_NO_ARG) {
        depstr << opcode2name[info.opcodes[1]];
        if (info.opcodes[2] != BF_NO_ARG) {
          depstr << ", " << opcode2name[info.opcodes[2]];
          if (info.more)
            depstr << ", ...";
        }
      }
      depstr << ')';
      info.name = depstr.str();
      if (info.name.size() > max_str_len)
        max_str_len = info.name.size();
      total_deps += info.tally;
    }

    // Report in textual format the top ten instruction+arguments triples.
    int nOut = 0;
    uint64_t partial_total_deps = 0;
    for (auto iter = deps_histo.begin(); iter != deps_histo.end() && nOut < 10; iter++, nOut++) {
      InstInfo& info = *iter;
      *bfout << tag << ": "
             << setw(25) << info.tally << ' '
             << setw(max_str_len) << left << info.name << right
             << " dependencies executed\n";
      partial_total_deps += info.tally;
    }
    *bfout << tag << ": "
           << setw(25) << total_deps - partial_total_deps << ' '
           << setw(max_str_len) << ""
           << " additional dependencies executed\n";
    *bfout << tag << ": " << separator << '\n';

    // Report in binary format all instruction+arguments triples.
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Instruction dependencies";
    *bfbin << uint8_t(BINOUT_COL_STRING) << "Instruction"
           << uint8_t(BINOUT_COL_STRING) << "Dependency 1"
           << uint8_t(BINOUT_COL_STRING) << "Dependency 2"
           << uint8_t(BINOUT_COL_BOOL)   << "More dependencies"
           << uint8_t(BINOUT_COL_UINT64) << "Tally"
           << uint8_t(BINOUT_COL_NONE);
    for (auto iter = deps_histo.begin(); iter != deps_histo.end(); iter++) {
      InstInfo& info = *iter;
      *bfbin << uint8_t(BINOUT_ROW_DATA);
      for (int o = 0; o < 3; o++)
        *bfbin << (info.opcodes[o] == BF_NO_ARG ? "" : opcode2name[info.opcodes[o]]);
      *bfbin << info.more
             << info.tally;
    }
    *bfbin << uint8_t(BINOUT_ROW_NONE);
  }

  // Report the total counter values across all basic blocks.
  void report_totals (const char* partition, ByteFlopCounters& counter_totals) {
    uint64_t global_bytes = counter_totals.loads + counter_totals.stores;
    uint64_t global_mem_ops = counter_totals.load_ins + counter_totals.store_ins;
    uint64_t global_unique_bytes = 0;
    vector<uint64_t>* reuse_hist;   // Histogram of reuse distances
    uint64_t reuse_unique;          // Unique bytes as measured by the reuse-distance calculator
    bf_get_reuse_distance(&reuse_hist, &reuse_unique);
    if (reuse_unique > 0)
      global_unique_bytes = reuse_unique;
    else
      if (bf_unique_bytes && !partition)
        global_unique_bytes = bf_mem_footprint ? bf_tally_unique_addresses_tb() : bf_tally_unique_addresses();

    // Prepare the tag to use for output, and indicate that we want to
    // use separators in numerical output.
    string tag(bf_output_prefix + "BYFL_SUMMARY");
    if (partition)
      tag += '(' + string(partition) + ')';
    bfout->imbue(locale(""));

    // For convenience, assign names to each of our terminator tallies.
    const uint64_t term_static =
      counter_totals.terminators[BF_END_BB_UNCOND_FAKE] +
      counter_totals.terminators[BF_END_BB_UNCOND_REAL];
    const uint64_t term_dynamic =
      counter_totals.terminators[BF_END_BB_COND_NT] +
      counter_totals.terminators[BF_END_BB_COND_T] +
      counter_totals.terminators[BF_END_BB_INDIRECT] +
      counter_totals.terminators[BF_END_BB_SWITCH];
    const uint64_t term_returns = counter_totals.terminators[BF_END_BB_RETURN];
    const uint64_t term_invokes = counter_totals.terminators[BF_END_BB_INVOKE];
    const uint64_t term_any = counter_totals.terminators[BF_END_BB_ANY];
    uint64_t term_other = term_any;
    for (int i = 0; i < BF_END_BB_NUM; i++)
      if (i != BF_END_BB_ANY)
        term_other -= counter_totals.terminators[i];

    // Produce a histogram that tallies each byte-access count.
    // Identify the number of bytes needed to cover 50% of all dynamic
    // byte accesses.
    vector<bf_addr_tally_t> access_counts;
    uint64_t total_bytes_accessed = 0;
    uint64_t bytes_for_50pct_hits = 0;
    if (bf_mem_footprint && !partition) {
      bf_get_address_tally_hist(access_counts, &total_bytes_accessed);
      uint64_t running_total_bytes = 0;     // Running total of tally (# of addresses)
      uint64_t running_total_accesses = 0;  // Running total of byte-access count times tally.
      for (auto counts_iter = access_counts.cbegin();
           counts_iter != access_counts.cend();
           counts_iter++) {
        running_total_bytes += counts_iter->second;
        running_total_accesses += uint64_t(counts_iter->first) * uint64_t(counts_iter->second);
        double hit_rate = double(running_total_accesses) / double(global_bytes);
        if (running_total_accesses*2 >= global_bytes) {
          bytes_for_50pct_hits = running_total_bytes;
          break;
        }
      }
    }

    // Compute the number of integer operations performed by
    // subtracting flops, memory ops, and branch ops from total ops.
    const uint64_t global_int_ops = counter_totals.ops - counter_totals.flops - global_mem_ops - term_any;

    // Report the raw measurements in terms of bytes and operations.
    *bfout << tag << ": " << separator << '\n';
    *bfout << tag << ": " << setw(25) << global_bytes << " bytes ("
           << counter_totals.loads << " loaded + "
           << counter_totals.stores << " stored)\n";
    if (bf_unique_bytes && !partition)
      *bfout << tag << ": " << setw(25) << global_unique_bytes << " unique bytes\n";
    if (bf_mem_footprint && !partition)
      *bfout << tag << ": " << setw(25) << bytes_for_50pct_hits
             << " addresses cover 50% of all dynamic loads and stores\n";
    *bfout << tag << ": " << setw(25) << counter_totals.flops << " flops\n";
    *bfout << tag << ": " << setw(25) << global_int_ops << " integer ops\n";
    *bfout << tag << ": " << setw(25) << global_mem_ops << " memory ops ("
           << counter_totals.load_ins << " loads + "
           << counter_totals.store_ins << " stores)\n";
    *bfout << tag << ": " << setw(25) << term_any + counter_totals.call_ins << " branch ops ("
           << term_static << " unconditional and direct + "
           << term_dynamic << " conditional or indirect + "
           << counter_totals.call_ins + term_returns + term_invokes
           << " function calls or returns + "
           << term_any - term_static - term_dynamic - term_returns << " other)\n";
    *bfout << tag << ": " << setw(25) << counter_totals.ops + counter_totals.call_ins << " TOTAL OPS\n";

    // Do the same but in binary format.  Note that we include FP bits
    // and op bits here rather than below, in contrast to the
    // text-format output.
    *bfbin << uint8_t(BINOUT_TABLE_KEYVAL)
           << (partition ? string("User-defined tag ") + string(partition) : "Program");
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Load operations"
           << counter_totals.load_ins;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Store operations"
           << counter_totals.store_ins;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Floating-point operations"
           << counter_totals.flops;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Integer operations"
           << global_int_ops;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Function-call operations (non-exception-throwing)"
           << counter_totals.call_ins;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Function-call operations (exception-throwing)"
           << counter_totals.terminators[BF_END_BB_INVOKE];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Unconditional and direct branch operations (removable)"
           << counter_totals.terminators[BF_END_BB_UNCOND_FAKE];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Unconditional and direct branch operations (mandatory)"
           << counter_totals.terminators[BF_END_BB_UNCOND_REAL];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Conditional branch operations (not taken)"
           << counter_totals.terminators[BF_END_BB_COND_NT];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Conditional branch operations (taken)"
           << counter_totals.terminators[BF_END_BB_COND_T];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Unconditional but indirect branch operations"
           << counter_totals.terminators[BF_END_BB_INDIRECT];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Multi-target (switch) branch operations"
           << counter_totals.terminators[BF_END_BB_SWITCH];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Function-return operations"
           << counter_totals.terminators[BF_END_BB_RETURN];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Other branch operations"
           << term_other;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Floating-point operation bits"
           << counter_totals.fp_bits;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Integer operation bits"
           << counter_totals.op_bits;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Bytes loaded"
           << counter_totals.loads;
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Bytes stored"
           << counter_totals.stores;
    if (bf_unique_bytes && !partition)
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Unique addresses loaded or stored"
             << global_unique_bytes;
    if (bf_mem_footprint && !partition)
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Bytes needed to cover half of all dynamic loads and stores"
             << bytes_for_50pct_hits;

    // Output reuse distance (if measured) in both text and binary formats.
    if (reuse_unique > 0) {
      uint64_t median_value;
      uint64_t mad_value;
      bf_get_median_reuse_distance(&median_value, &mad_value);
      *bfout << tag << ": " << setw(25);
      if (median_value == ~(uint64_t)0)
        *bfout << "infinite" << " median reuse distance\n";
      else
        *bfout << median_value << " median reuse distance (+/- "
               << mad_value << ")\n";
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Median reuse distance"
             << median_value;
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "MAD reuse distance"
             << mad_value;
    }
    *bfout << tag << ": " << separator << '\n';

    // Report the raw measurements in terms of bits and bit operations.
    *bfout << tag << ": " << setw(25) << global_bytes*8 << " bits ("
           << counter_totals.loads*8 << " loaded + "
           << counter_totals.stores*8 << " stored)\n";
    if (bf_unique_bytes && !partition)
      *bfout << tag << ": " << setw(25) << global_unique_bytes*8 << " unique bits\n";
    *bfout << tag << ": " << setw(25) << counter_totals.fp_bits << " flop bits\n";
    *bfout << tag << ": " << setw(25) << counter_totals.op_bits << " op bits (excluding memory ops)\n";
    *bfout << tag << ": " << separator << '\n';

    // Report in textual format the amount of memory that passed
    // through llvm.mem{set,cpy,move}.*.
    if (counter_totals.mem_intrinsics[BF_MEMSET_CALLS] > 0)
      *bfout << tag << ": " << setw(25)
             << counter_totals.mem_intrinsics[BF_MEMSET_BYTES]
             << " bytes stored by "
             << counter_totals.mem_intrinsics[BF_MEMSET_CALLS] << ' '
             << (counter_totals.mem_intrinsics[BF_MEMSET_CALLS] == 1 ? "call" : "calls")
             << " to memset()\n";
    if (counter_totals.mem_intrinsics[BF_MEMXFER_CALLS] > 0)
      *bfout << tag << ": " << setw(25)
             << counter_totals.mem_intrinsics[BF_MEMXFER_BYTES]
             << " bytes loaded and stored by "
             << counter_totals.mem_intrinsics[BF_MEMXFER_CALLS] << ' '
             << (counter_totals.mem_intrinsics[BF_MEMXFER_CALLS] == 1 ? "call" : "calls")
             << " to memcpy() or memmove()\n";
    if (counter_totals.mem_intrinsics[BF_MEMSET_CALLS] > 0
        || counter_totals.mem_intrinsics[BF_MEMXFER_CALLS] > 0)
      *bfout << tag << ": " << separator << '\n';

    // Do the same as the above in binary format.  Here, we include
    // all data, even zeroes.
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Calls to memset"
           << counter_totals.mem_intrinsics[BF_MEMSET_CALLS];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Calls to memcpy and memmove"
           << counter_totals.mem_intrinsics[BF_MEMXFER_CALLS];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Bytes stored by memset"
           << counter_totals.mem_intrinsics[BF_MEMSET_BYTES];
    *bfbin << uint8_t(BINOUT_COL_UINT64)
           << "Bytes loaded and stored by memcpy and memmove"
           << counter_totals.mem_intrinsics[BF_MEMXFER_BYTES];

    // Report vector-operation measurements in both textual and binary formats.
    uint64_t num_vec_ops=0, total_vec_elts, total_vec_bits;
    if (bf_vectors) {
      // Compute the vector statistics.
      if (partition)
        bf_get_vector_statistics(partition, &num_vec_ops, &total_vec_elts, &total_vec_bits);
      else
        bf_get_vector_statistics(&num_vec_ops, &total_vec_elts, &total_vec_bits);

      // Output information textually (excluding zeroes).
      *bfout << tag << ": " << setw(25) << num_vec_ops << " vector operations (FP & int)\n";
      if (num_vec_ops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)total_vec_elts / (double)num_vec_ops
               << " elements per vector\n"
               << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)total_vec_bits / (double)num_vec_ops
               << " bits per element\n";
      *bfout << tag << ": " << separator << '\n';

      // Output information in binary format (including zeroes).
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Vector operations"
             << num_vec_ops;
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Total vector elements"
             << total_vec_elts;
      *bfbin << uint8_t(BINOUT_COL_UINT64)
             << "Total vector-element bits"
             << total_vec_bits;
    }

    // Finish the current binary table (program or tag summary).
    *bfbin << uint8_t(BINOUT_COL_NONE);

    // Output raw, per-type information in both textual and binary formats.
    if (bf_types) {
      // The following need to be consistent with byfl-common.h.
      const char *memop2name[] = {"loads of ", "stores of "};
      const char *memref2name[] = {"", "pointers to "};
      const char *memagg2name[] = {"", "vectors of "};
      const char *memwidth2name[] = {"8-bit ", "16-bit ", "32-bit ",
                                     "64-bit ", "128-bit ", "oddly sized "};
      const char *memtype2name[] = {"integers", "floating-point values",
                                    "\"other\" values (not integers or FP values)"};

      // Output all nonzero entries.
      string mem_table_name("Memory accesses by data type");
      if (partition)
        mem_table_name += string(" for tag ") + string(partition);
      *bfbin << uint8_t(BINOUT_TABLE_KEYVAL) << mem_table_name;
      for (int memop = 0; memop < BF_OP_NUM; memop++)
        for (int memref = 0; memref < BF_REF_NUM; memref++)
          for (int memagg = 0; memagg < BF_AGG_NUM; memagg++)
            for (int memwidth = 0; memwidth < BF_WIDTH_NUM; memwidth++)
              for (int memtype = 0; memtype < BF_TYPE_NUM; memtype++) {
                uint64_t idx = mem_type_to_index(memop, memref, memagg, memtype, memwidth);
                uint64_t tally = counter_totals.mem_insts[idx];
                if (tally > 0) {
                  string colname = string(memop2name[memop])
                    + memref2name[memref]
                    + memagg2name[memagg]
                    + memwidth2name[memwidth]
                    + memtype2name[memtype];
                  *bfout << tag << ": " << setw(25) << tally << ' '
                         << colname << '\n';
                  colname[0] = toupper(colname[0]);
                  *bfbin << uint8_t(BINOUT_COL_UINT64)
                         << colname << tally;
                }
              }
      *bfout << tag << ": " << separator << '\n';
      *bfbin << uint8_t(BINOUT_COL_NONE);
    }

    // Pretty-print the histogram of instructions executed in both textual and
    // binary formats.
    uint64_t total_insts = 0;
    if (bf_tally_inst_mix)
      total_insts = report_instruction_mix(partition, counter_totals, tag);

    // Pretty-print the histogram of instruction dependencies in both textual
    // and binary formats.
    if (bf_tally_inst_deps)
      report_instruction_deps(tag);

    // Output quantiles of working-set sizes.
    if (bf_mem_footprint && !partition) {
      // Output every nth quantile textually.
      const double pct_change_text = 0.05;   // Minimum percentage-point change to output textually
      uint64_t running_total_bytes = 0;     // Running total of tally (# of addresses)
      uint64_t running_total_accesses = 0;  // Running total of byte-access count times tally.
      double hit_rate = 0.0;            // Quotient of the preceding two values
      for (auto counts_iter = access_counts.cbegin();
           counts_iter != access_counts.cend();
           counts_iter++) {
        running_total_bytes += counts_iter->second;
        running_total_accesses += uint64_t(counts_iter->first) * uint64_t(counts_iter->second);
        double new_hit_rate = double(running_total_accesses) / double(global_bytes);
        if (new_hit_rate - hit_rate > pct_change_text || running_total_bytes == global_unique_bytes) {
          hit_rate = new_hit_rate;
          *bfout << tag << ": "
                 << setw(25) << running_total_bytes << " bytes cover "
                 << fixed << setw(5) << setprecision(1) << hit_rate*100.0 << "% of memory accesses\n";
        }
      }
      *bfout << tag << ": " << separator << '\n';

      // Output every mth quantile in binary format.
      const double pct_change_bin = 0.001;    // Minimum percentage-point change to output in binary format
      *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Memory locality";
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Cache size"
             << uint8_t(BINOUT_COL_UINT64) << "Upper-bound of hit count"
             << uint8_t(BINOUT_COL_NONE);
      running_total_bytes = 0;
      running_total_accesses = 0;
      hit_rate = 0.0;
      for (auto counts_iter = access_counts.cbegin();
           counts_iter != access_counts.cend();
           counts_iter++) {
        running_total_bytes += counts_iter->second;
        running_total_accesses += uint64_t(counts_iter->first) * uint64_t(counts_iter->second);
        double new_hit_rate = double(running_total_accesses) / double(global_bytes);
        if (new_hit_rate - hit_rate > pct_change_bin || running_total_bytes == global_unique_bytes) {
          hit_rate = new_hit_rate;
          *bfbin << uint8_t(BINOUT_ROW_DATA)
                 << uint64_t(running_total_bytes)
                 << uint64_t(running_total_accesses);
        }
      }
      *bfbin << uint8_t(BINOUT_ROW_NONE);
    }

    // Report a bunch of derived measurements (textually only).
    if (counter_totals.stores > 0) {
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.loads / (double)counter_totals.stores
             << " bytes loaded per byte stored\n";
    }
    if (counter_totals.load_ins > 0)
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.ops / (double)counter_totals.load_ins
             << " ops per load instruction\n";
    if (global_mem_ops > 0)
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes*8 / (double)global_mem_ops
             << " bits loaded/stored per memory op\n";
    if (term_dynamic > 0) {
      if (counter_totals.flops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)counter_totals.flops / (double)term_dynamic
               << " flops per conditional/indirect branch\n";
      if (counter_totals.ops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)counter_totals.ops / (double)term_dynamic
               << " ops per conditional/indirect branch\n";
      if (num_vec_ops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)num_vec_ops / (double)term_dynamic
               << " vector ops (FP & int) per conditional/indirect branch\n";
    }
    if (num_vec_ops > 0) {
      if (counter_totals.flops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)num_vec_ops / (double)counter_totals.flops
               << " vector ops (FP & int) per flop\n";
      if (counter_totals.ops > 0)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)num_vec_ops / (double)counter_totals.ops
               << " vector ops (FP & int) per op\n";
    }
    if (total_insts > 0)
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)counter_totals.ops / (double)total_insts
             << " ops per instruction\n";
    *bfout << tag << ": " << separator << '\n';
    if (counter_totals.flops > 0) {
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes / (double)counter_totals.flops
             << " bytes per flop\n"
             << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes*8.0 / (double)counter_totals.fp_bits
             << " bits per flop bit\n";
    }
    if (counter_totals.ops > 0)
      *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes / (double)counter_totals.ops
             << " bytes per op\n"
             << tag << ": " << fixed << setw(25) << setprecision(4)
             << (double)global_bytes*8.0 / (double)counter_totals.op_bits
             << " bits per (non-memory) op bit\n";
    if (!partition) {
      if (bf_unique_bytes && (counter_totals.flops > 0 || counter_totals.ops > 0)) {
        *bfout << tag << ": " << separator << '\n';
        if (counter_totals.flops > 0)
          *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
                 << (double)global_unique_bytes / (double)counter_totals.flops
                 << " unique bytes per flop\n"
                 << tag << ": " << fixed << setw(25) << setprecision(4)
                 << (double)global_unique_bytes*8.0 / (double)counter_totals.fp_bits
                 << " unique bits per flop bit\n";
        if (counter_totals.ops > 0)
          *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
                 << (double)global_unique_bytes / (double)counter_totals.ops
                 << " unique bytes per op\n"
                 << tag << ": " << fixed << setw(25) << setprecision(4)
                 << (double)global_unique_bytes*8.0 / (double)counter_totals.op_bits
                 << " unique bits per (non-memory) op bit\n";
      }
      if (bf_unique_bytes)
        *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
               << (double)global_bytes / (double)global_unique_bytes
               << " bytes per unique byte\n";
      *bfout << tag << ": " << separator << '\n';}
  }

  // Report cache performance if it was used.
  void report_cache (ByteFlopCounters& counter_totals) {
    // Accumulate our measured cache data.
    const int n = 3;   // n different dump files are created.
    uint64_t accesses[n] = {bf_get_private_cache_accesses(),
                            bf_get_shared_cache_accesses(),
                            bf_get_shared_cache_accesses()};
    vector<unordered_map<uint64_t,uint64_t> > hits[n] = {bf_get_private_cache_hits(),
                                                         bf_get_shared_cache_hits(),
                                                         bf_get_remote_shared_cache_hits()};
    uint64_t cold_misses[n] = {bf_get_private_cold_misses(),
                               bf_get_shared_cold_misses(),
                               bf_get_shared_cold_misses()};
    uint64_t misaligned_mem_ops[n] = {bf_get_private_misaligned_mem_ops(),
                                      bf_get_shared_misaligned_mem_ops(),
                                      bf_get_shared_misaligned_mem_ops()};

    // Write detailed information for both shared and private caches.
    // TODO: Write this information only to the binary output file.
    string names[n]{"private-cache.dump",
        "shared-cache.dump",
        "remote-shared-cache.dump"};
    string table_names[n]{
        "Private cache",
        "Shared cache",
        "Remote shared cache"};
    for (int i = 0; i < n; ++i) {
      // Write some header information.
      ofstream dumpfile(names[i]);
      dumpfile << "Total cache accesses\t" << accesses[i] << endl;
      dumpfile << "Cold misses\t" << cold_misses[i] << endl;
      dumpfile << "Line size\t" << bf_line_size << endl;
      *bfbin << uint8_t(BINOUT_TABLE_KEYVAL) << (table_names[i] + " summary");
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Total cache accesses" << accesses[i]
             << uint8_t(BINOUT_COL_UINT64) << "Cold misses" << cold_misses[i]
             << uint8_t(BINOUT_COL_UINT64) << "Line size" << bf_line_size
             << uint8_t(BINOUT_COL_NONE);

      // Dump pairs of {lines searched, tally} for each set size.
      *bfbin << uint8_t(BINOUT_TABLE_BASIC) << table_names[i] + " model data";
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Set size"
             << uint8_t(BINOUT_COL_UINT64) << "LRU search distance"
             << uint8_t(BINOUT_COL_UINT64) << "Tally"
             << uint8_t(BINOUT_COL_NONE);
      for (uint64_t set = 0; set < bf_max_set_bits; ++set) {
        uint64_t num_sets = 1<<set;
        dumpfile << "Sets\t" << num_sets << endl;
        for (const auto& elem : hits[i][set]) {
          dumpfile << elem.first << "\t" << elem.second << endl;
          *bfbin << uint8_t(BINOUT_ROW_DATA)
                 << num_sets << elem.first << elem.second;
        }
      }

      // Close the current dump file.
      *bfbin << uint8_t(BINOUT_ROW_NONE);
      dumpfile.close();
    }

    // Output textual summary information.
    string tag(bf_output_prefix + "BYFL_SUMMARY");
    uint64_t global_mem_ops = counter_totals.load_ins + counter_totals.store_ins;
    *bfout << tag << ": " << setw(25)
           << accesses[0] << " cache lines accessed (due to "
           << global_mem_ops - misaligned_mem_ops[0] << " aligned + "
           << misaligned_mem_ops[0] << " misaligned memory ops; "
           << "line size = " << bf_line_size << " bytes)\n";
    *bfout << tag << ": " << separator << '\n';

    // Output binary summary information.
    *bfbin << uint8_t(BINOUT_TABLE_KEYVAL) << "Cache model";
    *bfbin << uint8_t(BINOUT_COL_UINT64) << "Modeled line size (bytes)" << bf_line_size
           << uint8_t(BINOUT_COL_UINT64) << "Cache accesses" << accesses[0]
           << uint8_t(BINOUT_COL_UINT64) << "Aligned memory operations" << global_mem_ops - misaligned_mem_ops[0]
           << uint8_t(BINOUT_COL_UINT64) << "Misaligned memory operations" << misaligned_mem_ops[0]
           << uint8_t(BINOUT_COL_NONE);
  }

  // Report miscellaneous information in the binary output file.
  void report_misc_info() {
    // Report the list of environment variables that are currently active.
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Environment variables";
    *bfbin << uint8_t(BINOUT_COL_STRING) << "Variable"
           << uint8_t(BINOUT_COL_STRING) << "Value"
           << uint8_t(BINOUT_COL_NONE);
    class compare_case_insensitive
    {
    public:
      // Compare two strings in a case-insensitive manner.
      bool operator() (const string& a, const string& b) const {
        return strcasecmp(a.c_str(), b.c_str()) < 0;
      }
    };
    map<string, string, compare_case_insensitive> environment_variables;
    for (char** ep = environ; *ep != NULL; ep++) {
      char* var_val_str = strdup(*ep);
      char* eqptr = strchr(var_val_str, '=');
      if (eqptr != NULL) {
        *eqptr = '\0';
        environment_variables[string(var_val_str)] = string(eqptr + 1);
      }
      free(var_val_str);
    }
    for (auto iter = environment_variables.cbegin(); iter != environment_variables.cend(); iter++)
      *bfbin << uint8_t(BINOUT_ROW_DATA)
             << iter->first << iter->second;
    *bfbin << uint8_t(BINOUT_ROW_NONE);

    // Report the program command line, if possible (probably only Linux).
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Command line";
    *bfbin << uint8_t(BINOUT_COL_STRING) << "Argument"
           << uint8_t(BINOUT_COL_NONE);
    vector<string> command_line = parse_command_line();   // All command-line arguments
    for (auto iter = command_line.cbegin(); iter != command_line.cend(); iter++)
      *bfbin << uint8_t(BINOUT_ROW_DATA) << *iter;
    *bfbin << uint8_t(BINOUT_ROW_NONE);

    // Report the Byfl options specified at compile time.  We're given
    // these as a single string so we split the string heuristically
    // by assuming that each option begins with a space followed by
    // "-bf-".
    char* optstr = strdup(bf_option_string);
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Byfl options";
    *bfbin << uint8_t(BINOUT_COL_STRING) << "Option"
           << uint8_t(BINOUT_COL_NONE);
    char* one_opt;       // Pointer into optstr
    char* optr = optstr; // Pointer to the next " -bf-"
    for (one_opt = optstr; optr; one_opt += strlen(one_opt) + 1) {
      optr = strstr(one_opt, " -bf-");
      if (optr)
        *optr = '\0';
      *bfbin << uint8_t(BINOUT_ROW_DATA) << one_opt;
    }
    *bfbin << uint8_t(BINOUT_ROW_NONE);
    free(optstr);
  }

public:
  RunAtEndOfProgram() {
    separator = "-----------------------------------------------------------------";
  }

  ~RunAtEndOfProgram() {
    // Do nothing if our output is suppressed.
    bf_initialize_if_necessary();
    if (suppress_output() || bf_abnormal_exit)
      return;

    // Complete the basic-block table.
    finalize_bblocks();

    // Report the number of times each basic block was executed.
    if (bf_every_bb)
      bf_report_bb_execution();

    // Report per-function counter totals.
    if (bf_per_func)
      report_by_function();

    // Output a histogram of vector usage.
    if (bf_vectors)
      bf_report_vector_operations(call_stack->max_depth);

    // Report per-data-structure counts if requested.
    if (bf_data_structs)
      bf_report_data_struct_counts();

    // Report user-defined counter totals, if any.
    vector<const char*>* all_tag_names = user_defined_totals().sorted_keys(compare_char_stars);
    for (vector<const char*>::const_iterator tag_iter = all_tag_names->cbegin();
         tag_iter != all_tag_names->cend();
         tag_iter++) {
      const char* tag_name = *tag_iter;
      report_totals(tag_name, *user_defined_totals()[tag_name]);
    }

    // Report the global counter totals across all basic blocks.
    report_totals(NULL, global_totals);

    // Report the cache performance if it was turned on.
    if (bf_cache_model)
      report_cache(global_totals);

    // Report anything else we can think to report.
    report_misc_info();

    // Flush our output data before exiting.
    bfout->flush();
    *bfbin << uint8_t(BINOUT_TABLE_NONE);
    bfbin->flush();
  }
} run_at_end_of_program;

} // namespace bytesflops

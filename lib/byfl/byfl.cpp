/*
 * Helper library for computing bytes:flops ratios
 * (core functions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

const unsigned int HDR_COL_WIDTH = 20;

#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)
#define FUNCTION_FORMAT_STR "%" STR(MAX_FUNCTION_NAMELEN) "s"
#define REPORT_DERIVED(OUTSTRM, TAG, MEASURE, TEXTSTR) if (MEASURE >= 0) *OUTSTRM << TAG << ": " << fixed << setw(25) << setprecision(4) << MEASURE << TEXTSTR

// Define the different ways a basic block can terminate.
typedef enum {
  BB_NOT_END=0,       // Basic block has not actually terminated.
  BB_END_UNCOND=1,    // Basic block terminated with an unconditional branch.
  BB_END_COND=2       // Basic block terminated with a conditional branch.
} bb_end_t;


// Encapsulate of all of our counters into a single structure.
class ByteFlopCounters {
public:
  uint64_t mem_insts[NUM_MEM_INSTS];  // Number of memory instructions by type
  uint64_t inst_mix_histo[NUM_OPCODES];   // Histogram of instruction mix
  uint64_t terminators[BF_END_BB_NUM];    // Tally of basic-block terminator types
  uint64_t mem_intrinsics[BF_NUM_MEM_INTRIN];  // Tallies of data movement performed by memory intrinsics
  uint64_t loads;                     // Number of bytes loaded
  uint64_t stores;                    // Number of bytes stored
  uint64_t load_ins;                  // Number of load instructions executed
  uint64_t store_ins;                 // Number of store instructions executed
  uint64_t flops;                     // Number of floating-point operations performed
  uint64_t fp_bits;                   // Number of bits consumed or produced by all FP operations
  uint64_t ops;                       // Number of operations of any type performed
  uint64_t op_bits;                   // Number of bits consumed or produced by any operation except loads/stores

  // Initialize all of the counters.
  ByteFlopCounters (uint64_t* initial_mem_insts=NULL,
                    uint64_t* initial_inst_mix_histo=NULL,
                    uint64_t* initial_terminators=NULL,
                    uint64_t* initial_mem_intrinsics=NULL,
                    uint64_t initial_loads=0,
                    uint64_t initial_stores=0,
                    uint64_t initial_load_ins=0,
                    uint64_t initial_store_ins=0,
                    uint64_t initial_flops=0,
                    uint64_t initial_fp_bits=0,
                    uint64_t initial_ops=0,
                    uint64_t initial_op_bits=0) {
    // Initialize mem_insts only if -bf-types was specified.
    if (bf_types) {
      if (initial_mem_insts == NULL)
        for (size_t i = 0; i < NUM_MEM_INSTS; i++)
          mem_insts[i] = 0;
      else
        for (size_t i = 0; i < NUM_MEM_INSTS; i++)
          mem_insts[i] = initial_mem_insts[i];
    }

    // Initialize inst_mix_histo only if -bf-inst-mix was specified.
    if (bf_tally_inst_mix) {
      if (initial_inst_mix_histo == NULL)
        for (size_t i = 0; i < NUM_OPCODES; i++)
          inst_mix_histo[i] = 0;
      else
        for(size_t i = 0; i < NUM_OPCODES; i++)
          inst_mix_histo[i] = initial_inst_mix_histo[i];
    }

    // Unconditionally initialize everything else.
    if (initial_terminators == NULL)
      for (size_t i = 0; i < BF_END_BB_NUM; i++)
        terminators[i] = 0;
    else
      for (size_t i = 0; i < BF_END_BB_NUM; i++)
        terminators[i] = initial_terminators[i];
    if (initial_mem_intrinsics == NULL)
      for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
        mem_intrinsics[i] = 0;
    else
      for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
        mem_intrinsics[i] = initial_mem_intrinsics[i];
    loads    = initial_loads;
    stores   = initial_stores;
    load_ins = initial_load_ins;
    store_ins= initial_store_ins;
    flops    = initial_flops;
    fp_bits  = initial_fp_bits;
    ops      = initial_ops;
    op_bits  = initial_op_bits;
  }

  // Accumulate new values into our counters.
  void accumulate (uint64_t* more_mem_insts,
                   uint64_t* more_inst_mix_histo,
                   uint64_t* more_terminators,
                   uint64_t* more_mem_intrinsics,
                   uint64_t more_loads,
                   uint64_t more_stores,
                   uint64_t more_load_ins,
                   uint64_t more_store_ins,
                   uint64_t more_flops,
                   uint64_t more_fp_bits,
                   uint64_t more_ops,
                   uint64_t more_op_bits) {
    // Accumulate mem_insts only if -bf-types was specified.
    if (bf_types)
      for (size_t i = 0; i < NUM_MEM_INSTS; i++)
        mem_insts[i] += more_mem_insts[i];

    // Accumulate inst_mix_histo only if -bf-inst-mix was specified.
    if (bf_tally_inst_mix)
      for (size_t i = 0; i < NUM_OPCODES; i++)
        inst_mix_histo[i] += more_inst_mix_histo[i];

    // Unconditionally accumulate everything else.
    for (size_t i = 0; i < BF_END_BB_NUM; i++)
      terminators[i] += more_terminators[i];
    for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
      mem_intrinsics[i] += more_mem_intrinsics[i];
    loads     += more_loads;
    stores    += more_stores;
    load_ins  += more_load_ins;
    store_ins += more_store_ins;
    flops     += more_flops;
    fp_bits   += more_fp_bits;
    ops       += more_ops;
    op_bits   += more_op_bits;
  }

  // Accumulate another counter's values into our counters.
  void accumulate (ByteFlopCounters* other) {
    // Accumulate mem_insts only if -bf-types was specified.
    if (bf_types)
      for (size_t i = 0; i < NUM_MEM_INSTS; i++)
        mem_insts[i] += other->mem_insts[i];

    // Accumulate inst_mix_histo only if -bf-inst-mix was specified.
    if (bf_tally_inst_mix)
      for (size_t i = 0; i < NUM_OPCODES; i++)
        inst_mix_histo[i] += other->inst_mix_histo[i];

    // Unconditionally accumulate everything else.
    for (size_t i = 0; i < BF_END_BB_NUM; i++)
      terminators[i] += other->terminators[i];
    for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
      mem_intrinsics[i] += other->mem_intrinsics[i];
    loads     += other->loads;
    stores    += other->stores;
    load_ins  += other->load_ins;
    store_ins += other->store_ins;
    flops     += other->flops;
    fp_bits   += other->fp_bits;
    ops       += other->ops;
    op_bits   += other->op_bits;
  }

  // Return the difference of our counters and another set of counters.
  ByteFlopCounters* difference (ByteFlopCounters* other) {
    // Take the difference of mem_insts only if -bf-types was specified.
    uint64_t delta_mem_insts[NUM_MEM_INSTS];
    if (bf_types)
      for (size_t i = 0; i < NUM_MEM_INSTS; i++)
        delta_mem_insts[i] = mem_insts[i] - other->mem_insts[i];

    // Take the difference of inst_mix_histo only if -bf-inst-mix was specified.
    uint64_t delta_inst_mix_histo[NUM_OPCODES];
    if (bf_tally_inst_mix)
      for (size_t i = 0; i < NUM_OPCODES; i++)
        delta_inst_mix_histo[i] = inst_mix_histo[i] - other->inst_mix_histo[i];

    // Unconditionally take the difference of everything else.
    uint64_t delta_terminators[BF_END_BB_NUM];
    for (size_t i = 0; i < BF_END_BB_NUM; i++)
      delta_terminators[i] = terminators[i] - other->terminators[i];
    uint64_t delta_mem_intrinsics[BF_NUM_MEM_INTRIN];
    for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
      delta_mem_intrinsics[i] = mem_intrinsics[i] - other->mem_intrinsics[i];
    ByteFlopCounters *byflc = new ByteFlopCounters(delta_mem_insts,
                                                   delta_inst_mix_histo,
                                                   delta_terminators,
                                                   delta_mem_intrinsics,
                                                   loads - other->loads,
                                                   stores - other->stores,
                                                   load_ins - other->load_ins,
                                                   store_ins - other->store_ins,
                                                   flops - other->flops,
                                                   fp_bits - other->fp_bits,
                                                   ops - other->ops,
                                                   op_bits - other->op_bits);
    return byflc;
  }

  // Reset all of our counters to zero.
  void reset (void) {
    // Reset mem_insts only if -bf-types was specified.
    if (bf_types)
      for (size_t i = 0; i < NUM_MEM_INSTS; i++)
        mem_insts[i] = 0;

    // Reset inst_mix_histo only if -bf-inst-mix was specified.
    if (bf_tally_inst_mix)
      for (size_t i = 0; i < NUM_OPCODES; i++)
        inst_mix_histo[i] = 0;

    // Unconditionally reset everything else.
    for (size_t i = 0; i < BF_END_BB_NUM; i++)
      terminators[i] = 0;
    for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
      mem_intrinsics[i] = 0;
    loads     = 0;
    stores    = 0;
    load_ins  = 0;
    store_ins = 0;
    flops     = 0;
    fp_bits   = 0;
    ops       = 0;
    op_bits   = 0;
  }
};

// The following values get reset at the end of every basic block.
uint64_t  bf_load_count       = 0;    // Tally of the number of bytes loaded
uint64_t  bf_store_count      = 0;    // Tally of the number of bytes stored
uint64_t* bf_mem_insts_count  = NULL; // Tally of memory instructions by type
uint64_t* bf_inst_mix_histo   = NULL; // Tally of instruction mix (as histogram)
uint64_t* bf_terminator_count = NULL; // Tally of terminators by type
uint64_t* bf_mem_intrin_count = NULL; // Tally of memory intrinsic calls and data movement
uint64_t  bf_load_ins_count   = 0;    // Tally of the number of load instructions performed
uint64_t  bf_store_ins_count  = 0;    // Tally of the number of store instructions performed
uint64_t  bf_flop_count       = 0;    // Tally of the number of FP operations performed
uint64_t  bf_fp_bits_count    = 0;    // Tally of the number of bits used by all FP operations
uint64_t  bf_op_count         = 0;    // Tally of the number of operations performed
uint64_t  bf_op_bits_count    = 0;    // Tally of the number of bits used by all operations except loads/stores

// The following values represent more persistent counter and other state.
static uint64_t num_merged = 0;    // Number of basic blocks merged so far
static ByteFlopCounters global_totals;  // Global tallies of all of our counters
static ByteFlopCounters prev_global_totals;  // Previously reported global tallies of all of our counters

// Keep track of counters on a per-function basis, being careful to
// work around the "C++ static initialization order fiasco" (cf. the
// C++ FAQ).
typedef CachedUnorderedMap<const char*, ByteFlopCounters*> str2bfc_t;
typedef str2bfc_t::iterator counter_iterator;
static str2bfc_t& per_func_totals()
{
  static str2bfc_t* mapping = new str2bfc_t();
  return *mapping;
}
typedef CachedUnorderedMap<const char*, uint64_t> str2num_t;
static str2num_t& func_call_tallies()
{
  static str2num_t* mapping = new str2num_t();
  return *mapping;
}

// Keep track of counters on a user-defined basis, being careful to
// work around the "C++ static initialization order fiasco" (cf. the
// C++ FAQ).
static str2bfc_t& user_defined_totals()
{
  static str2bfc_t* mapping = new str2bfc_t();
  return *mapping;
}

// bf_categorize_counters() is intended to be overridden by a
// user-defined function.
extern "C" {
  static const char* bf_categorize_counters_original (void)
  {
    return NULL;
  }
  const char* bf_categorize_counters (void) __attribute__((weak, alias("bf_categorize_counters_original")));
}

namespace bytesflops {

const char* bf_func_and_parents; // Top of the complete_call_stack stack
string bf_output_prefix;         // String to output before "BYFL" on every line
string bf_binary_output_filename;       // Name for binary output file
string bf_output_id;            // unique identifier for this output (eg. rank, nodename, etc.)
ostream* bfout;                  // Stream to which to send standard output
ofstream* bfbinout = NULL;                  // Stream to which to send binary output
bool use_bf_db = false;
char* bf_db_location;
char* bf_db_name;
char* bf_db_user;
char* bf_db_password;
int bbid = 0; // unique basic block id
uint64_t utc_sec = 0;
uint64_t utc_usec = 0;
char datetime_str[40];

// Define a stack of tallies of all of our counters across <=
// num_merged basic blocks.  This *ought* to be a top-level variable
// defintion, but because of the "C++ static initialization order
// fiasco" (cf. the C++ FAQ) we have to use the following roundabout
// approach.
typedef vector<ByteFlopCounters*> counter_vector_t;
static counter_vector_t& bb_totals (void)
{
  static counter_vector_t* bfc = new counter_vector_t();
  return *bfc;
}

// Define a memory pool for ByteFlopCounters.
class CounterMemoryPool {
private:
  vector<ByteFlopCounters*> freelist;
public:
  // Allocate a new ByteFlopCounters -- from the free list if possible.
  ByteFlopCounters* allocate(void) {
    ByteFlopCounters* newBFC;
    if (freelist.size() > 0) {
      newBFC = freelist.back();
      newBFC->reset();
      freelist.pop_back();
    }
    else
      newBFC = new ByteFlopCounters();
    return newBFC;
  }

  // Return a ByteFlopCounters to the free list.
  void deallocate(ByteFlopCounters* oldBFC) {
    freelist.push_back(oldBFC);
  }
};
static CounterMemoryPool* counter_memory_pool = NULL;

// Maintain a function call stack.
class CallStack {
private:
  vector<const char*> complete_call_stack;  // Stack of function and ancestor names
public:
  size_t max_depth;   // Maximum depth achieved by complete_call_stack

  CallStack() {
    bf_func_and_parents = "-";
    max_depth = 0;
  }

  // Push a function name onto the stack.  Return a string containing
  // the name of the function followed by the names of all of its
  // ancestors.
  const char* push_function (const char* funcname) {
    // Push both the current function name and the combined name of the
    // function and its call stack.
    static char* combined_name = NULL;
    static size_t len_combined_name = 0;
    const char* unique_combined_name;      // Interned version of combined_name
    size_t current_stack_depth = complete_call_stack.size();
    if (current_stack_depth == 0) {
      // First function on the call stack
      len_combined_name = strlen(funcname) + 1;
      combined_name = (char*) malloc(len_combined_name);
      strcpy(combined_name, funcname);
      max_depth = 1;
    }
    else {
      // All other calls (the common case)
      const char* ancestors_names = complete_call_stack.back();
      size_t length_needed = strlen(funcname) + strlen(ancestors_names) + 2;
      if (len_combined_name < length_needed) {
        len_combined_name = length_needed*2;
        combined_name = (char*) realloc(combined_name, len_combined_name);
      }
      sprintf(combined_name, "%s %s", funcname, ancestors_names);
      current_stack_depth++;
      if (current_stack_depth > max_depth)
        max_depth = current_stack_depth;
    }
    unique_combined_name = bf_string_to_symbol(combined_name);
    complete_call_stack.push_back(unique_combined_name);
    return unique_combined_name;
  }

  // Pop a function name from the call stack and return the new top of
  // the call stack (function + ancestors).
  const char* pop_function (void) {
    complete_call_stack.pop_back();
    if (complete_call_stack.size() > 0)
      return complete_call_stack.back();
    else
      return "[EMPTY]";
  }
};
static CallStack* call_stack = NULL;

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


int set_time() {
  if (utc_sec != 0) return -1;
  struct timeval now;
  struct tm* ptm;
  int rc;
  char usec_str[10];
  rc=gettimeofday(&now, NULL);
  if(rc==0) {
    utc_sec =  now.tv_sec;
    utc_usec = now.tv_usec;
    ptm = localtime(&now.tv_sec);
    strftime(datetime_str, sizeof(datetime_str), "%F %T.", ptm);
    sprintf (usec_str, "%03ld", utc_usec); 
    strcat(datetime_str, usec_str);
  } else {
    printf("gettimeofday() failed, errno = %d\n", errno);
    return -1;
  }
 
  printf("SET TIME: sec: %" PRIu64 ", usec: %" PRIu64 ", datetime: %s\n", utc_sec, utc_usec, datetime_str);

  return 0;
}


// Initialize some of our variables at first use.
void initialize_byfl (void) {
  call_stack = new CallStack();
  counter_memory_pool = new CounterMemoryPool();
  if (bf_types) {
    bf_mem_insts_count = new uint64_t[NUM_MEM_INSTS];
    for (size_t i = 0; i < NUM_MEM_INSTS; i++)
      bf_mem_insts_count[i] = 0;
  }
  if (bf_tally_inst_mix) {
    bf_inst_mix_histo = new uint64_t[NUM_OPCODES];
    for (unsigned int i = 0; i < NUM_OPCODES; i++)
      bf_inst_mix_histo[i] = 0;
  }
  bf_terminator_count = new uint64_t[BF_END_BB_NUM];
  for (unsigned int i = 0; i < BF_END_BB_NUM; i++)
    bf_terminator_count[i] = 0;
  bf_mem_intrin_count = new uint64_t[BF_NUM_MEM_INTRIN];
  for (unsigned int i = 0; i < BF_NUM_MEM_INTRIN; i++)
    bf_mem_intrin_count[i] = 0;
  bf_push_basic_block();
}


// Initialize on first use all top-level variables in all files.  This
// is a kludge to work around the "C++ static initialization order
// fiasco" (cf. the C++ FAQ).  bf_initialize_if_necessary() can safely
// be called multiple times.
void bf_initialize_if_necessary (void)
{
  static bool initialized = false;
  if (!__builtin_expect(initialized, true)) {
    initialize_byfl();
    initialize_reuse();
    initialize_symtable();
    initialize_threading();
    initialize_ubytes();
    initialize_tallybytes();
    initialize_vectors();
    set_time();
    initialized = true;
  }
}


// Push a new basic block onto the stack (before a function call).
void bf_push_basic_block (void)
{
  bb_totals().push_back(counter_memory_pool->allocate());
}


// Pop and discard the top basic block off the stack (after a function
// returns).
void bf_pop_basic_block (void)
{
  counter_memory_pool->deallocate(bb_totals().back());
  bb_totals().pop_back();
}


// Tally the number of calls to each function.
void bf_incr_func_tally (const char* funcname)
{
  const char* unique_name = bf_string_to_symbol(funcname);
  func_call_tallies()[unique_name]++;
}


// Push a function name onto the call stack.  Increment the invocation
// count the call stack as a whole, and ensure the individual function
// name also exists in the hash table.
void bf_push_function (const char* funcname)
{
  const char* unique_combined_name = call_stack->push_function(funcname);
  bf_func_and_parents = unique_combined_name;
  func_call_tallies()[bf_func_and_parents]++;
  const char* unique_name = bf_string_to_symbol(funcname);
  func_call_tallies()[unique_name] += 0;
}


// Pop the top function name from the call stack.
void bf_pop_function (void)
{
  bf_func_and_parents = call_stack->pop_function();
}


// Determine if we should suppress output from this process.
static bool suppress_output (void)
{
  if (!all_constructors_called)
    // If cout hasn't been constructed, force all output to be suppressed.
    return true;
  static enum {UNKNOWN, SUPPRESS, SHOW} output = UNKNOWN;
  if (output == UNKNOWN) {
    // First invocation -- we can begin outputting.
    output = SHOW;

    // check if we want output to go to stdout or not
    char *bf_no_text = getenv("BF_NO_TEXT");
    if (bf_no_text) {
      bfout = NULL;
    } else {
      bfout = &cout;
    }

    // If the BF_PREFIX environment variable is set, expand it and
    // output it before each line of output.
    char *prefix = getenv("BF_PREFIX");
    if (prefix) {
      // Perform shell expansion on BF_PREFIX.
      wordexp_t expansion;
      if (wordexp(prefix, &expansion, 0)) {
        cerr << "Failed to expand BF_PREFIX (\"" << prefix << "\")\n";
        exit(1);
      }
      for (size_t i = 0; i < expansion.we_wordc; i++)
        bf_output_prefix += string(expansion.we_wordv[i]) + string(" ");
      wordfree(&expansion);

      // If the prefix starts with "/" or "./", treat it as a filename
      // and write all output there.
      if ((bf_output_prefix.size() >= 1 && bf_output_prefix[0] == '/')
          || (bf_output_prefix.size() >= 2 && bf_output_prefix[0] == '.' && bf_output_prefix[1] == '/')) {
        bf_output_prefix.resize(bf_output_prefix.size() - 1);  // Drop the trailing space character.
        bfout = new ofstream(bf_output_prefix.c_str(), ios_base::out | ios_base::trunc);
        if (bfout->fail()) {
          cerr << "Failed to create output file " << bf_output_prefix << '\n';
          exit(1);
        }
        bf_output_prefix = "";
      }
    }

    

    // If the BF_OUTPUT_BINARY environment variable is set,
    // it indicates a file name for the binary output
    char *binary_file = getenv("BF_OUTPUT_BINARY");
    if (binary_file) {
      cout << "Binary file is specified!\n";
      // Perform shell expansion on BF_OUTPUT_BINARY.
      wordexp_t expansion;
      if (wordexp(binary_file, &expansion, 0)) {
        cerr << "Failed to expand BF_OUTPUT_BINARY (\"" << binary_file << "\")\n";
        exit(1);
      }
      for (size_t i = 0; i < expansion.we_wordc; i++)
        bf_binary_output_filename += string(expansion.we_wordv[i]) + string(" ");
      wordfree(&expansion);

      if ((bf_binary_output_filename.size() >= 1 && bf_binary_output_filename[0] == '/')
          || (bf_binary_output_filename.size() >= 2 && bf_binary_output_filename[0] == '.' && bf_binary_output_filename[1] == '/')) {
        bf_binary_output_filename.resize(bf_binary_output_filename.size() - 1);  // Drop the trailing space character.
        bfbinout = new ofstream(bf_binary_output_filename.c_str(), 
          ios_base::out | ios_base::trunc | ios::binary);
        if (bfbinout->fail()) {
          cerr << "Failed to create binary output file " << bf_binary_output_filename << '\n';
          exit(1);
        }
      }
    }

    // If the BF_OUTPUT_ID environment variable is set,
    // it indicates a unique ID such has rank number or host name
    char *output_id = getenv("BF_OUTPUT_ID");
    if (output_id) {
      // Perform shell expansion on BF_OUTPUT_ID.
      wordexp_t expansion;
      if (wordexp(output_id, &expansion, 0)) {
        cerr << "Failed to expand BF_OUTPUT_ID (\"" << output_id << "\")\n";
        exit(1);
      }
      for (size_t i = 0; i < expansion.we_wordc; i++)
        bf_output_id += string(expansion.we_wordv[i]) + string(" ");
      wordfree(&expansion);
    }

    if (get_db_vars(&bf_db_location, &bf_db_name, &bf_db_user, &bf_db_password)) {
      if (connect_database(bf_db_location, bf_db_name, bf_db_user, bf_db_password)) {
        use_bf_db = true;
      } 
    }

    // Log the Byfl command line to help users reproduce their results.
    if (bfout) 
      *bfout << "BYFL_INFO: Byfl command line: " << bf_option_string << '\n';

    // Warn the user if he defined bf_categorize_counters() but didn't
    // compile with -bf-every-bb.
    if (bf_categorize_counters != bf_categorize_counters_original && !bf_every_bb)
      if (bfout)
        *bfout << "BYFL_WARNING: bf_categorize_counters() has no effect without -bf-every-bb.\n"
             << "BYFL_WARNING: Consider using -bf-every-bb -bf-merge-bb="
             << uint64_t(-1) << ".\n";
  }
  return output == SUPPRESS;
}


// At the end of a basic block, accumulate the current counter
// variables (bf_*_count) into the current basic block's counters and
// into the global counters.
void bf_accumulate_bb_tallies (void)
{
  // Add the current values to the per-BB totals.
  ByteFlopCounters* current_bb = bb_totals().back();
  current_bb->accumulate(bf_mem_insts_count,
                         bf_inst_mix_histo,
                         bf_terminator_count,
                         bf_mem_intrin_count,
                         bf_load_count,
                         bf_store_count,
                         bf_load_ins_count,
                         bf_store_ins_count,
                         bf_flop_count,
                         bf_fp_bits_count,
                         bf_op_count,
                         bf_op_bits_count);
  global_totals.accumulate(current_bb);
  const char* partition = bf_string_to_symbol(bf_categorize_counters());
  if (partition != NULL) {
    counter_iterator sm_iter = user_defined_totals().find(partition);
    if (sm_iter == per_func_totals().end())
      user_defined_totals()[partition] = new ByteFlopCounters(*current_bb);
    else
      user_defined_totals()[partition]->accumulate(current_bb);
  }
}

// Reset the current basic block's tallies rather than requiring a
// push and a pop for every basic block.
void bf_reset_bb_tallies (void)
{
  bb_totals().back()->reset();
}

// Report what we've measured for the current basic block.
void bf_report_bb_tallies (void)
{
  static bool showed_header = false;         // true=already output our header

  // Do nothing if our output is suppressed.
  if (suppress_output())
    return;

  // If this is our first invocation, output a basic-block header line.
  if (__builtin_expect(!showed_header, 0)) {
    if (bfout)  {
      *bfout << bf_output_prefix
           << "BYFL_BB_HEADER: "
           << setw(HDR_COL_WIDTH) << "LD_bytes" << ' '
           << setw(HDR_COL_WIDTH) << "ST_bytes" << ' '
           << setw(HDR_COL_WIDTH) << "LD_ops" << ' '
           << setw(HDR_COL_WIDTH) << "ST_ops" << ' '
           << setw(HDR_COL_WIDTH) << "Flops" << ' '
           << setw(HDR_COL_WIDTH) << "FP_bits" << ' '
           << setw(HDR_COL_WIDTH) << "Int_ops" << ' '
           << setw(HDR_COL_WIDTH) << "Int_op_bits";
      *bfout << '\n';
    }
    showed_header = true;
  }

  // If we've accumulated enough basic blocks, output the aggregate of
  // their values.
  if (++num_merged >= bf_bb_merge) {
    // Output the difference between the current counter values and
    // our previously saved values.
    ByteFlopCounters* counter_deltas = global_totals.difference(&prev_global_totals);
    if (bfout) {
      *bfout << bf_output_prefix
      << "BYFL_BB:        "
      << setw(HDR_COL_WIDTH) << counter_deltas->loads << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->stores << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->load_ins << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->store_ins << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->flops << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->fp_bits << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->ops << ' '
      << setw(HDR_COL_WIDTH) << counter_deltas->op_bits;
    *bfout << '\n';
    }

    if (bfbinout) { 
      bf_table_t table = BF_BASICBLOCKS;

      // write out the type of table entry this is
      bfbinout->write((char*)&table, sizeof(bf_table_t));

      // write out the data for this basicblocks table entry
      bf_basicblocks_table bftable = {utc_sec, utc_usec, bbid, num_merged, 
        counter_deltas->loads, counter_deltas->stores, 
        counter_deltas->load_ins, counter_deltas->store_ins,
        counter_deltas->flops, counter_deltas->fp_bits, 
        counter_deltas->ops, counter_deltas->op_bits};

      bfbinout->write((char*)&bftable, sizeof(bf_basicblocks_table));
    }

    if (use_bf_db) {
      insert_basicblocks(utc_sec, utc_usec, bbid, num_merged,
        counter_deltas->loads, counter_deltas->stores, 
        counter_deltas->load_ins, counter_deltas->store_ins,
        counter_deltas->flops, counter_deltas->fp_bits, 
        counter_deltas->ops, counter_deltas->op_bits);
    }

    if (use_bf_db || bfbinout) {
      bbid++;
    }

    num_merged = 0;
    prev_global_totals = global_totals;
    delete counter_deltas;
  }
}


// Associate the current counter values with a given function.
void bf_assoc_counters_with_func (const char* funcname)
{
  // Ensure that per_func_totals contains an ByteFlopCounters entry
  // for funcname, then add the current counters to that entry.
  if (bf_call_stack)
    funcname = bf_func_and_parents;
  else
    funcname = bf_string_to_symbol(funcname);
  counter_iterator sm_iter = per_func_totals().find(funcname);
  if (sm_iter == per_func_totals().end())
    // This is the first time we've seen this function name.
    per_func_totals()[funcname] =
      new ByteFlopCounters(bf_mem_insts_count,
                           bf_inst_mix_histo,
                           bf_terminator_count,
                           bf_mem_intrin_count,
                           bf_load_count,
                           bf_store_count,
                           bf_load_ins_count,
                           bf_store_ins_count,
                           bf_flop_count,
                           bf_fp_bits_count,
                           bf_op_count,
                           bf_op_bits_count);
  else {
    // Accumulate the current counter values into those associated
    // with an existing function name.
    ByteFlopCounters* func_counters = sm_iter->second;
    func_counters->accumulate(bf_mem_insts_count,
                              bf_inst_mix_histo,
                              bf_terminator_count,
                              bf_mem_intrin_count,
                              bf_load_count,
                              bf_store_count,
                              bf_load_ins_count,
                              bf_store_ins_count,
                              bf_flop_count,
                              bf_fp_bits_count,
                              bf_op_count,
                              bf_op_bits_count);
  }
}

int copyName(const char* source, char* dest) 
{
//  printf("Calling copyName\n");

  int srci = 0;
  int desti = 0;
  int len = 0;  

  if (source[0] == '\0') {
    //printf("first char null\n"); 
    dest[0] = '\0'; 
    return 0;
  }

  while (source[srci] == ' ') {
    srci++; 
    len++; 
  }

  for (;(source[srci] != ' ') && (source[srci] != '\0') && (desti < MAX_FUNCTION_NAMELEN); 
      srci++, desti++, len++)
  {
    dest[desti] = source[srci];
  }

  dest[desti] = '\0';

  if (source[srci] == '\0') {
    //printf("last func\n"); 
    return 0;
  }
  
  if (source[srci] == ' ') {
    //printf("not last func\n"); 
    return len+1;
  }

  // keep reading through name if longer than max
  if (desti == MAX_FUNCTION_NAMELEN) {
    for (;(source[srci] != ' ') && (source[srci] != '\0'); srci++, len++); 
  }

  if (source[srci] == '\0') 
    return 0;
  else return len+1;

}

// At the end of the program, report what we measured.
static class RunAtEndOfProgram {
private:

    
  string separator;    // Horizontal rule to output between sections
    
  // Compare two strings.
  static bool compare_char_stars (const char* one, const char* two) {
    return strcmp(one, two) < 0;
  }

  // Compare two function names, reporting which was called more
  // times.  Break ties by comparing function names.
  static bool compare_func_totals (const char* one, const char* two) {
    uint64_t one_calls = func_call_tallies()[one];
    uint64_t two_calls = func_call_tallies()[two];
    if (one_calls != two_calls)
      return one_calls > two_calls;
    else
      return compare_char_stars(one, two);
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

  void report_run_info (void) {

    if (bfbinout) {
      bf_table_t table = BF_RUNS;

      // write out the type of table entry this is
      bfbinout->write((char*)&table, sizeof(bf_table_t));

      // write out the data for this basicblocks table entry
      bf_runs_table bftable = {
        utc_sec, utc_usec, "", "", bf_run_number, "", ""};

      // copy over datetime string
      strncpy(bftable.datetime, datetime_str, MAX_DATETIME_LEN);

      // also need to copy over name, but haven't figured that out how we're getting that yet

      // copy over output id
      strncpy(bftable.output_id, bf_output_id.c_str(), MAX_OUTPUTID_LEN);

      // copy over bf command options
      strncpy(bftable.bf_options, bf_option_string, MAX_BFOPTIONS_LEN);

      // write out the record for runs
      bfbinout->write((char*)&bftable, sizeof(bf_runs_table));
    }

    string run_name = "";

    if (use_bf_db) {
      insert_runs(utc_sec, utc_usec, datetime_str, run_name, bf_run_number, bf_output_id, bf_option_string);
    }
  }

  // Report per-function counter totals.
  void report_by_function (void) {
    // Output a header line.
    if (bfout) {
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
    }

    // Output the data by sorted function name.
    uint64_t stackid = 0;
    vector<const char*>* all_func_names = per_func_totals().sorted_keys(compare_char_stars);
    for (vector<const char*>::iterator fn_iter = all_func_names->begin();
        fn_iter != all_func_names->end();
        fn_iter++) {
      const string funcname = *fn_iter;
      const char* funcname_c = bf_string_to_symbol(funcname.c_str());
      ByteFlopCounters* func_counters = per_func_totals()[funcname_c];
      if (bfout) {
        *bfout << bf_output_prefix
          << "BYFL_FUNC:        "
          << setw(HDR_COL_WIDTH) << func_counters->loads << ' '
          << setw(HDR_COL_WIDTH) << func_counters->stores << ' '
          << setw(HDR_COL_WIDTH) << func_counters->load_ins << ' '
          << setw(HDR_COL_WIDTH) << func_counters->store_ins << ' '
          << setw(HDR_COL_WIDTH) << func_counters->flops << ' '
          << setw(HDR_COL_WIDTH) << func_counters->fp_bits << ' '
          << setw(HDR_COL_WIDTH) << func_counters->ops << ' '
          << setw(HDR_COL_WIDTH) << func_counters->op_bits;
        if (bf_unique_bytes)
          *bfout << ' '
            << setw(HDR_COL_WIDTH)
            << (bf_mem_footprint ? bf_tally_unique_addresses_tb(funcname_c) : bf_tally_unique_addresses(funcname_c));
        *bfout << ' '
          << setw(HDR_COL_WIDTH) << func_counters->terminators[BF_END_BB_DYNAMIC] << ' '
          << setw(HDR_COL_WIDTH) << func_call_tallies()[funcname_c] << ' '
          << funcname_c << '\n';
      }

      if (bfbinout || use_bf_db) {

        // write out the data for this basicblocks table entry
        bf_functions_table bftable = {
          utc_sec, utc_usec, stackid, 
          func_counters->loads, func_counters->stores,
          func_counters->load_ins, func_counters->store_ins,
          func_counters->flops, func_counters->fp_bits,
          func_counters->ops, func_counters->op_bits,
          bf_mem_footprint ? 
            bf_tally_unique_addresses_tb(funcname_c) : 
            bf_tally_unique_addresses(funcname_c),
          func_counters->terminators[BF_END_BB_DYNAMIC],
          func_call_tallies()[funcname_c],
          "", "", "", "", "", "", "", "", "", "", "", ""};

        bool done = false;

        const char* funcnamep = funcname_c;
        int len = 0;
        len = copyName(funcnamep, bftable.Function);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func1);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func2);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func3);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func4);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func5);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func6);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func7);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func8);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func9);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func10);
        if (len > 0) {funcnamep += len;} else {done = true;}
        if (!done) len = copyName(funcnamep, bftable.Parent_func11);
        if (len > 0) {funcnamep += len;} else {done = true;}

        if (bfbinout) {        
          bf_table_t table = BF_FUNCTIONS;

          // write out the type of table entry this is
          bfbinout->write((char*)&table, sizeof(bf_table_t));
          bfbinout->write((char*)&bftable, sizeof(bf_functions_table));
        }

        if (use_bf_db) {
          insert_functions(bftable);
        }

        stackid++;
      }

    }

    delete all_func_names;

    // Output invocation tallies for all called functions, not just
    // instrumented functions.
    if (bfout) {
      *bfout << bf_output_prefix
        << "BYFL_CALLEE_HEADER: "
        << setw(13) << "Invocations" << ' '
        << "Byfl" << ' '
        << "Function\n";
    }
    vector<const char*> all_called_funcs;
    for (str2num_t::iterator sm_iter = func_call_tallies().begin();
        sm_iter != func_call_tallies().end();
        sm_iter++)
      all_called_funcs.push_back(sm_iter->first);
    sort(all_called_funcs.begin(), all_called_funcs.end(), compare_func_totals);
    for (vector<const char*>::iterator fn_iter = all_called_funcs.begin();
        fn_iter != all_called_funcs.end();
        fn_iter++) {
      const char* funcname = *fn_iter;   // Function name
      uint64_t tally = 0;                // Invocation count
      bool instrumented = true;          // Whether function was instrumented
      if (funcname[0] == '+') {
        const char* unique_name = bf_string_to_symbol(funcname+1);
        str2num_t::iterator tally_iter = func_call_tallies().find(unique_name);
        instrumented = (tally_iter != func_call_tallies().end());
        tally = func_call_tallies()[bf_string_to_symbol(funcname)];
        funcname = unique_name;
      }
      string funcname_orig = demangle_func_name(funcname);
      if (tally > 0) {
        if (bfout) {
          *bfout << bf_output_prefix
            << "BYFL_CALLEE: "
            << setw(HDR_COL_WIDTH) << tally << ' '
            << (instrumented ? "Yes " : "No  ") << ' '
            << funcname_orig;
          if (funcname_orig != funcname)
            *bfout << " [" << funcname << ']';
          *bfout << '\n';
        }

        if (bfbinout) {
          bf_table_t table = BF_CALLEE;

          // write out the type of table entry this is
          bfbinout->write((char*)&table, sizeof(bf_table_t));

          // write out the data for this callee table entry
          bf_callee_table bftable = {
            utc_sec, utc_usec, tally, instrumented? (short)1:(short)0, ""};

          // should I be copying funcname_orig? or maybe both funcname and funcname_orig?
          strncpy(bftable.Function, funcname, MAX_FUNCTION_NAMELEN);

          bfbinout->write((char*)&bftable, sizeof(bf_callee_table));
        }

        if (use_bf_db) {
          insert_callee(utc_sec, utc_usec, tally,  instrumented? (short)1:(short)0,
            funcname);
        }
      }
    }
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
    if (bfout) bfout->imbue(locale(""));

    // For convenience, assign names to each of our terminator tallies.
    const uint64_t term_static = counter_totals.terminators[BF_END_BB_STATIC];
    const uint64_t term_dynamic = counter_totals.terminators[BF_END_BB_DYNAMIC];
    const uint64_t term_any = counter_totals.terminators[BF_END_BB_ANY];

    // Compute the number of integer operations performed by
    // subtracting flops, memory ops, and branch ops from total ops.
    const uint64_t global_int_ops = counter_totals.ops - counter_totals.flops - global_mem_ops - term_any;

    // Report the raw measurements in terms of bytes and operations.
    if (bfout) {
      *bfout << tag << ": " << separator << '\n';
      *bfout << tag << ": " << setw(25) << global_bytes << " bytes ("
        << counter_totals.loads << " loaded + "
        << counter_totals.stores << " stored)\n";
      if (bf_unique_bytes && !partition)
        *bfout << tag << ": " << setw(25) << global_unique_bytes << " unique bytes\n";
      *bfout << tag << ": " << setw(25) << counter_totals.flops << " flops\n";
      *bfout << tag << ": " << setw(25) << global_int_ops << " integer ops\n";
      *bfout << tag << ": " << setw(25) << global_mem_ops << " memory ops ("
        << counter_totals.load_ins << " loads + "
        << counter_totals.store_ins << " stores)\n";
      *bfout << tag << ": " << setw(25) << term_any << " branch ops ("
        << term_static << " unconditional and direct + "
        << term_dynamic << " conditional or indirect + "
        << term_any - term_static - term_dynamic << " other)\n";
      *bfout << tag << ": " << setw(25) << counter_totals.ops << " TOTAL OPS\n";
    }

    // Output reuse distance if measured.
    if (reuse_unique > 0) {
      uint64_t median_value;
      uint64_t mad_value;
      bf_get_median_reuse_distance(&median_value, &mad_value);
      if (bfout) {
        *bfout << tag << ": " << setw(25);
        if (median_value == ~(uint64_t)0)
          *bfout << "infinite" << " median reuse distance\n";
        else
          *bfout << median_value << " median reuse distance (+/- "
            << mad_value << ")\n";
      }
    }
    if (bfout) *bfout << tag << ": " << separator << '\n';

    // Output raw, per-type information.
    if (bf_types) {
      // The following need to be consistent with byfl-common.h.
      const char *memop2name[] = {"loads of ", "stores of "};
      const char *memref2name[] = {"", "pointers to "};
      const char *memagg2name[] = {"", "vectors of "};
      const char *memwidth2name[] = {"8-bit ", "16-bit ", "32-bit ",
        "64-bit ", "128-bit ", "oddly sized "};
      const char *memtype2name[] = {"integers", "floating-point values",
        "\"other\" values (not integers or FP values)"};

      int lsid = 0; // unique load/store id
      bf_table_t table = BF_LOADSTORES;

      // Output all nonzero entries.
      for (int memop = 0; memop < BF_OP_NUM; memop++)
        for (int memref = 0; memref < BF_REF_NUM; memref++)
          for (int memagg = 0; memagg < BF_AGG_NUM; memagg++)
            for (int memwidth = 0; memwidth < BF_WIDTH_NUM; memwidth++)
              for (int memtype = 0; memtype < BF_TYPE_NUM; memtype++) {
                uint64_t idx = mem_type_to_index(memop, memref, memagg, memtype, memwidth);
                uint64_t tally = counter_totals.mem_insts[idx];
                if (tally > 0) {
                  if (bfout) {
                    *bfout << tag << ": " << setw(25) << tally << ' '
                      << memop2name[memop]
                      << memref2name[memref]
                      << memagg2name[memagg]
                      << memwidth2name[memwidth]
                      << memtype2name[memtype]
                      << '\n';
                  }

                  if (bfbinout) { 
                    // write out the type of table entry this is
                    bfbinout->write((char*)&table, sizeof(bf_table_t));

                    // write out the data for this table entry
                    bf_loadstores_table bftable = {utc_sec, utc_usec, lsid, tally, (short)memop, 
                      (short)memref, (short)memagg, (short)memwidth, (short)memtype};
                    bfbinout->write((char*)&bftable, sizeof(bf_loadstores_table));
                  }

                  if (use_bf_db) {
                    insert_loadstores(utc_sec, utc_usec, lsid, tally, (short)memop,
                        (short)memref, (short)memagg, (short)memwidth, (short)memtype);
                  }

                  if (bfbinout || use_bf_db) {
                    lsid++;
                  }
                }
              }
      if (bfout) *bfout << tag << ": " << separator << '\n';
    }
        
    // Report the raw measurements in terms of bits and bit operations.
    if (bfout) {
      *bfout << tag << ": " << setw(25) << global_bytes*8 << " bits ("
        << counter_totals.loads*8 << " loaded + "
        << counter_totals.stores*8 << " stored)\n";
      if (bf_unique_bytes && !partition)
        *bfout << tag << ": " << setw(25) << global_unique_bytes*8 << " unique bits\n";
      *bfout << tag << ": " << setw(25) << counter_totals.fp_bits << " flop bits\n";
      *bfout << tag << ": " << setw(25) << counter_totals.op_bits << " op bits (excluding memory ops)\n";
      *bfout << tag << ": " << separator << '\n';

      // Report the amount of memory that passed through
      // llvm.mem{set,cpy,move}.*.
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
    }

    // Report vector-operation measurements.
    uint64_t num_vec_ops=0, total_vec_elts, total_vec_bits;
    if (bf_vectors) {
      if (partition)
        bf_get_vector_statistics(partition, &num_vec_ops, &total_vec_elts, &total_vec_bits);
      else
        bf_get_vector_statistics(&num_vec_ops, &total_vec_elts, &total_vec_bits);
      if (bfout) {
        *bfout << tag << ": " << setw(25) << num_vec_ops << " vector operations (FP & int)\n";
        if (num_vec_ops > 0)
          *bfout << tag << ": " << fixed << setw(25) << setprecision(4)
            << (double)total_vec_elts / (double)num_vec_ops
            << " elements per vector\n"
            << tag << ": " << fixed << setw(25) << setprecision(4)
            << (double)total_vec_bits / (double)num_vec_ops
            << " bits per element\n";
        *bfout << tag << ": " << separator << '\n';
      }
    }

    // Pretty-print the histogram of instructions executed.
    uint64_t total_insts = 0;
    if (bf_tally_inst_mix) {
      // Sort the histogram by decreasing opcode tally.
      vector<name_tally> sorted_inst_mix;
      size_t maxopnamelen = 0;
      for (uint64_t i = 0; i < NUM_OPCODES; i++)
        if (counter_totals.inst_mix_histo[i] != 0) {
          sorted_inst_mix.push_back(name_tally(opcode2name[i],
                                               counter_totals.inst_mix_histo[i]));
          size_t opnamelen = strlen(opcode2name[i]);
          if (opnamelen > maxopnamelen)
            maxopnamelen = opnamelen;
        }
      sort(sorted_inst_mix.begin(), sorted_inst_mix.end(), compare_name_tallies);

      // Output the sorted results.
      for (vector<name_tally>::iterator ntiter = sorted_inst_mix.begin();
           ntiter != sorted_inst_mix.end();
           ntiter++) {
        total_insts += ntiter->second;
        if (bfout) {
          *bfout << tag << ": " << setw(25) << ntiter->second << ' '
            << setw(maxopnamelen) << left
            << ntiter->first << " instructions executed\n"
            << right;
        }

        if (bfbinout) { 
          bf_table_t table = BF_INSTMIX;

          // write out the type of table entry this is
          bfbinout->write((char*)&table, sizeof(bf_table_t));

          // write out the data for this instmix table entry
          bf_instmix_table bftable = {utc_sec, utc_usec, {}, ntiter->second}; 
          strncpy(bftable.inst_type, ntiter->first, MAX_OPCODE_NAMELEN);

          bfbinout->write((char*)&bftable, sizeof(bf_instmix_table));
        }

        if (use_bf_db) {
          insert_instmix(utc_sec, utc_usec, ntiter->first, ntiter->second);
        }

      }

      if (bfout) {
        *bfout << tag << ": " << setw(25) << total_insts << ' '
          << setw(maxopnamelen) << left
          << "TOTAL" << " instructions executed\n"
          << right
          << tag << ": " << separator << '\n';
      }
    }


    // Output quantiles of working-set sizes.
    if (bf_mem_footprint) {
      // Produce a histogram that tallies each byte-access count.
      vector<bf_addr_tally_t> access_counts;
      uint64_t total_bytes = 0;
      bf_get_address_tally_hist (access_counts, &total_bytes);

      // Output every nth quantile.
      const double pct_change = 0.05;   // Minimum percentage-point change to output
      uint64_t running_total_bytes = 0;     // Running total of tally (# of addresses)
      uint64_t running_total_accesses = 0;  // Running total of byte-access count times tally.
      double hit_rate = 0.0;            // Quotient of the preceding two values
      for (vector<bf_addr_tally_t>::iterator counts_iter = access_counts.begin();
           counts_iter != access_counts.end();
           counts_iter++) {
        running_total_bytes += counts_iter->second;
        running_total_accesses += uint64_t(counts_iter->first) * uint64_t(counts_iter->second);
        double new_hit_rate = double(running_total_accesses) / double(global_bytes);
        if (new_hit_rate - hit_rate > pct_change || running_total_accesses == global_bytes) {
          hit_rate = new_hit_rate;
          *bfout << tag << ": "
                 << setw(25) << running_total_bytes << " bytes cover "
                 << fixed << setw(5) << setprecision(1) << hit_rate*100.0 << "% of memory accesses\n";
        }
      }
      *bfout << tag << ": " << separator << '\n';
    }

    // Calculate derived measurements

    derived_measurements dm {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1 }; 

    // Report a bunch of derived measurements.
    if (counter_totals.stores > 0) {
      dm.bytes_loaded_per_byte_stored =  (double)counter_totals.loads / (double)counter_totals.stores;
    }

    if (counter_totals.load_ins > 0) {
      dm.ops_per_load_instr = (double)counter_totals.ops / (double)counter_totals.load_ins;
    }

    if (global_mem_ops > 0) {
      dm.bits_loaded_stored_per_memory_op = (double)global_bytes*8 / (double)global_mem_ops;
    }

    if (term_dynamic > 0) {
      if (counter_totals.flops > 0) {
        dm.flops_per_conditional_indirect_branch = (double)counter_totals.flops / (double)term_dynamic;
      }

      if (counter_totals.ops > 0) {
        dm.ops_per_conditional_indirect_branch = (double)counter_totals.ops / (double)term_dynamic;
      }

      if (num_vec_ops > 0) {
        dm.vector_ops_per_conditional_indirect_branch = (double)num_vec_ops / (double)term_dynamic;
      }
    }

    if (num_vec_ops > 0) {
      if (counter_totals.flops > 0) {
        dm.vector_ops_per_flop = (double)num_vec_ops / (double)counter_totals.flops;
      }

      if (counter_totals.ops > 0) {
        dm.vector_ops_per_op = (double)num_vec_ops / (double)counter_totals.ops;
      }
    }

    if (total_insts > 0) {
      dm.ops_per_instruction = (double)counter_totals.ops / (double)total_insts;
    }

    if (counter_totals.flops > 0) {
      dm.bytes_per_flop = (double)global_bytes / (double)counter_totals.flops;
      dm.bits_per_flop_bit = (double)global_bytes*8.0 / (double)counter_totals.fp_bits;
    }

    if (counter_totals.ops > 0) {
      dm.bytes_per_op = (double)global_bytes / (double)counter_totals.ops;
      dm.bits_per_nonmemory_op_bit = (double)global_bytes*8.0 / (double)counter_totals.op_bits;
    }

    if (bf_unique_bytes && (counter_totals.flops > 0 || counter_totals.ops > 0)) {
      if (counter_totals.flops > 0) {
        dm.unique_bytes_per_flop = (double)global_unique_bytes / (double)counter_totals.flops;
        dm.unique_bits_per_flop_bit = (double)global_unique_bytes*8.0 / (double)counter_totals.fp_bits;
      }

      if (counter_totals.ops > 0) {
        dm.unique_bytes_per_op = (double)global_unique_bytes / (double)counter_totals.ops;
        dm.unique_bits_per_nonmemory_op_bit = (double)global_unique_bytes*8.0 / (double)counter_totals.op_bits;
      }
    }

    if (bf_unique_bytes && !partition) {
      dm.bytes_per_unique_byte = (double)global_bytes / (double)global_unique_bytes;
    }
    
    // Report a bunch of derived measurements.

    if (bfout) {

      REPORT_DERIVED(bfout, tag, dm.bytes_loaded_per_byte_stored, " bytes loaded per byte stored\n");
      REPORT_DERIVED(bfout, tag, dm.ops_per_load_instr, " ops per load instruction\n");
      REPORT_DERIVED(bfout, tag, dm.bits_loaded_stored_per_memory_op, " bits loaded/stored per memory op\n");
      REPORT_DERIVED(bfout, tag, dm.flops_per_conditional_indirect_branch, " flops per conditional/indirect branch\n");
      REPORT_DERIVED(bfout, tag, dm.ops_per_conditional_indirect_branch, " ops per conditional/indirect branch\n");
      REPORT_DERIVED(bfout, tag, dm.vector_ops_per_conditional_indirect_branch, " vector ops (FP & int) per conditional/indirect branch\n");
      REPORT_DERIVED(bfout, tag, dm.vector_ops_per_flop, " vector ops (FP & int) per flop\n");
      REPORT_DERIVED(bfout, tag, dm.vector_ops_per_op, " vector ops (FP & int) per op\n");
      REPORT_DERIVED(bfout, tag, dm.ops_per_instruction, " ops per instruction\n");
      *bfout << tag << ": " << separator << '\n';
      REPORT_DERIVED(bfout, tag, dm.bytes_per_flop, " bytes per flop\n");
      REPORT_DERIVED(bfout, tag, dm.bits_per_flop_bit, " bits per flop bit\n");
      REPORT_DERIVED(bfout, tag, dm.bytes_per_op, " bytes per op\n");
      REPORT_DERIVED(bfout, tag, dm.bits_per_nonmemory_op_bit, " bits per (nonmemory) op bit\n");
      *bfout << tag << ": " << separator << '\n';
      REPORT_DERIVED(bfout, tag, dm.unique_bytes_per_flop, " unique bytes per flop\n");
      REPORT_DERIVED(bfout, tag, dm.unique_bits_per_flop_bit, " unique bits per flop bit\n");
      REPORT_DERIVED(bfout, tag, dm.unique_bytes_per_op, " unique bytes per op\n");
      REPORT_DERIVED(bfout, tag, dm.unique_bits_per_nonmemory_op_bit, " unique bits per (nonmemory) op bit\n");
      REPORT_DERIVED(bfout, tag, dm.bytes_per_unique_byte, " bytes per unique byte\n");
      if (!partition) *bfout << tag << ": " << separator << '\n';
    }

    if (bfbinout) {
      binout_derived(utc_sec, utc_usec, bfbinout, dm);
    }

    if (use_bf_db) {
      insert_derived(utc_sec, utc_usec, dm);
    } 

  }

public:
  RunAtEndOfProgram() {
    separator = "-----------------------------------------------------------------";
  }

  ~RunAtEndOfProgram() {
    // Do nothing if our output is suppressed.
    bf_initialize_if_necessary();
    if (suppress_output())
      return;
    
    report_run_info();

    // Report per-function counter totals.
    if (bf_per_func)
      report_by_function();

    // Output a histogram of vector usage.
    if (bf_vectors)
      bf_report_vector_operations(call_stack->max_depth);

    // If we're not instrumented on the basic-block level, then we
    // need to accumulate the current values of all of our counters
    // into the global totals.
    if (!bf_every_bb)
      global_totals.accumulate(bf_mem_insts_count,
                               bf_inst_mix_histo,
                               bf_terminator_count,
                               bf_mem_intrin_count,
                               bf_load_count,
                               bf_store_count,
                               bf_load_ins_count,
                               bf_store_ins_count,
                               bf_flop_count,
                               bf_fp_bits_count,
                               bf_op_count,
                               bf_op_bits_count);

    // If the global counter totals are empty, this means that we were
    // tallying per-function data and resetting the global counts
    // after each tally.  We therefore reconstruct the lost global
    // counts from the per-function tallies.
    if (global_totals.terminators[BF_END_BB_ANY] == 0)
      for (counter_iterator sm_iter = per_func_totals().begin();
           sm_iter != per_func_totals().end();
           sm_iter++)
        global_totals.accumulate(sm_iter->second);

    // Report user-defined counter totals, if any.
    vector<const char*>* all_tag_names = user_defined_totals().sorted_keys(compare_char_stars);
    for (vector<const char*>::const_iterator tag_iter = all_tag_names->begin();
         tag_iter != all_tag_names->end();
         tag_iter++) {
      const char* tag_name = *tag_iter;
      report_totals(tag_name, *user_defined_totals()[tag_name]);
    }

    // Report the global counter totals across all basic blocks.
    report_totals(NULL, global_totals);
    if (bfout) bfout->flush();
    if (bfbinout) bfbinout->close();
    if (use_bf_db) close_database();
  }
} run_at_end_of_program;

} // namespace bytesflops

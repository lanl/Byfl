/*
 * Helper library for computing bytes:flops ratios
 * (tracking basic blocks)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 */

#include "byfl.h"
#include "byfl-common.h"
#ifdef USE_BFD
# include "findsrc.h"
#endif

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

// Define the different ways a basic block can terminate.
typedef enum {
  BB_NOT_END=0,       // Basic block has not actually terminated.
  BB_END_UNCOND=1,    // Basic block terminated with an unconditional branch.
  BB_END_COND=2       // Basic block terminated with a conditional branch.
} bb_end_t;

// The following values get reset at the end of every basic block.
uint64_t  bf_load_count       = 0;    // Tally of the number of bytes loaded
uint64_t  bf_store_count      = 0;    // Tally of the number of bytes stored
uint64_t* bf_mem_insts_count  = NULL; // Tally of memory instructions by type
uint64_t* bf_inst_mix_histo   = NULL; // Tally of instruction mix (as histogram)
uint64_t* bf_terminator_count = NULL; // Tally of terminators by type
uint64_t* bf_mem_intrin_count = NULL; // Tally of memory intrinsic calls and data movement
uint64_t  bf_load_ins_count   = 0;    // Tally of the number of load instructions performed
uint64_t  bf_store_ins_count  = 0;    // Tally of the number of store instructions performed
uint64_t  bf_call_ins_count   = 0;    // Tally of the number of function-call instructions (non-exception-throwing) performed
uint64_t  bf_flop_count       = 0;    // Tally of the number of FP operations performed
uint64_t  bf_fp_bits_count    = 0;    // Tally of the number of bits used by all FP operations
uint64_t  bf_op_count         = 0;    // Tally of the number of operations performed
uint64_t  bf_op_bits_count    = 0;    // Tally of the number of bits used by all operations except loads/stores

namespace bytesflops {

// The following values represent more persistent counter and other state.
ByteFlopCounters global_totals;  // Global tallies of all of our counters
static ByteFlopCounters prev_global_totals;  // Previously reported global tallies of all of our counters
static uint64_t num_merged = 0;    // Number of basic blocks merged so far
static uint64_t first_bb = 0;      // First basic block in a merged set
static ByteFlopCounters bb_totals; // Tallies of all of our counters across <= num_merged basic blocks

extern ostream* bfout;
extern BinaryOStream* bfbin;
#ifdef USE_BFD
extern ProcessSymbolTable* procsymtab;  // The calling process's symbol table
#endif

// Initialize some of our variables at first use.
void initialize_bblocks (void)
{
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
}

// Initialize all of the basic-block counters.
ByteFlopCounters::ByteFlopCounters (uint64_t* initial_mem_insts,
                                    uint64_t* initial_inst_mix_histo,
                                    uint64_t* initial_terminators,
                                    uint64_t* initial_mem_intrinsics,
                                    uint64_t initial_loads,
                                    uint64_t initial_stores,
                                    uint64_t initial_load_ins,
                                    uint64_t initial_store_ins,
                                    uint64_t initial_call_ins,
                                    uint64_t initial_flops,
                                    uint64_t initial_fp_bits,
                                    uint64_t initial_ops,
                                    uint64_t initial_op_bits)
{
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
      for (size_t i = 0; i < NUM_OPCODES; i++)
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
  loads     = initial_loads;
  stores    = initial_stores;
  load_ins  = initial_load_ins;
  store_ins = initial_store_ins;
  call_ins  = initial_call_ins;
  flops     = initial_flops;
  fp_bits   = initial_fp_bits;
  ops       = initial_ops;
  op_bits   = initial_op_bits;
}

// Assign new values into a basic block's counters.
void ByteFlopCounters::assign (uint64_t* new_mem_insts,
                               uint64_t* new_inst_mix_histo,
                               uint64_t* new_terminators,
                               uint64_t* new_mem_intrinsics,
                               uint64_t new_loads,
                               uint64_t new_stores,
                               uint64_t new_load_ins,
                               uint64_t new_store_ins,
                               uint64_t new_call_ins,
                               uint64_t new_flops,
                               uint64_t new_fp_bits,
                               uint64_t new_ops,
                               uint64_t new_op_bits)
{
  // Assign mem_insts only if -bf-types was specified.
  if (bf_types)
    for (size_t i = 0; i < NUM_MEM_INSTS; i++)
      mem_insts[i] = new_mem_insts[i];

  // Assign inst_mix_histo only if -bf-inst-mix was specified.
  if (bf_tally_inst_mix)
    for (size_t i = 0; i < NUM_OPCODES; i++)
      inst_mix_histo[i] = new_inst_mix_histo[i];

  // Unconditionally assign everything else.
  for (size_t i = 0; i < BF_END_BB_NUM; i++)
    terminators[i] = new_terminators[i];
  for (size_t i = 0; i < BF_NUM_MEM_INTRIN; i++)
    mem_intrinsics[i] = new_mem_intrinsics[i];
  loads     = new_loads;
  stores    = new_stores;
  load_ins  = new_load_ins;
  store_ins = new_store_ins;
  call_ins  = new_call_ins;
  flops     = new_flops;
  fp_bits   = new_fp_bits;
  ops       = new_ops;
  op_bits   = new_op_bits;
}

// Accumulate new values into a basic block's counters.
void ByteFlopCounters::accumulate (uint64_t* more_mem_insts,
                                   uint64_t* more_inst_mix_histo,
                                   uint64_t* more_terminators,
                                   uint64_t* more_mem_intrinsics,
                                   uint64_t more_loads,
                                   uint64_t more_stores,
                                   uint64_t more_load_ins,
                                   uint64_t more_store_ins,
                                   uint64_t more_call_ins,
                                   uint64_t more_flops,
                                   uint64_t more_fp_bits,
                                   uint64_t more_ops,
                                   uint64_t more_op_bits)
{
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
  call_ins  += more_call_ins;
  flops     += more_flops;
  fp_bits   += more_fp_bits;
  ops       += more_ops;
  op_bits   += more_op_bits;
}

// Accumulate another counter's values into a basic block's counters.
void ByteFlopCounters::accumulate (ByteFlopCounters* other)
{
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
  call_ins  += other->call_ins;
  flops     += other->flops;
  fp_bits   += other->fp_bits;
  ops       += other->ops;
  op_bits   += other->op_bits;
}

// Return the difference of one basic block's counters and another's.
ByteFlopCounters* ByteFlopCounters::difference (ByteFlopCounters* other,
                                                ByteFlopCounters* target)
{
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
  if (target == nullptr)
    return new ByteFlopCounters(delta_mem_insts,
                                delta_inst_mix_histo,
                                delta_terminators,
                                delta_mem_intrinsics,
                                loads - other->loads,
                                stores - other->stores,
                                load_ins - other->load_ins,
                                store_ins - other->store_ins,
                                call_ins = other->call_ins,
                                flops - other->flops,
                                fp_bits - other->fp_bits,
                                ops - other->ops,
                                op_bits - other->op_bits);
  else {
    target->assign(delta_mem_insts,
                   delta_inst_mix_histo,
                   delta_terminators,
                   delta_mem_intrinsics,
                   loads - other->loads,
                   stores - other->stores,
                   load_ins - other->load_ins,
                   store_ins - other->store_ins,
                   call_ins - other->call_ins,
                   flops - other->flops,
                   fp_bits - other->fp_bits,
                   ops - other->ops,
                   op_bits - other->op_bits);
    return target;
  }
}

// Reset all of a basic block's counters to zero.
void ByteFlopCounters::reset (void)
{
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
  call_ins  = 0;
  flops     = 0;
  fp_bits   = 0;
  ops       = 0;
  op_bits   = 0;
}

// At the end of a basic block, accumulate the current counter
// variables (bf_*_count) into the current basic block's counters and
// into the global counters.
extern "C"
void bf_accumulate_bb_tallies (void)
{
  // Add the current values to the per-BB totals.
  bb_totals.accumulate(bf_mem_insts_count,
                       bf_inst_mix_histo,
                       bf_terminator_count,
                       bf_mem_intrin_count,
                       bf_load_count,
                       bf_store_count,
                       bf_load_ins_count,
                       bf_store_ins_count,
                       bf_call_ins_count,
                       bf_flop_count,
                       bf_fp_bits_count,
                       bf_op_count,
                       bf_op_bits_count);
  global_totals.accumulate(&bb_totals);
  const char* partition = bf_string_to_symbol(bf_categorize_counters());
  if (partition != NULL) {
    auto sm_iter = user_defined_totals().find(partition);
    if (sm_iter == user_defined_totals().end())
      user_defined_totals()[partition] = new ByteFlopCounters(bb_totals);
    else
      user_defined_totals()[partition]->accumulate(&bb_totals);
  }
}

// Reset the current basic block's tallies rather than requiring a
// push and a pop for every basic block.
extern "C"
void bf_reset_bb_tallies (void)
{
  bb_totals.reset();
}

// Report what we've measured for the current basic block.
static void report_bb_tallies (uint64_t bb_merge)
{
  static bool showed_header = false;         // true=already output our header

  // Do nothing if our output is suppressed.
  if (suppress_output())
    return;

  // If this is our first invocation, begin a basic-block table.  We
  // output only in binary format to avoid flooding the standard
  // output device.
  if (__builtin_expect(!showed_header, 0)) {
    *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Basic blocks";
    if (bb_merge == 1) {
      // Log every basic block individually.
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Basic block number"
             << uint8_t(BINOUT_COL_STRING) << "Tag";
#ifdef HAVE_BACKTRACE
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Address";
# ifdef USE_BFD
      *bfbin << uint8_t(BINOUT_COL_STRING) << "File name"
             << uint8_t(BINOUT_COL_STRING) << "Function name"
             << uint8_t(BINOUT_COL_UINT64) << "Line number"
#  if HAVE_DECL_BFD_FIND_NEAREST_LINE_DISCRIMINATOR
             << uint8_t(BINOUT_COL_UINT64) << "Line-number discriminator";
#  else
             ;
#  endif
# else
      *bfbin << uint8_t(BINOUT_COL_STRING) << "Symbolic location";
# endif
#endif
    }
    else
      // Log groups of basic blocks.
      *bfbin << uint8_t(BINOUT_COL_UINT64) << "Beginning basic block number"
             << uint8_t(BINOUT_COL_UINT64) << "Ending basic block number";
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
           << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded and stored by memcpy and memmove"
           << uint8_t(BINOUT_COL_NONE);
    showed_header = true;
  }

  // If we've accumulated enough basic blocks, output the aggregate of
  // their values.
  if (__builtin_expect(++num_merged >= bb_merge, 0)) {
    // Output -- only to the binary output file, not the standard
    // output device -- the difference between the current counter
    // values and our previously saved values.
    static ByteFlopCounters counter_deltas;
    (void) global_totals.difference(&prev_global_totals, &counter_deltas);
    *bfbin << uint8_t(BINOUT_ROW_DATA);
    *bfbin << first_bb;
    if (bb_merge != 1)
      *bfbin << first_bb + num_merged - 1;
    first_bb += num_merged;
    if (bb_merge == 1) {
      const char* partition = bf_categorize_counters();
      *bfbin << (partition == NULL ? "" : partition);
#ifdef HAVE_BACKTRACE
      void* caller_addr = bf_find_caller_address();
      *bfbin << (uint64_t) (uintptr_t) caller_addr;
# ifdef USE_BFD
      SourceCodeLocation* srcloc = procsymtab->find_address((uintptr_t)caller_addr);
      if (srcloc == nullptr)
        *bfbin << "" << "" << UINT64_C(0) << UINT64_C(0);
      else
        *bfbin << srcloc->file_name
               << srcloc->function_name
               << srcloc->line_number
#  if HAVE_DECL_BFD_FIND_NEAREST_LINE_DISCRIMINATOR
               << srcloc->discriminator;
#  else
               ;
#  endif
# else
      *bfbin << bf_address_to_location_string(caller_addr);
# endif
#endif
    }
    uint64_t other_branches = counter_deltas.terminators[BF_END_BB_ANY];
    for (int i = 0; i < BF_END_BB_NUM; i++)
      if (i != BF_END_BB_ANY)
        other_branches -= counter_deltas.terminators[i];
    *bfbin << counter_deltas.load_ins
           << counter_deltas.store_ins
           << counter_deltas.flops
           << counter_deltas.ops - counter_deltas.flops - counter_deltas.load_ins - counter_deltas.store_ins - counter_deltas.terminators[BF_END_BB_ANY]
           << counter_deltas.call_ins
           << counter_deltas.terminators[BF_END_BB_INVOKE]
           << counter_deltas.terminators[BF_END_BB_UNCOND_FAKE]
           << counter_deltas.terminators[BF_END_BB_UNCOND_REAL]
           << counter_deltas.terminators[BF_END_BB_COND_NT]
           << counter_deltas.terminators[BF_END_BB_COND_T]
           << counter_deltas.terminators[BF_END_BB_INDIRECT]
           << counter_deltas.terminators[BF_END_BB_SWITCH]
           << counter_deltas.terminators[BF_END_BB_RETURN]
           << other_branches
           << counter_deltas.fp_bits
           << counter_deltas.op_bits
           << counter_deltas.loads
           << counter_deltas.stores
           << counter_deltas.mem_insts[BF_MEMSET_CALLS]
           << counter_deltas.mem_insts[BF_MEMSET_BYTES]
           << counter_deltas.mem_insts[BF_MEMXFER_CALLS]
           << counter_deltas.mem_insts[BF_MEMXFER_BYTES];

    // Prepare for the next round of output.
    num_merged = 0;
    prev_global_totals = global_totals;
  }
}

// Report what we've measured for the current basic block by calling the
// internal report_bb_tallies() function with its default argument.
extern "C"
void bf_report_bb_tallies (void)
{
  report_bb_tallies(bf_bb_merge);
}

// Associate the current counter values with a given function.
extern "C"
void bf_assoc_counters_with_func (KeyType_t funcID)
{
  // Ensure that per_func_totals contains an ByteFlopCounters entry
  // for funcname, then add the current counters to that entry.
  key2bfc_t::iterator sm_iter;
  KeyType_t key;
  if ( bf_call_stack )
  {
      sm_iter = per_func_totals().find(bf_func_and_parents_id);
      key = bf_func_and_parents_id;
  }
  else
  {
      sm_iter = per_func_totals().find(funcID);
      key = funcID;
  }

  if (sm_iter == per_func_totals().end())
    // This is the first time we've seen this function name.
    per_func_totals()[key] =
      new ByteFlopCounters(bf_mem_insts_count,
                           bf_inst_mix_histo,
                           bf_terminator_count,
                           bf_mem_intrin_count,
                           bf_load_count,
                           bf_store_count,
                           bf_load_ins_count,
                           bf_store_ins_count,
                           bf_call_ins_count,
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
                              bf_call_ins_count,
                              bf_flop_count,
                              bf_fp_bits_count,
                              bf_op_count,
                              bf_op_bits_count);
  }
}

// Finalize the basic-block tallies at the end of the run.
void finalize_bblocks (void)
{
  if (bf_every_bb) {
    // Complete the basic-block table.
    if (num_merged > 0)
      // Flush the last set of basic blocks.
      report_bb_tallies(0);
    *bfbin << uint8_t(BINOUT_ROW_NONE);
  }
  else {
    // If we're not instrumented on the basic-block level, then we need to
    // accumulate the current values of all of our counters into the global
    // totals.
    if (!bf_every_bb)
      global_totals.accumulate(bf_mem_insts_count,
                               bf_inst_mix_histo,
                               bf_terminator_count,
                               bf_mem_intrin_count,
                               bf_load_count,
                               bf_store_count,
                               bf_load_ins_count,
                               bf_store_ins_count,
                               bf_call_ins_count,
                               bf_flop_count,
                               bf_fp_bits_count,
                               bf_op_count,
                               bf_op_bits_count);

    // If the global counter totals are empty, this means that we were tallying
    // per-function data and resetting the global counts after each tally.  We
    // therefore reconstruct the lost global counts from the per-function
    // tallies.
    if (global_totals.terminators[BF_END_BB_ANY] == 0)
      for (auto sm_iter = per_func_totals().begin();
           sm_iter != per_func_totals().end();
           sm_iter++)
        global_totals.accumulate(sm_iter->second);
  }
}

} // namespace bytesflops

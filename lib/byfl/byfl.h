/*
 * Common definitions across the helper library
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BYFL_H_
#define _BYFL_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <wordexp.h>

#include "byfl-common.h"
#include "cachemap.h"
#include "bitpagetable.h"
#include "binaryoutput.h"

// The following constants are defined by the instrumented code.
extern const char * bf_foofoo;
extern uint64_t * testkey;
extern uint64_t * bf_keys;
extern uint64_t bf_bb_merge;         // Number of basic blocks to merge to compress the output
extern uint8_t  bf_call_stack;       // 1=maintain a function call stack
extern uint8_t  bf_every_bb;         // 1=tally and output per-basic-block data
extern uint64_t bf_max_reuse_distance;  // Maximum reuse distance to consider */
extern const char* bf_option_string; // -bf-* command-line options
extern uint8_t  bf_per_func;         // 1=tally and output per-function data
extern uint8_t  bf_mem_footprint;    // 1=keep track of how many times each byte of memory is accessed
extern uint8_t  bf_tally_inst_mix;   // 1=maintain instruction-mix histogram
extern uint8_t  bf_tally_inst_deps;  // 1=maintain instruction-dependency histogram
extern uint8_t  bf_types;            // 1=count loads/stores per type
extern uint8_t  bf_unique_bytes;     // 1=tally and output unique bytes
extern uint8_t  bf_vectors;          // 1=bin then output vector characteristics
extern uint8_t  bf_cache_model;      // 1=use the simple cache model
extern uint8_t  bf_data_structs;     // 1=tally and output counters by data structure
extern uint8_t  bf_strides;          // 1=tally and output information about access strides
extern uint64_t bf_line_size;        // cache line size in bytes
extern uint64_t bf_max_set_bits;     // log base 2 of max number of sets to model

// The following globals are defined by the instrumented code.
extern uint64_t bf_fmap_cnt;

// The following function is expected to be overridden by user code.
extern "C" {
  extern const char* bf_categorize_counters (void);   // Return a category in which to partition data.
}

namespace bytesflops {
  // Define a datatype for counting bytes.
  typedef uint32_t bytecount_t;
  const bytecount_t bf_max_bytecount = ~(bytecount_t)(0);  // Clamp to this value
  typedef pair<bytecount_t, uint64_t> bf_addr_tally_t;  // Number of times a count was seen ({count, multiplier})

  // The following library functions are used in files other than the
  // one in which they're defined.
  extern void bf_get_address_tally_hist (vector<bf_addr_tally_t>& histogram, uint64_t* total);
  extern void bf_get_median_reuse_distance(uint64_t* median_value, uint64_t* mad_value);
  extern void bf_get_reuse_distance(vector<uint64_t>** hist, uint64_t* unique_addrs);
  extern void bf_get_vector_statistics(const char* tag, uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
  extern void bf_get_vector_statistics(uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
  extern void bf_abend(void) __attribute__ ((noreturn));
  extern void bf_report_vector_operations(size_t call_stack_depth);
  extern void bf_report_data_struct_counts(void);
  extern void bf_report_bb_execution(void);
  extern void bf_report_strides_by_call_point(void);
  extern uint64_t bf_tally_unique_addresses(const char* funcname);
  extern uint64_t bf_tally_unique_addresses_tb(const char* funcname);
  extern uint64_t bf_tally_unique_addresses_tb(void);
  extern uint64_t bf_tally_unique_addresses(void);
  extern "C" const char* bf_string_to_symbol(const char *nonunique);
  extern void initialize_byfl(void);
  extern void initialize_bblocks(void);
  extern void initialize_reuse(void);
  extern void initialize_symtable(void);
  extern void initialize_tallybytes(void);
  extern void initialize_threading(void);
  extern void initialize_ubytes(void);
  extern void initialize_vectors(void);
  extern void initialize_data_structures(void);
  extern void initialize_strides(void);
  extern void initialize_cache(void);
  extern void finalize_bblocks(void);
  extern uint64_t bf_get_private_cache_accesses(void);
  extern vector<unordered_map<uint64_t,uint64_t> > bf_get_private_cache_hits(void);
  extern uint64_t bf_get_private_cold_misses(void);
  extern uint64_t bf_get_private_misaligned_mem_ops(void);
  extern uint64_t bf_get_shared_cache_accesses(void);
  extern vector<unordered_map<uint64_t,uint64_t> > bf_get_shared_cache_hits(void);
  extern uint64_t bf_get_shared_cold_misses(void);
  extern uint64_t bf_get_shared_misaligned_mem_ops(void);
  extern vector<unordered_map<uint64_t,uint64_t> > bf_get_remote_shared_cache_hits(void);
  extern bool suppress_output(void);

  // The following library variables are used in files other than the
  // one in which they're defined.
  extern const char* bf_func_and_parents;   // Name of the current function and its parents
  extern string bf_output_prefix;           // Prefix appearing before each line of output
  extern const char* opcode2name[];         // Map from an LLVM opcode to its name
  extern KeyType_t bf_func_and_parents_id;  // Top of the complete_call_stack stack
  extern bool bf_suppress_counting;         // Whether to update Byfl data structures

  // Encapsulate of all of our basic-block counters into a single structure.
  class ByteFlopCounters {
  public:
    uint64_t mem_insts[NUM_MEM_INSTS];  // Number of memory instructions by type
    uint64_t inst_mix_histo[NUM_LLVM_OPCODES];   // Histogram of instruction mix
    uint64_t terminators[BF_END_BB_NUM];    // Tally of basic-block terminator types
    uint64_t mem_intrinsics[BF_NUM_MEM_INTRIN];  // Tallies of data movement performed by memory intrinsics
    uint64_t loads;                     // Number of bytes loaded
    uint64_t stores;                    // Number of bytes stored
    uint64_t load_ins;                  // Number of load instructions executed
    uint64_t store_ins;                 // Number of store instructions executed
    uint64_t call_ins;                  // Number of function calls executed
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
                    uint64_t initial_call_ins=0,
                    uint64_t initial_flops=0,
                    uint64_t initial_fp_bits=0,
                    uint64_t initial_ops=0,
                    uint64_t initial_op_bits=0);

  // Assign new values into our counters.
  void assign (uint64_t* new_mem_insts,
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
               uint64_t new_op_bits);

  // Accumulate new values into our counters.
  void accumulate (uint64_t* more_mem_insts,
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
                   uint64_t more_op_bits);

  // Accumulate another counter's values into our counters.
  void accumulate (ByteFlopCounters* other);

  // Return the difference of our counters and another set of counters.
  ByteFlopCounters* difference (ByteFlopCounters* other, ByteFlopCounters* target=nullptr);

  // Reset all of our counters to zero.
  void reset (void);
};

// Define datatypes for tracking basic blocks on a per-function basis.
typedef const char* MapKey_t;
typedef CachedUnorderedMap<KeyType_t, ByteFlopCounters*> key2bfc_t;
typedef CachedUnorderedMap<MapKey_t, ByteFlopCounters*> str2bfc_t;
typedef str2bfc_t::iterator counter_iterator;

// The following library variables are used in files other than the one in
// which they're defined.
extern ByteFlopCounters global_totals;    // Global tallies of all of our counters
extern key2bfc_t& per_func_totals(void);
extern str2bfc_t& user_defined_totals(void);

}

#endif

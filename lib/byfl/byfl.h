/*
 * Common definitions across the helper library
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BYFL_H_
#define _BYFL_H_

#include <algorithm>
#include <cassert>
#include <fstream>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>
#include <wordexp.h>

#include "byfl-common.h"
#include "cachemap.h"
#include "opcode2name.h"

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
extern uint8_t  bf_tally_inst_mix;   // 1=maintain instruction mix histogram
extern uint8_t  bf_types;            // 1=count loads/stores per type
extern uint8_t  bf_unique_bytes;     // 1=tally and output unique bytes
extern uint8_t  bf_vectors;          // 1=bin then output vector characteristics

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
  typedef pair<bytecount_t, bytecount_t> bf_addr_tally_t;  // Number of times a count was seen ({count, multiplier})

  // The following library functions are used in files other than the
  // one in which they're defined.
  extern void bf_get_address_tally_hist (vector<bf_addr_tally_t>& histogram, uint64_t* total);
  extern void bf_get_median_reuse_distance(uint64_t* median_value, uint64_t* mad_value);
  extern void bf_get_reuse_distance(vector<uint64_t>** hist, uint64_t* unique_addrs);
  extern void bf_get_vector_statistics(const char* tag, uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
  extern void bf_get_vector_statistics(uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits);
  extern "C" void bf_push_basic_block(void);
  extern void bf_report_vector_operations(size_t call_stack_depth);
  extern uint64_t bf_tally_unique_addresses(const char* funcname);
  extern uint64_t bf_tally_unique_addresses_tb(const char* funcname);
  extern uint64_t bf_tally_unique_addresses_tb(void);
  extern uint64_t bf_tally_unique_addresses(void);
  extern "C" const char* bf_string_to_symbol(const char *nonunique);
  extern void initialize_byfl(void);
  extern void initialize_reuse(void);
  extern void initialize_symtable(void);
  extern void initialize_tallybytes(void);
  extern void initialize_threading(void);
  extern void initialize_ubytes(void);
  extern void initialize_vectors(void);

  // The following library variables are used in files other than the
  // one in which they're defined.
  extern const char* bf_func_and_parents;   // Name of the current function and its parents
  extern string bf_output_prefix;           // Prefix appearing before each line of output
  extern const char* opcode2name[];         // Map from an LLVM opcode to its name
}

#endif

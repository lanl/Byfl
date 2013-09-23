/*
 * Common definitions across both the LLVM
 * pass and the helper library
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BYFL_COMMON_H_
#define _BYFL_COMMON_H_

#include <string>
#include <cxxabi.h>
#include <stdlib.h>
#include <inttypes.h>

using namespace std;

enum {
  BF_OP_LOAD,
  BF_OP_STORE,
  BF_OP_NUM
};

enum {
  BF_REF_VALUE,
  BF_REF_POINTER,   
  BF_REF_NUM
};

enum {
  BF_AGG_SCALAR,
  BF_AGG_VECTOR,
  BF_AGG_NUM
};

enum {
  BF_TYPE_INT,
  BF_TYPE_FP,
  BF_TYPE_OTHER,
  BF_TYPE_NUM
};

enum {
  BF_WIDTH_8,
  BF_WIDTH_16,
  BF_WIDTH_32,
  BF_WIDTH_64,
  BF_WIDTH_128,
  BF_WIDTH_OTHER,
  BF_WIDTH_NUM
};

#define NUM_MEM_INSTS (BF_OP_NUM*BF_REF_NUM*BF_AGG_NUM*BF_TYPE_NUM*BF_WIDTH_NUM)

enum {
  BF_END_BB_ANY,       // Any terminator (i.e., total number of basic blocks)
  BF_END_BB_STATIC,    // Unconditional BranchInst
  BF_END_BB_DYNAMIC,   // IndirectBrinst, SwitchInst, or conditional BranchInst
  BF_END_BB_NUM
};

enum {
  BF_MEMSET_CALLS,     // Calls to llvm.memset.*
  BF_MEMSET_BYTES,     // Bytes stored by llvm.memset.*
  BF_MEMXFER_CALLS,    // Calls to either llvm.memcpy.* or llvm.memmove.*
  BF_MEMXFER_BYTES,    // Bytes loaded and stored by llvm.mem{cpy,move}.*
  BF_NUM_MEM_INTRIN
};

// Map a memory-access type to an index into bf_mem_insts_count[].
static inline uint64_t
mem_type_to_index(uint64_t memop,
		  uint64_t memref,
		  uint64_t memagg,
		  uint64_t memtype,
		  uint64_t memwidth)
{
  uint64_t idx = 0;
  idx = idx*BF_OP_NUM + memop;
  idx = idx*BF_REF_NUM + memref;
  idx = idx*BF_AGG_NUM + memagg;
  idx = idx*BF_TYPE_NUM + memtype;
  idx = idx*BF_WIDTH_NUM + memwidth;
  return idx;
}

// Attempt to demangle function names so the masses can follow
// along.  The caller must free() the result.
#pragma GCC diagnostic ignored "-Wunused-function"
static string
demangle_func_name(string mangled_name) {
  int status;
  char* demangled_name = __cxxabiv1::__cxa_demangle(mangled_name.c_str(), NULL, 0, &status);
  if (status == 0 && demangled_name != 0) {
    string result(demangled_name);
    free(demangled_name);
    return result;
  }
  else
    return string(mangled_name);
}

struct derived_measurements {
  double bytes_loaded_per_byte_stored;
  double ops_per_load_instr;
  double bits_loaded_stored_per_memory_op;
  double flops_per_conditional_indirect_branch;
  double ops_per_conditional_indirect_branch;
  double vector_ops_per_conditional_indirect_branch;
  double vector_ops_per_flop;
  double vector_ops_per_op;
  double ops_per_instruction;
  double bytes_per_flop;
  double bits_per_flop_bit;
  double bytes_per_op;
  double bits_per_nonmemory_op_bit;
  double unique_bytes_per_flop;
  double unique_bits_per_flop_bit;
  double unique_bytes_per_op;
  double unique_bits_per_nonmemory_op_bit;
  double bytes_per_unique_byte;
};

#endif


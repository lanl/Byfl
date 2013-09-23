#ifndef _BYFL_BINARY_H_
#define _BYFL_BINARY_H_


#include <stdint.h>
#include "byfl-common.h"

typedef enum {
  BF_LOADSTORES=0,
  BF_INSTMIX=1,
  BF_BASICBLOCKS=2,
  BF_VECTOROPS=3,
  BF_FUNCTIONS=4,
  BF_CALLEE=5,
  BF_RUNS=7,
  BF_DERIVED=8
} bf_table_t;

#define MAX_OPCODE_NAMELEN 25  // wild guess...
#define MAX_FUNCTION_NAMELEN 128  // wild guess...
#define MAX_RUN_NAMELEN 1028  // wild guess...
#define MAX_DATETIME_LEN 32
#define MAX_OUTPUTID_LEN 64  
#define MAX_BFOPTIONS_LEN 1028  

struct bf_runs_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  char datetime[MAX_DATETIME_LEN];
  char name[MAX_RUN_NAMELEN];
  uint64_t run_no;
  char output_id[MAX_OUTPUTID_LEN]; // rank or hostname
  char bf_options[MAX_BFOPTIONS_LEN];
};

// use orig byfl names for consistency
struct bf_loadstores_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  int lsid;         // unique id for this load/store summary info
  uint64_t tally;   // count of this load/store category
  short memop;      // load or store
  short memref;     // is pointer?
  short memagg;     // is vector?
  short memsize;    // number of bits (8, 16, 32, 64, 128, other)
  short memtype;    // int or fp or other
};

// do these all really need to be 64-bit values?
struct bf_basicblocks_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  uint64_t bbid;
  uint64_t num_merged;   
  uint64_t LD_bytes;   
  uint64_t ST_bytes;   
  uint64_t LD_ops;   
  uint64_t ST_ops;   
  uint64_t Flops;   
  uint64_t FP_bits;   
  uint64_t Int_ops;   
  uint64_t Int_op_bits;   
};

struct bf_instmix_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  char inst_type[MAX_OPCODE_NAMELEN]; 
  uint64_t tally;   // count of this inst type 
};

struct bf_vectorops_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  uint64_t vectid;
  uint64_t Elements;
  uint64_t Elt_bits;
  short IsFlop;
  uint64_t Tally;
  char Function[MAX_FUNCTION_NAMELEN];
};

struct bf_functions_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  uint64_t stackid;
  uint64_t LD_bytes;   
  uint64_t ST_bytes;   
  uint64_t LD_ops;   
  uint64_t ST_ops;   
  uint64_t Flops;   
  uint64_t FP_bits;   
  uint64_t Int_ops;   
  uint64_t Int_op_bits;   
  uint64_t Uniq_bytes;
  uint64_t Cond_brs;
  uint64_t Invocations;
  char Function[MAX_FUNCTION_NAMELEN];
  char Parent_func1[MAX_FUNCTION_NAMELEN];
  char Parent_func2[MAX_FUNCTION_NAMELEN];
  char Parent_func3[MAX_FUNCTION_NAMELEN];
  char Parent_func4[MAX_FUNCTION_NAMELEN];
  char Parent_func5[MAX_FUNCTION_NAMELEN];
  char Parent_func6[MAX_FUNCTION_NAMELEN];
  char Parent_func7[MAX_FUNCTION_NAMELEN];
  char Parent_func8[MAX_FUNCTION_NAMELEN];
  char Parent_func9[MAX_FUNCTION_NAMELEN];
  char Parent_func10[MAX_FUNCTION_NAMELEN];
  char Parent_func11[MAX_FUNCTION_NAMELEN];
};  

struct bf_callee_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  uint64_t Invocations;
  short Byfl;
  char Function[MAX_FUNCTION_NAMELEN];
};

struct bf_derived_table {
  uint64_t sec;  // UTC seconds since epoch
  uint64_t usec;  // UTC microsecond portion
  derived_measurements dm;
};


#endif

#ifdef CMA
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
#endif



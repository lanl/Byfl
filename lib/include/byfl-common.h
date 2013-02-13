/*
 * Common definitions across both the LLVM
 * pass and the helper library
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BYFL_COMMON_H_
#define _BYFL_COMMON_H_

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

#endif

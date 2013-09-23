#include <iostream>
#include <fstream>
#include "byfl-common.h"
#include "byfl-binary.h"

using namespace std;

namespace bytesflops {

void binout_derived(uint64_t utc_sec, uint64_t utc_usec, ofstream* bfbinout, 
  const derived_measurements& dm) 
{
  bf_table_t table = BF_DERIVED;

  // write out the type of table entry this is
  bfbinout->write((char*)&table, sizeof(bf_table_t));

  bf_derived_table bftable = { utc_sec, utc_usec, dm};

  bfbinout->write((char*)&bftable, sizeof(bf_derived_table));
}
 }

#ifdef CMA
    dm.bytes_loaded_per_byte_stored,
    dm.ops_per_load_instr,
    dm.bits_loaded_stored_per_memory_op,
    dm.flops_per_conditional_indirect_branch,
    dm.ops_per_conditional_indirect_branch,
    dm.vector_ops_per_conditional_indirect_branch,
    dm.vector_ops_per_flop,
    dm.vector_ops_per_op,
    dm.ops_per_instruction,
    dm.bytes_per_flop,
    dm.bits_per_flop_bit,
    dm.bytes_per_op,
    dm.bits_per_nonmemory_op_bit,
    dm.unique_bytes_per_flop,
    dm.unique_bits_per_flop_bit,
    dm.unique_bytes_per_op,
    dm.unique_bits_per_nonmemory_op_bit,
    dm.bytes_per_unique_byte
#endif

/*
 * Interface Byfl to PAPI's software defined events
 *
 * By Heike Jagode <jagode@icl.utk.edu>
 */

#include "papi_sde.h"

void initialize_papi_sde(void)
{
  papi_sde_fptr_struct_t fptr_struct;

  POPULATE_SDE_FPTR_STRUCT(fptr_struct);
  papi_sde_hook_list_events(&fptr_struct);
}


/** This function registers and (optionally) describes events
 *  available from BYFL for listing in papi_native_avail.
 *  @param[in] papi_sde_fptr_struct_t fptr_struct
 *             fptr_struct->init -- function pointer to papi_sde_init
 *             fptr_struct->register_counter -- name of the event
 *             fptr_struct->describe_counter -- description of the event
 **/
papi_handle_t papi_sde_hook_list_events( papi_sde_fptr_struct_t *fptr_struct)
{
  int i;
  void* sde_handle = nullptr;

  const char* byfl_counter_name[] = {
    "byfl::load_count",
    "byfl::store_count",
    "byfl::load_ins_count",
    "byfl::store_ins_count",
    "byfl::call_ins_count",
    "byfl::flop_count",
    "byfl::fp_bits_count",
    "byfl::op_count",
    "byfl::op_bits_count"
  };

  const char* byfl_counter_description[] = {
    "Total number of bytes loaded.",
    "Total number of bytes stored.",
    "Total number of load instructions performed.",
    "Total number of store instructions performed.",
    "Total number of function-call instructions (non-exception-throwing) performed.",
    "Total number of FP operations performed.",
    "Total number of bits used by all FP operations.",
    "Total number of operations performed.",
    "Total number of bits used by all operations except loads/stores."
  };

  uint64_t* byfl_counter_count[] = {
    &bf_load_count,
    &bf_store_count,
    &bf_load_ins_count,
    &bf_store_ins_count,
    &bf_call_ins_count,
    &bf_flop_count,
    &bf_fp_bits_count,
    &bf_op_count,
    &bf_op_bits_count
  };

  /* papi_sde_init() */
  sde_handle = fptr_struct->init("byfl");

  /* papi_sde_register_counter() */
  for (i=0; i<BYFL_MAX_COUNTERS; i++) {
    fptr_struct->register_counter( sde_handle, byfl_counter_name[i],
      PAPI_SDE_long_long, PAPI_SDE_RO|PAPI_SDE_DELTA,
      byfl_counter_count[i] );
  }

  /* papi_sde_describe_counter() */
  for (i=0; i<BYFL_MAX_COUNTERS; i++) {
    fptr_struct->describe_counter( sde_handle, byfl_counter_name[i],
      byfl_counter_description[i] );
  }

  return sde_handle;
}

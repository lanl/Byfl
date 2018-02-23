/*
 * Interface Byfl to PAPI's software defined events
 *
 * By Heike Jagode <jagode@icl.utk.edu>
 */

#ifndef PAPI_SDE_H
#define PAPI_SDE_H

#include "byfl.h"

#define BYFL_MAX_COUNTERS 9

extern uint64_t  bf_load_count;
extern uint64_t  bf_store_count;
#if 0
/* TODO: */
extern uint64_t* bf_mem_insts_count;  // memory instructions by type
extern uint64_t* bf_inst_mix_histo;   // instruction mix (as histogram)
extern uint64_t* bf_terminator_count; // terminators by type
extern uint64_t* bf_mem_intrin_count; // memory intrinsic calls and data movement
#endif
extern uint64_t  bf_load_ins_count;
extern uint64_t  bf_store_ins_count;
extern uint64_t  bf_call_ins_count;
extern uint64_t  bf_flop_count;
extern uint64_t  bf_fp_bits_count;
extern uint64_t  bf_op_count;
extern uint64_t  bf_op_bits_count;

void initialize_papi_sde(void);

#endif

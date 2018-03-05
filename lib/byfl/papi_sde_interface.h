/*
 * Interface Byfl to PAPI's software defined events
 *
 * By Heike Jagode <jagode@icl.utk.edu>
 */

#ifndef PAPI_SDE_INTERFACE_H
#define PAPI_SDE_INTERFACE_H

/* 'cntr_mode' input parameter for papi_sde_register_counter */
#define SDE_RO        0x00  // 0000 0000
#define SDE_RW        0x01  // 0000 0001
#define SDE_DELTA     0x00  // 0000 0000
#define SDE_INSTANT   0x10  // 0001 0000

/* 'cntr_type' input parameter for papi_sde_register_counter */
#define PAPI_SDE_long_long 0x0
#define PAPI_SDE_int       0x1
#define PAPI_SDE_double    0x2
#define PAPI_SDE_float     0x3

/* Interface to PAPI SDE functions */
typedef void* papi_handle_t;

papi_handle_t papi_sde_init(const char *name_of_library, int event_count);
void papi_sde_register_counter(papi_handle_t handle, const char *event_name, int cntr_mode, int cntr_type, void *counter);
void papi_sde_describe_counter(papi_handle_t handle, const char *event_name, const char *event_description);

/* Required for papi_native_avail */
void papi_sde_hook_list_events(
    papi_handle_t (*sym_init)(const char *, int),
    void          (*sym_reg)(void *, const char *, int, int, void *),
    void          (*sym_desc)(void *, const char *, const char *));

#endif

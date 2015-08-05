/*
 * Declarations for the Byfl API
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BYFL_H_
#define _BYFL_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Associate an arbitrary textual tag with a fragment of a data structure (an
 * individual memory allocation).  Has no effect without the -bf-data-structs
 * compile flag. */
extern void bf_tag_data_region (void* address, const char *tag);

/* Toggle suppression of Byfl counter updates. */
extern void bf_enable_counting (int enable);

#ifdef __cplusplus
}
#endif

#endif

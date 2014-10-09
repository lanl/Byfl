/*
 * Library for parsing Byfl binary output files
 * (header file)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BFBIN_H_
#define _BFBIN_H_

#include <inttypes.h>
#include "binarytagdefs.h"

/* Define a structure containing pointers to library callback functions. */
typedef struct {
  void (*error_cb)(void *user_data, const char *message);       /* Arbitrary error */
  void (*table_basic_cb)(void *user_data, const char *name);    /* Beginning of a basic table */
  void (*table_keyval_cb)(void *user_data, const char *name);   /* Beginning of a key:value table */
  void (*table_end_cb)(void *user_data);                        /* End of the current table */
  void (*column_begin_cb)(void *user_data);                     /* Beginning of a list of column headers */
  void (*column_uint64_cb)(void *user_data, const char *name);  /* Header for an unsigned 64-bit integer column */
  void (*column_string_cb)(void *user_data, const char *name);  /* Header for a string column */
  void (*column_bool_cb)(void *user_data, const char *name);    /* Header for a Boolean column */
  void (*column_end_cb)(void *user_data);                       /* End of the current list of column headers */
  void (*row_begin_cb)(void *user_data);                        /* Beginning of a row of data */
  void (*data_uint64_cb)(void *user_data, uint64_t data);       /* Unsigned 64-bit integer data */
  void (*data_string_cb)(void *user_data, const char *data);    /* String data */
  void (*data_bool_cb)(void *user_data, uint8_t data);          /* Boolean data (0 or 1) */
  void (*row_end_cb)(void *user_data);                          /* End of a row of data */
} bfbin_callback_t;

#endif

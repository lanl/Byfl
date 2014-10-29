/*
 * Library for parsing Byfl binary output files
 * (SWIG-enabled header file)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifdef SWIG
%module bfbin
%ignore opaque_stuff;
%{
# undef SWIG
# include <byfl/bfbin-swig.h>
# define SWIG
%}
#endif

#ifndef _BFBIN_SWIG_H_
#define _BFBIN_SWIG_H_

#include <inttypes.h>

 /* Type of items that can be returned */
enum {
  ERROR,               /* Arbitrary error */
  FILE_BEGIN,          /* File processing is about to begin */
  TABLE_BEGIN_BASIC,   /* Beginning of a basic table */
  TABLE_END_BASIC,     /* End of a basic table */
  TABLE_BEGIN_KEYVAL,  /* Beginning of a key:value table */
  TABLE_END_KEYVAL,    /* End of a key:value table */
  COLUMN_BEGIN,        /* Beginning of a list of column headers */
  COLUMN_UINT64,       /* Header for an unsigned 64-bit integer column */
  COLUMN_STRING,       /* Header for a string column */
  COLUMN_BOOL,         /* Header for a Boolean column */
  COLUMN_END,          /* End of the current list of column headers */
  ROW_BEGIN,           /* Beginning of a row of data */
  DATA_UINT64,         /* Unsigned 64-bit integer data */
  DATA_STRING,         /* String data */
  DATA_BOOL,           /* Boolean data (0 or 1) */
  ROW_END,             /* End of a row of data */
  FILE_END             /* No more tables are forthcoming */
};

/* Parsed table item */
#ifdef SWIG
%immutable;
#endif
typedef struct {
  int item_type;          /* One of the preceding constants */
  uint64_t integer;       /* 64-bit unsigned integer value */
  const char *string;     /* Null-terminated string value */
  uint8_t boolean;        /* Boolean value */
} table_item_t;
#ifdef SWIG
%mutable;
#endif

/* Internal parsing state -- not exposed to SWIG */
typedef struct {
#ifdef SWIG
  int opaque_stuff;
#else
  table_item_t data;                      /* Data from library */
  pthread_cond_t data_available;          /* Library -> SWIG: "Data is available" */
  pthread_mutex_t data_available_lock;    /* Lock for the above */
  pthread_cond_t data_consumed;           /* SWIG -> library: "I'm done with the data" */
  pthread_mutex_t data_consumed_lock;     /* Lock for the above */
  char *filename;                         /* Byfl filename */
  pthread_t tid;                          /* ID for library thread */
#endif
} parse_state_t;

/* Declare the parsing functions. */
#ifdef __cplusplus
extern "C" {
#endif
extern parse_state_t *bf_open_byfl_file (const char *byfl_filename);
extern table_item_t bf_read_byfl_item (parse_state_t *state);
extern void bf_close_byfl_file (parse_state_t *state);
#ifdef __cplusplus
}
#endif

#endif

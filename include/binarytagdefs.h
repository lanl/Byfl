/*
 * Tags to use for Byfl's binary data output -- for use both by the
 * Byfl library and by postprocessing tools
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BINARY_TAG_DEFS_H
#define _BINARY_TAG_DEFS_H

/* Define tags for various table types in the binary output. */
typedef enum {
  BINOUT_TABLE_NONE,     /* No more tables follow (i.e., EOF) */
  BINOUT_TABLE_BASIC,    /* A basic table of columnar data follows */
  BINOUT_TABLE_KEYVAL    /* A key:value table follows */
} BINOUT_TABLE_T;

/* Define tags for column types in the binary output. */
typedef enum {
  BINOUT_COL_NONE,       /* No more column headers follow */
  BINOUT_COL_UINT64,     /* Column contains unsigned 64-bit integers */
  BINOUT_COL_STRING,     /* Column contains text strings */
  BINOUT_COL_BOOL        /* Column contains boolean values */
} BINOUT_COL_T;

/* Define tags for row types in the binary output. */
typedef enum {
  BINOUT_ROW_NONE,       /* No columns in this row (i.e., end of table) */
  BINOUT_ROW_DATA        /* Columns will follow */
} BINOUT_ROW_T;

#endif

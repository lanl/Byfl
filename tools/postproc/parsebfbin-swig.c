/*
 * Non-callback-based SWIG wrapper for
 * parsing Byfl binary output files
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#include <bfbin.h>
#include <bfbin-swig.h>

/* Abort the parsing thread. */
static void abort_parsing_thread (parse_state_t *lstate, const char *message)
{
  lstate->data.item_type = ERROR;
  lstate->data.string = strdup(message);
  (void) pthread_exit(NULL);
}

/* Transmit data from the library to the SWIG-invoked thread.  Invoke
 * abort_parsing_thread() on error. */
static void transfer_data (parse_state_t *lstate)
{
  if (write(lstate->channel[1], &lstate->data, sizeof(table_item_t)) != sizeof(table_item_t))
    abort_parsing_thread(lstate, "Failed to send data between threads");
}

/* Clear the previous data. */
static void clear_data (table_item_t *data)
{
  memset((void *) data, 0, sizeof(table_item_t));
}

/* Return a nullary value to the SWIG-invoked thread. */
static void return_null_value (parse_state_t *lstate, int item_type)
{
  clear_data(&lstate->data);
  lstate->data.item_type = item_type;
  transfer_data(lstate);
}

/* Return a string value to the SWIG-invoked thread. */
static void return_string_value (parse_state_t *lstate, int item_type, const char *value)
{
  clear_data(&lstate->data);
  lstate->data.item_type = item_type;
  lstate->data.string = strdup(value);
  transfer_data(lstate);
}

/* Return an unsigned 64-bit integer value to the SWIG-invoked thread. */
static void return_integer_value (parse_state_t *lstate, int item_type, uint64_t value)
{
  clear_data(&lstate->data);
  lstate->data.item_type = item_type;
  lstate->data.integer = (unsigned long) value;
  transfer_data(lstate);
}

/* Return a Boolean as an unsigned 8-bit integer value to the SWIG-invoked thread. */
static void return_boolean_value (parse_state_t *lstate, int item_type, uint8_t value)
{
  clear_data(&lstate->data);
  lstate->data.item_type = item_type;
  lstate->data.boolean = (int)value;
  transfer_data(lstate);
}

/* Callback for table_begin_basic_cb */
static void begin_basic_table (void *state, const char *name)
{
  return_string_value((parse_state_t *)state, TABLE_BEGIN_BASIC, name);
}

/* Callback for table_end_basic_cb */
static void end_basic_table (void *state)
{
  return_null_value((parse_state_t *)state, TABLE_END_BASIC);
}

/* Callback for table_begin_keyval_cb */
static void begin_keyval_table (void *state, const char *name)
{
  return_string_value((parse_state_t *)state, TABLE_BEGIN_KEYVAL, name);
}

/* Callback for table_end_keyval_cb */
static void end_keyval_table (void *state)
{
  return_null_value((parse_state_t *)state, TABLE_END_KEYVAL);
}

/* Callback for column_begin_cb */
static void begin_column (void *state)
{
  return_null_value((parse_state_t *)state, COLUMN_BEGIN);
}

/* Callback for column_uint64_cb */
static void column_integer_type (void *state, const char *name)
{
  return_string_value((parse_state_t *)state, COLUMN_UINT64, name);
}

/* Callback for column_string_cb */
static void column_string_type (void *state, const char *name)
{
  return_string_value((parse_state_t *)state, COLUMN_STRING, name);
}

/* Callback for column_bool_cb */
static void column_boolean_type (void *state, const char *name)
{
  return_string_value((parse_state_t *)state, COLUMN_BOOL, name);
}

/* Callback for column_end_cb */
static void end_column (void *state)
{
  return_null_value((parse_state_t *)state, COLUMN_END);
}

/* Callback for row_begin_cb */
static void begin_row (void *state)
{
  return_null_value((parse_state_t *)state, ROW_BEGIN);
}

/* Callback for row_uint64_cb */
static void row_integer_type (void *state, uint64_t value)
{
  return_integer_value((parse_state_t *)state, DATA_UINT64, value);
}

/* Callback for row_string_cb */
static void row_string_type (void *state, const char *value)
{
  return_string_value((parse_state_t *)state, DATA_STRING, value);
}

/* Callback for row_bool_cb */
static void row_boolean_type (void *state, uint8_t value)
{
  return_boolean_value((parse_state_t *)state, DATA_BOOL, value);
}

/* Callback for row_end_cb */
static void end_row (void *state)
{
  return_null_value((parse_state_t *)state, ROW_END);
}

/* Callback for error_cb */
static void handle_parse_error (void *state, const char *message)
{
  return_string_value((parse_state_t *)state, ERROR, message);
}

/* Thread entry point for invoking bf_process_byfl_file() in a
 * separate thread. */
static void *parse_byfl_file (void *state)
{
  bfbin_callback_t callback_list;

  /* Prepare a callback list. */
  parse_state_t *lstate = (parse_state_t *)state;
  memset(&callback_list, 0, sizeof(bfbin_callback_t));
  callback_list.error_cb = handle_parse_error;
  callback_list.table_begin_basic_cb = begin_basic_table;
  callback_list.table_end_basic_cb = end_basic_table;
  callback_list.table_begin_keyval_cb = begin_keyval_table;
  callback_list.table_end_keyval_cb = end_keyval_table;
  callback_list.column_begin_cb = begin_column;
  callback_list.column_uint64_cb = column_integer_type;
  callback_list.column_string_cb = column_string_type;
  callback_list.column_bool_cb = column_boolean_type;
  callback_list.column_end_cb = end_column;
  callback_list.row_begin_cb = begin_row;
  callback_list.data_uint64_cb = row_integer_type;
  callback_list.data_string_cb = row_string_type;
  callback_list.data_bool_cb = row_boolean_type;
  callback_list.row_end_cb = end_row;

  /* Announce that we're ready to go. */
  return_string_value((parse_state_t *)state, FILE_BEGIN, lstate->filename);

  /* Invoke the Byfl library function to parse the entire file. */
  bf_process_byfl_file(lstate->filename, &callback_list, state);

  /* Indicate that we've finished parsing. */
  return_null_value(lstate, FILE_END);
  return NULL;
}

/* Create, initialize, and return new parsing state.  Return NULL on
 * failure. */
parse_state_t *bf_open_byfl_file (const char *byfl_filename)
{
  parse_state_t *state;

  /* Allocate parse state. */
  state = (parse_state_t *) malloc(sizeof(parse_state_t));
  if (!state)
    return NULL;
  memset((void *) state, 0, sizeof(parse_state_t));

  /* Initialize the state. */
  if (pipe(state->channel) == -1)
    return NULL;
  state->filename = strdup(byfl_filename);
  state->data.item_type = FILE_BEGIN;    /* Must not be ERROR or FILE_END, or bf_read_byfl_item will lock onto that. */

  /* Process the file in the background. */
  if (pthread_create(&state->tid, NULL, parse_byfl_file, state) != 0) {
    free(state->filename);
    free(state);
    return NULL;
  }
  return state;
}

/* Block until the library thread returns data then return a copy of
 * that to the caller. */
table_item_t bf_read_byfl_item (parse_state_t *lstate)
{
  static table_item_t result = {0};   /* Copy of the data to return */

  /* Free the *previous* string value, if any, now that we no longer
   * need it. */
  if (result.string != NULL) {
    free((void *) result.string);
    result.string = NULL;
  }

  /* Wait for data from the library thread then return it. */
  if (read(lstate->channel[0], &result, sizeof(table_item_t)) != sizeof(table_item_t)) {
    clear_data(&result);
    result.item_type = ERROR;
    result.string = strdup("pthread_barrier_wait() failed");
  }
  return result;
}

/* Free resources that were needed for parsing. */
void bf_close_byfl_file (parse_state_t *lstate)
{
  (void) pthread_cancel(lstate->tid);  /* It's not an error if the thread already exited. */
  (void) close(lstate->channel[0]);
  (void) close(lstate->channel[1]);
  clear_data(&lstate->data);
  free((void *)lstate->filename);
  free((void *)lstate);
}

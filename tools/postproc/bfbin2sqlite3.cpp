/**********************************************
 * Convert Byfl binary data to SQLite3 format *
 * By Scott Pakin <pakin@lanl.gov>            *
 **********************************************/

#include <iostream>
#include <sqlite3.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "bfbin.h"

using namespace std;

// Store the name of the current executable.
static string progname;

// Abort the program.  This is expected to be used at the end of a
// stream write.
static ostream& die (ostream& os)
{
  os.flush();
  exit(1);
  return os;
}

// Define our local state.
class LocalState
{
private:
  string replace_extension (const string oldfilename, const string newext);
  void show_usage (ostream& os);

public:
  sqlite3* db;                // Handle to database
  string byflfilename;        // Name of input file
  string sqlite3filename;     // Name of output file
  string tablename;           // Current table name
  string create_table;        // Current CREATE TABLE statement as text
  string insert_into;         // Current INSERT INTO statement as text
  sqlite3_stmt* insert_stmt;  // Compiled INSERT INTO statement template
  size_t num_rows;            // Number of rows written to the current table
  const size_t rows_per_transaction = 1000000;   // Maximum number of rows per transaction
  int column;                 // Current column number
  bool issued_overflow_warning;  // true=an integer already overflowed in the current table; false=not yet
  bool live_data;             // true=wait for more data to arrive; false=fail if data aren't available

  LocalState(int argc, char *argv[]);
  ~LocalState();
};

// Output a usage string.
void LocalState::show_usage (ostream& os)
{
  os << "Usage: " << progname
     << " [--output=<filename.db>]"
     << " [--live-data]"
     << " <filename.byfl>\n";
}

// Parse the command line and open the database file.
LocalState::LocalState (int argc, char *argv[])
{
  // Initialize our local state.
  db = nullptr;
  live_data = false;

  // Walk the command line and process each option we encounter.
  static struct option cmd_line_options[] = {
    { "help",            no_argument,       NULL, 'h' },
    { "output",          required_argument, NULL, 'o' },
    { "live-data",       no_argument,       NULL, 'l' },
    { NULL,              0,                 NULL, 0 }
  };
  int opt_index = 0;
  while (true) {
    int c = getopt_long(argc, argv, "ho:l", cmd_line_options, &opt_index);
    if (c == -1)
      break;
    switch (c) {
      case 'h':
        show_usage(cout);
        exit(0);
        break;

      case 'o':
        sqlite3filename = string(optarg);
        break;

      case 'l':
        live_data = true;
        break;

      case 0:
        cerr << progname << ": Internal error in " << __FILE__
             << ", line " << __LINE__ << '\n' << die;
        break;

      default:
        show_usage(cout);
        exit(1);
        break;
    }
  }

  // Parse the remaining non-option, if any.
  switch (argc - optind) {
    case 1:
      // Exactly one argument: Store it as the input file name.
      byflfilename = string(argv[optind]);
      break;

    case 0:
      // No arguments: Complain.
      cerr << progname << ": The name of a Byfl binary file must be specified\n" << die;
      break;

    default:
      // More than one argument: Complain.
      cerr << progname << ": Only a single input file is allowed to be specified\n" << die;
      break;
  }

  // Choose a name for the output file if not specified explicitly.
  if (sqlite3filename == "")
    sqlite3filename = replace_extension(byflfilename, ".db");

  // Create a new database file.
  (void) unlink(sqlite3filename.c_str());
  int retval = sqlite3_open_v2(sqlite3filename.c_str(),
                               &db,
                               SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                               NULL);
  if (retval != SQLITE_OK)
    cerr << progname << ": " << sqlite3_errstr(retval)
         << " (" << sqlite3filename << ")\n" << die;
}

// Close the database.
LocalState::~LocalState (void)
{
  (void) sqlite3_close_v2(db);
}

// Replace a file extension.
string LocalState::replace_extension (const string oldfilename, const string newext)
{
  size_t dot_ofs = oldfilename.rfind('.');
  if (dot_ofs == string::npos)
    return oldfilename + newext;
  string newfilename(oldfilename);
  return newfilename.replace(dot_ofs, string::npos, newext);
}

// Quote a symbol name for a SQL query.  Specifically, double each
// single-quote character and wrap the entire symbol in single quotes.
static string sql_symbol (const char* in_str)
{
  string out_str("'");
  for (const char* cp = in_str; *cp != '\0'; cp++) {
    if (*cp == '\'')
      out_str += '\'';
    out_str += *cp;
  }
  out_str += '\'';
  return out_str;
}

// Do the same as above but accept an STL string.
static string sql_symbol (const string& in_str)
{
  return sql_symbol(in_str.c_str());
}

// Execute a SQL statement and abort on error.
static void execute_sql_statement (LocalState* lstate, const string& statement, const string& error_template)
{
  char *errmsg = nullptr;
  int retval = sqlite3_exec(lstate->db, statement.c_str(), NULL, NULL, &errmsg);
  if (retval != SQLITE_OK) {
    cerr << progname << ": " << error_template << " (" << errmsg << ")\n";
    sqlite3_free(errmsg);
    (void) sqlite3_close_v2(lstate->db);
    cerr << die;
  }
  sqlite3_free(errmsg);
}

// If the return value of a SQLite function is not as expected, issue
// a given error message, followed by "table" and the table name,
// followed by a string version of the error code, then exit.
static void check_return_value (int retval, LocalState* lstate,
                                const char* error_message,
                                int expected_retval=SQLITE_OK)
{
  if (retval != expected_retval) {
    cerr << progname << ": " << error_message << " table \""
         << lstate->tablename << "\" (" << sqlite3_errstr(retval) << ")\n";
    (void) sqlite3_close_v2(lstate->db);
    cerr << die;
  }
}

// Report a parse error and abort.
static void error_callback (void* state, const char* message)
{
  LocalState* lstate = (LocalState*) state;
  (void) sqlite3_close_v2(lstate->db);
  cerr << progname << ": " << message << endl << die;
}

// Begin a CREATE TABLE statement and an INSERT INTO statement.
static void begin_any_table (void* state, const char* tablename)
{
  LocalState* lstate = (LocalState*) state;
  lstate->tablename = tablename;
  string tablesym = sql_symbol(lstate->tablename);
  lstate->create_table = "CREATE TABLE " + tablesym + " (";
  lstate->insert_into = "INSERT INTO " + tablesym + " VALUES (";
  lstate->num_rows = 0;
  lstate->issued_overflow_warning = false;
}

// Append a column of a given data type to the column description.
static void add_any_column (void* state, const char* colname, const char* coltype)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->create_table[lstate->create_table.length() - 1] != '(') {
    lstate->create_table += ", ";
    lstate->insert_into += ", ";
  }
  string colsym = sql_symbol(colname);
  lstate->create_table += colsym + ' ' + coltype;
  lstate->insert_into += '?';
}

// Append an integer column to the column description.
static void column_header_integer (void* state, const char* colname)
{
  add_any_column(state, colname, "INTEGER");
}

// Append a string column to the column description.
static void column_header_string (void* state, const char* colname)
{
  add_any_column(state, colname, "TEXT");
}

// Append a boolean column to the column description.
static void column_header_boolean (void* state, const char* colname)
{
  add_any_column(state, colname, "INTEGER");  // SQLite doesn't have a Boolean type.
}

// Execute the SQL CREATE TABLE statement we constructed and prepare
// to insert data.
static void end_column_header (void* state)
{
  LocalState* lstate = (LocalState*) state;

  // Ignore empty tables.
  if (lstate->create_table[lstate->create_table.length() - 1] == '(')
    return;

  // Create a table.
  lstate->create_table += ");";
  execute_sql_statement(lstate, lstate->create_table,
                        "Failed to create table \"" + lstate->tablename + '"');
  lstate->create_table = "";

  // Create a template for inserting data.
  lstate->insert_into += ");";
  check_return_value(sqlite3_prepare_v2(lstate->db, lstate->insert_into.c_str(),
                                        lstate->insert_into.length() + 1,
                                        &lstate->insert_stmt, NULL),
                     lstate, "Failed to compile an insertion template for");
  lstate->insert_into = "";

  // Begin a transaction.
  execute_sql_statement(lstate, "BEGIN IMMEDIATE TRANSACTION;",
                        "Failed to begin a transaction for table \"" + lstate->tablename + '"');
}

// Begin a row of data.
static void begin_row (void* state)
{
  LocalState* lstate = (LocalState*) state;
  lstate->column = 0;
}

// Write an integer value.  If the integer is too big, issue a warning
// and write a double instead.
static void write_integer_value (void* state, uint64_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (value > UINT64_C(9223372036854775807)) {
    // Value is too big for an int64 -- complain and write a double instead.
    if (!lstate->issued_overflow_warning) {
      cerr << progname << ": WARNING -- an integral value in table \""
           << lstate->tablename << "\" doesn't fit within a signed 64-bit integer;"
           << " using floating-point instead" << endl;
      lstate->issued_overflow_warning = true;
    }
    check_return_value(sqlite3_bind_double(lstate->insert_stmt, lstate->column + 1,
                                           double(value)),
                       lstate, "Failed to insert an integer (cast to double) into");
  }
  else
    // Common case (we hope) -- write the uint64 as an int64.
    check_return_value(sqlite3_bind_int64(lstate->insert_stmt, lstate->column + 1,
                                          sqlite3_int64(value)),
                       lstate, "Failed to insert an integer into");
  lstate->column++;
}

// Write a text value.
static void write_text_value (void* state, const char* value)
{
  LocalState* lstate = (LocalState*) state;
  check_return_value(sqlite3_bind_text(lstate->insert_stmt, lstate->column + 1,
                                       value, strlen(value), SQLITE_TRANSIENT),
                     lstate, "Failed to insert a string into");
  lstate->column++;
}

// Write a Boolean value.
static void write_boolean_value (void* state, uint8_t value)
{
  LocalState* lstate = (LocalState*) state;
  check_return_value(sqlite3_bind_int(lstate->insert_stmt, lstate->column + 1,
                                      int(value)),
                     lstate, "Failed to insert a Boolean into");
  lstate->column++;
}

// Write a row of data to the database.  Periodically commit the
// transaction and begin a new transaction.
static void end_row (void* state)
{
  LocalState* lstate = (LocalState*) state;

  // Ignore empty tables.
  if (lstate->create_table[lstate->create_table.length() - 1] == '(')
    return;

  // Execute the INSERT INTO statement.
  check_return_value(sqlite3_step(lstate->insert_stmt),
                     lstate, "Failed to write a row to", SQLITE_DONE);

  // Prepare to insert new parameters into the INSERT INTO template.
  check_return_value(sqlite3_reset(lstate->insert_stmt),
                     lstate, "Failed to reset the insertion template for");

  // Periodically commit the current transaction and begin a new one.
  lstate->num_rows++;
  if (lstate->live_data || lstate->num_rows == lstate->rows_per_transaction) {
    execute_sql_statement(lstate, "END TRANSACTION;",
                          "Failed to commit a transaction for table \"" + lstate->tablename + '"');
    execute_sql_statement(lstate, "BEGIN IMMEDIATE TRANSACTION;",
                          "Failed to begin a transaction for table \"" + lstate->tablename + '"');
    lstate->num_rows = 0;
  }
}

// End the current table, cleaning up any resources used.
static void end_any_table (void* state)
{
  LocalState* lstate = (LocalState*) state;

  // Ignore empty tables.
  if (lstate->create_table[lstate->create_table.length() - 1] == '(')
    return;

  // Commit the current transaction.
  execute_sql_statement(lstate, "END TRANSACTION;",
                        "Failed to commit a transaction for table \"" + lstate->tablename + '"');
  check_return_value(sqlite3_finalize(lstate->insert_stmt),
                     lstate, "Failed to finalize the insertion template for");
}

int main (int argc, char *argv[])
{
  // Store the base filename of the current executable in progname.
  progname = argv[0];
  size_t slash_ofs = progname.rfind('/');
  if (slash_ofs != string::npos)
    progname.erase(0, slash_ofs + 1);

  // Allocate and initialize some local state.
  LocalState state(argc, argv);

  // Register callbacks.
  bfbin_callback_t callbacks;
  memset(&callbacks, 0, sizeof(bfbin_callback_t));
  callbacks.error_cb = error_callback;
  callbacks.table_begin_basic_cb = begin_any_table;
  callbacks.table_begin_keyval_cb = begin_any_table;
  callbacks.column_uint64_cb = column_header_integer;
  callbacks.column_string_cb = column_header_string;
  callbacks.column_bool_cb = column_header_boolean;
  callbacks.column_end_cb = end_column_header;
  callbacks.row_begin_cb = begin_row;
  callbacks.data_uint64_cb = write_integer_value;
  callbacks.data_string_cb = write_text_value;
  callbacks.data_bool_cb = write_boolean_value;
  callbacks.row_end_cb = end_row;
  callbacks.table_end_basic_cb = end_any_table;
  callbacks.table_end_keyval_cb = end_any_table;

  // Process the input file.
  bf_process_byfl_file(state.byflfilename.c_str(), &callbacks, &state, int(state.live_data));
  return 0;
}

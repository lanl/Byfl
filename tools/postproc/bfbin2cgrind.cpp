/****************************************
 * Convert Byfl binary data to suitable *
 * input for KCachegrind                *
 *                                      *
 * By Scott Pakin <pakin@lanl.gov>      *
 ****************************************/

#include <iostream>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "bfbin.h"

using namespace std;

// Define the name of the current executable.
string progname;

// Abort the program.  This is expected to be used at the end of a
// stream write.
ostream& die (ostream& os)
{
  os.flush();
  exit(1);
  return os;
}

// Define a column (name, type, and data) data type.
class Column {
public:
  enum column_t {
    UINT64_T,               // Unsigned 64-bit integer column
    STRING_T,               // String column
    BOOL_T                  // Boolean column
  };
  string name;              // Column name
  size_t number;            // Column number
  column_t type;            // Column data type
  union {
    vector<uint64_t>* uint64_data;   // Integer data
    vector<string>* string_data;     // String data
    vector<bool>* bool_data;         // Boolean data
  };

  // Create a column given its name, number, and data type.
  Column(string n, size_t no, column_t t) : name(n), number(no), type(t) {
    switch (t) {
      case UINT64_T:
        uint64_data = new vector<uint64_t>;
        break;

      case STRING_T:
        string_data = new vector<string>;
        break;

      case BOOL_T:
        bool_data = new vector<bool>;
        break;

      default:
        cerr << "Internal error: Invalid column type\n" << die;
        break;
    }
  }

  // Append data to the column.
  void push_back(uint64_t v) { uint64_data->push_back(v); }
  void push_back(string v)   { string_data->push_back(v); }
  void push_back(bool v)     { bool_data->push_back(v); }
};

// Define a type for our local parsing state.
class LocalState {
private:
  Column* func_col;            // Column in table_data corresponding to function names
  Column* file_col;            // Column in table_data corresponding to file names
  Column* lineno_col;          // Column in table_data corresponding to line numbers
  Column* invoke_col;          // Column in table_data corresponding to invocation counts
  unordered_map<string, string> short_evname;   // Mapping from long to short event names
  unordered_map<string, int> func2id;   // Map from a function name to an ID
  unordered_map<int, string> id2func;   // Map from an ID to a function name
  unordered_map<string, int> fname2id;  // Map from a file name to an ID
  unordered_map<int, string> id2fname;  // Map from an ID to a file name
  unordered_map<int, int> funcid2fnameid;   // Map from a function ID to a file ID

public:
  enum table_state_t {
    UNINTERESTING,             // We not in any interesting table
    IN_FUNCS,                  // We're currently in the Functions table
    IN_SYSINFO,                // We're currently in the System Information table
    IN_CMDLINE                 // We're currently in the Command Line table
  };

  // Define a node in a trie of call paths (sequences of functions but not
  // finer resolution than that).
  class TrieNode {
  private:
    LocalState* lstate;        // Local state on which to operate

  public:
    string funcname;                             // Name of a function
    string filename;                             // File that defines the function
    uint64_t lineno;                             // Line number at which the function definition begins
    unordered_map<string, TrieNode*> children;   // All functions it calls
    vector<uint64_t> self_data;                  // Metric data for this node only
    vector<uint64_t> path_data;                  // Metric data for this node and all its descendants
    uint64_t invocations;                        // Number of times this node was encountered

    // Construct a TrieNode given the local state it should refer to.
    TrieNode(LocalState* ls) : lstate(ls), invocations(0) { }

    // Insert a call path into a trie given its column and row in table_data.
    void insert(size_t func_col, size_t row);

    // Accumulate data from children into their parent.
    void propagate_data_upwards();

    // Output a node and, recursively, all of its children.
    void output_callgrind(ostream& of);
  };

  string infilename;           // Name of the input file
  string outfilename;          // Name of the output file
  ostream* outfile;            // Output file stream
  table_state_t table_state;   // Whether we're processing the current table or not
  vector<Column*> table_data;  // All columns, all rows of data
  size_t current_col;          // Current column number
  TrieNode* call_forest;       // All function call paths with pointers to data
  bool have_func_table;        // true=seen Functions table
  bool have_sysinfo_table;     // true=seen System Information table
  bool have_cmdline_table;     // true=seen Command Line table
  vector<string> sysinfo_keys;    // All keys in the System Information table
  vector<string> sysinfo_values;  // All values in the System Information table
  vector<string> cmdline_values;  // All values in the Command Line table

  LocalState(int argc, char* argv[]);
  ~LocalState();
  string short_event_name(string longname);
  void finalize();
  void output_callgrind();
};

// Insert a call path into a trie given its column and row in table_data.
void LocalState::TrieNode::insert (size_t col, size_t row)
{
  // Split the call path into a vector of strings.
  vector<string> call_path;
  string& call_path_str = (*lstate->table_data[col]->string_data)[row];
  const string sep(" # ");
  size_t pos0, pos1;
  for (pos0 = 0, pos1 = call_path_str.find(sep);
       pos1 != string::npos;
       pos0 = pos1 + sep.size(), pos1 = call_path_str.find(sep, pos0 + 1))
    call_path.push_back(call_path_str.substr(pos0, pos1 - pos0));
  call_path.push_back(call_path_str.substr(pos0, pos1 - pos0));

  // Insert the call path into the trie.
  TrieNode* node = lstate->call_forest;
  if (node == nullptr)
    node = new TrieNode(lstate);
  for (int idx = call_path.size() - 1; idx >= 0; idx--) {
    auto niter = node->children.find(call_path[idx]);
    if (niter == node->children.end())
      node = node->children[call_path[idx]] = new TrieNode(lstate);
    else
      node = niter->second;
  }

  // Copy all column data from the given row into the leaf node.
  for (auto citer = lstate->table_data.begin(); citer != lstate->table_data.end(); citer++) {
    // Include only integer columns (except line numbers).
    Column* column = *citer;
    if (column->type != Column::UINT64_T)
      continue;
    string cname(column->name);
    if (cname == "Line number" || cname == "Leaf line number")
      continue;
    node->self_data.push_back((*column->uint64_data)[row]);
    node->path_data.push_back((*column->uint64_data)[row]);
    node->funcname = (*lstate->func_col->string_data)[row];
    node->funcname = node->funcname.substr(0, node->funcname.find(" # "));
    node->filename = (*lstate->file_col->string_data)[row];
    node->lineno = (*lstate->lineno_col->uint64_data)[row];
    node->invocations = (*lstate->invoke_col->uint64_data)[row];
  }
}

// Accumulate data from children into their parent.
void LocalState::TrieNode::propagate_data_upwards (void)
{
  for (auto citer = children.begin(); citer != children.end(); citer++) {
    TrieNode* child = citer->second;
    child->propagate_data_upwards();
    for (size_t i = 0; i < self_data.size(); i++)
      path_data[i] += child->path_data[i];
    invocations += child->invocations;
  }
}

// Output a node and, recursively, all of its children.
void LocalState::TrieNode::output_callgrind (ostream& of)
{
  // Output our exclusive data.
  int file_id = lstate->fname2id[filename];
  int func_id = lstate->func2id[funcname];
  of << "fl=(" << file_id << ")\n"
     << "fn=(" << func_id << ")\n"
     << lineno;
  for (auto diter = self_data.begin(); diter != self_data.end(); diter++)
    of << ' ' << *diter;
  of << '\n';

  // Output our immediate children's data.
  for (auto citer = children.begin(); citer != children.end(); citer++) {
    TrieNode* child = citer->second;
    int cfile_id = lstate->fname2id[child->filename];
    int cfunc_id = lstate->func2id[child->funcname];
    if (cfile_id != file_id)
      of << "cfl=(" << cfile_id << ")\n"
         << "cfn=(" << cfunc_id << ")\n";
    else
      if (cfunc_id != func_id)
        of << "cfn=(" << cfunc_id << ")\n";
    of << "calls=" << child->invocations << ' ' << child->lineno << '\n'
       << lineno;
    for (auto diter = child->path_data.begin(); diter != child->path_data.end(); diter++)
      of << ' ' << *diter;
    of << '\n';
  }
  of << '\n';

  // Recursively output each of our children.
  for (auto citer = children.begin(); citer != children.end(); citer++)
    citer->second->output_callgrind(of);
}

// Parse the command line into a LocalState.
LocalState::LocalState (int argc, char* argv[])
{
  // Initialize the current state.
  infilename = "";
  outfilename = "";
  outfile = &cout;
  have_func_table = false;
  have_sysinfo_table = false;

  // Read an input file name and optional output file name.
  switch (argc - 1) {
    case 0:
      // No arguments: Complain.
      cerr << progname << ": The name of a Byfl binary file must be specified\n" << die;
      break;

    case 1:
      // Exactly one argument: Store it as the input file name.
      infilename = string(argv[1]);
      break;

    case 2:
      // Exactly two arguments: Store the first as the input file name
      // and the second as the output file name.
      infilename = string(argv[1]);
      outfilename = string(argv[2]);
      break;

    default:
      // More than two arguments: Complain.
      cerr << progname << ": Only a single input file and single output file are allowed to be specified\n" << die;
      break;
  }

  // Open the output file if specified.
  if (outfilename != "") {
    ofstream* named_outfile = new ofstream(outfilename, ofstream::trunc|ofstream::binary);
    if (!named_outfile->is_open())
      cerr << progname << ": Failed to open " << outfilename
           << " for writing\n" << die;
    outfile = named_outfile;
  }

  // Construct a mapping from long to short event names.
  short_evname["Load operations"] = "LD_ops";
  short_evname["Store operations"] = "ST_ops";
  short_evname["Floating-point operations"] = "FP_ops";
  short_evname["Integer operations"] = "Int_ops";
  short_evname["Function-call operations (non-exception-throwing)"] = "Calls";
  short_evname["Function-call operations (exception-throwing)"] = "Calls_exc";
  short_evname["Unconditional and direct branch operations (removable)"] = "Br_rem";
  short_evname["Unconditional and direct branch operations (mandatory)"] = "Br_mand";
  short_evname["Conditional branch operations (not taken)"] = "Cond_Br_NT";
  short_evname["Conditional branch operations (taken)"] = "Cond_Br_T";
  short_evname["Unconditional but indirect branch operations"] = "Ind_Br";
  short_evname["Multi-target (switch) branch operations"] = "Sw";
  short_evname["Function-return operations"] = "Ret";
  short_evname["Other branch operations"] = "Other_br";
  short_evname["Floating-point operation bits"] = "FP_bits";
  short_evname["Integer operation bits"] = "Int_bits";
  short_evname["Bytes loaded"] = "LD_bytes";
  short_evname["Bytes stored"] = "ST_bytes";
  short_evname["Calls to memset"] = "Memset";
  short_evname["Bytes stored by memset"] = "Memset_bytes";
  short_evname["Calls to memcpy and memmove"] = "Memcpy";
  short_evname["Bytes loaded and stored by memcpy and memmove"] = "Memcpy_bytes";
  short_evname["Unique bytes"] = "Uniq_bytes";
  short_evname["Invocations"] = "Invokes";
}

// Flush the output stream and close it if it's a file.
LocalState::~LocalState()
{
  if (outfilename != "")
    delete outfile;
  else
    outfile->flush();
}

// Reduce an event name to a short string.
string LocalState::short_event_name (string longname)
{
  string shortname;
  auto siter = short_evname.find(longname);
  if (siter == short_evname.end()) {
    // Unknown name: Make up something for now, but we really ought to fix the
    // code to match the run-time library.
    static size_t id = 1;
    char sname[25];
    sprintf(sname, "E%lu", id++);
    shortname = string(sname);
    short_evname[longname] = shortname;
  }
  else
    shortname = siter->second;
  return shortname;
}

// Construct various tables in preparation for output.
void LocalState::finalize (void)
{
  // Find the line-number column.
  lineno_col = nullptr;
  for (auto citer = table_data.begin(); citer != table_data.end(); citer++) {
    Column* column = *citer;
    string colname(column->name);
    if (colname == "Line number" || colname == "Leaf line number")
      lineno_col = column;
  }
  if (lineno_col == nullptr)
    cerr << progname << ": Failed to find a \"Line number\" or \"Leaf line number\" column in the \"Functions\" table_data\n" << die;
  if (lineno_col->type != Column::UINT64_T)
    cerr << progname << ": The \""
         << lineno_col->name
         << "\" column does not contain integer data\n" << die;

  // Find the invocation-count column.
  invoke_col = nullptr;
  for (auto citer = table_data.begin(); citer != table_data.end(); citer++) {
    Column* column = *citer;
    string colname(column->name);
    if (colname == "Invocations")
      invoke_col = column;
  }
  if (invoke_col == nullptr)
    cerr << progname << ": Failed to find an \"Invocations\" column in the \"Functions\" table_data\n" << die;
  if (invoke_col->type != Column::UINT64_T)
    cerr << progname << ": The \""
         << invoke_col->name
         << "\" column does not contain integer data\n" << die;

  // Construct a mapping from file name to ID and back.
  size_t id = 1;
  file_col = nullptr;
  for (size_t i = 0; i < table_data.size(); i++)
    if (table_data[i]->name == "File name" || table_data[i]->name == "Leaf file name") {
      file_col = table_data[i];
      break;
    }
  if (file_col == nullptr)
    cerr << progname << ": Failed to find a \"File name\" or \"Leaf file name\" column in the \"Functions\" table_data\n" << die;
  if (file_col->type != Column::STRING_T)
    cerr << progname << ": The \""
         << file_col->name
         << "\" column does not contain string data\n" << die;
  for (auto riter = file_col->string_data->begin();
       riter != file_col->string_data->end();
       riter++) {
    string fname = *riter;
    if (fname2id.find(fname) != fname2id.end())
      continue;
    fname2id[fname] = id;
    id2fname[id] = fname;
    id++;
  }

  // Construct a mapping from demangled function name or demangled call stack
  // to ID and back.
  id = 1;
  func_col = nullptr;
  for (size_t i = 0; i < table_data.size(); i++)
    if (table_data[i]->name == "Demangled function name" || table_data[i]->name == "Demangled call stack") {
      func_col = table_data[i];
      break;
    }
  if (func_col == nullptr)
    cerr << progname << ": Failed to find a \"Demangled function name\" or \"Demangled call stack\" column in the \"Functions\" table\n" << die;
  if (func_col->type != Column::STRING_T)
    cerr << progname << ": The \""
         << func_col->name
         << "\" column does not contain string data\n" << die;
  size_t row = 0;
  for (auto riter = func_col->string_data->begin();
       riter != func_col->string_data->end();
       riter++, row++) {
    string func = *riter;
    string func_only(func.substr(0, func.find(" # ")));
    if (func2id.find(func_only) != func2id.end())
      continue;
    func2id[func_only] = id;
    id2func[id] = func_only;
    funcid2fnameid[id] = fname2id[(*file_col->string_data)[row]];
    id++;
  }

  // Insert each call path into a trie forest.  Compute inclusive and exclude
  // metrics.
  size_t func_col_num = func_col->number;
  size_t nrows = func_col->string_data->size();
  call_forest = new LocalState::TrieNode(this);
  for (size_t r = 0; r < nrows; r++)
    call_forest->insert(func_col_num, r);
  auto citer = call_forest->children.begin();
  if (citer != call_forest->children.end()) {
    // Initialize the root to have all-zero data.
    TrieNode* child = citer->second;
    call_forest->self_data.resize(child->self_data.size(), 0);
    call_forest->path_data.resize(child->path_data.size(), 0);
  }
  call_forest->propagate_data_upwards();
}

// Output our table in Callgrind Profile Format, version 1.
void LocalState::output_callgrind (void)
{
  // Define a short alias for our output stream.
  ostream& of = *outfile;

  // Output some header information.
  of << "# KCachegrind view of " << infilename << '\n'
     << "version: 1\n"
     << "creator: bfbin2cgrind\n"
     << "positions: line\n"
     << "cmd:";
  for (auto cmditer = cmdline_values.begin(); cmditer != cmdline_values.end(); cmditer++)
    of << ' ' << *cmditer;
  of << "\n\n";

  // Output all event definitions (table columns).
  of << "# Define all of the events represented in the .byfl file.\n";
  vector<string> all_short_events;
  for (auto citer = table_data.begin(); citer != table_data.end(); citer++) {
    // Include only integer columns (except line numbers).
    Column* column = *citer;
    if (column->type != Column::UINT64_T)
      continue;
    string cname(column->name);
    if (cname == "Line number" || cname == "Leaf line number")
      continue;
    string sh_event(short_event_name(cname));
    all_short_events.push_back(sh_event);
    of << "event: " << sh_event << " : " << cname << '\n';
  }

  // Output a set of useful derived events.
  of << "event: Mem_ops = LD_ops + ST_ops + Memset + Memcpy : All memory operations\n"
     << "event: ALU_ops = FP_ops + Int_ops : All ALU operations\n"
     << "event: Bytes = LD_bytes + ST_bytes + Memset_bytes + 2*Memcpy_bytes : All bytes loaded or stored\n"
     << "event: Branches = Calls + Calls_exc + Br_rem + Br_mand + Cond_Br_NT + Cond_Br_T + Ind_br + Sw + Ret + Other_br : All branches\n";

  // Output a list of all event short names.
  of << "events:";
  for (auto siter = all_short_events.begin(); siter != all_short_events.end(); siter++)
    of << ' ' << *siter;
  of << "\n\n";

  // Report the totals of each event counter.
  of << "# Precompute each event's total across all positions.\n"
     << "summary:";
  for (auto eviter = call_forest->path_data.begin();
       eviter != call_forest->path_data.end();
       eviter++)
    of << ' ' << *eviter;
  of << "\n\n";

  // Output the executable name, if known.
  for (size_t i = 0; i < sysinfo_keys.size(); i++)
    if (sysinfo_keys[i] == "Executable name")
      of << "# Note the name of the executable that was instrumented.\n"
         << "ob=" << sysinfo_values[i] << "\n\n";

  // Output a mapping from file name to ID.
  of << "# Associate a small integer with each file name.\n";
  for (size_t id = 1; id <= id2fname.size(); id++) {
    string fname = id2fname[id];
    if (fname == "")
      of << "fl=(" << id << ") ???\n";
    else
      of << "fl=(" << id << ") " << fname << "\n";
  }
  of << '\n';

  // Output a mapping from demangled function name or demangled call stack
  // to ID.
  of << "# Associate a small integer with each function name.\n";
  int prev_file_id = -1;
  for (size_t id = 1; id <= id2func.size(); id++) {
    string func = id2func[id];
    int file_id = funcid2fnameid[id];
    if (file_id != prev_file_id) {
      of << "fl=(" << file_id << ")\n";
      prev_file_id = file_id;
    }
    of << "fn=(" << id << ") " << func << '\n';
  }
  of << '\n';

  // Output all call paths.
  of << "# List event values for each function on each call path.\n";
  for (auto riter = call_forest->children.begin();
       riter != call_forest->children.end();
       riter++)
    riter->second->output_callgrind(of);
}

// Report a parse error and abort.
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void error_callback (void* state, const char* message)
{
  cerr << progname << ": " << message << endl << die;
}

// Determine if we're in the Functions table.
static void begin_any_table (void* state, const char* tablename)
{
  LocalState* lstate = (LocalState*) state;
  if (strcmp(tablename, "Functions") == 0) {
    lstate->table_state = LocalState::IN_FUNCS;
    lstate->have_func_table = true;
  }
  else if (strcmp(tablename, "System information") == 0) {
    lstate->table_state = LocalState::IN_SYSINFO;
    lstate->have_sysinfo_table = true;
  }
  else if (strcmp(tablename, "Command line") == 0) {
    lstate->table_state = LocalState::IN_CMDLINE;
    lstate->have_cmdline_table = true;
  }
}

// Store the name and data type of an integer-typed column.
static void store_uint64_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data.push_back(new Column(colname, lstate->table_data.size(), Column::UINT64_T));
}

// Store the name and data type of a string-typed column.
static void store_string_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  switch (lstate->table_state) {
    case LocalState::IN_FUNCS:
      lstate->table_data.push_back(new Column(colname, lstate->table_data.size(), Column::STRING_T));
      break;

    case LocalState::IN_SYSINFO:
      lstate->sysinfo_keys.push_back(colname);
      break;

    default:
      break;
  }
}

// Store the name and data type of a Boolean-typed column.
static void store_boolean_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data.push_back(new Column(colname, lstate->table_data.size(), Column::BOOL_T));
}

// Reset the column counter at the beginning of each row.
static void begin_data_row (void* state)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS &&
      lstate->table_state != LocalState::IN_SYSINFO &&
      lstate->table_state != LocalState::IN_CMDLINE)
    return;
  lstate->current_col = 0;
}

// Store a 64-bit unsigned integer value in the current column.
static void store_uint64_value (void* state, uint64_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data[lstate->current_col++]->push_back(value);
}

// Store a string value in the current column.
static void store_string_value (void* state, const char* value)
{
  LocalState* lstate = (LocalState*) state;
  switch (lstate->table_state) {
    case LocalState::IN_FUNCS:
      lstate->table_data[lstate->current_col++]->push_back(string(value));
      break;

    case LocalState::IN_SYSINFO:
      lstate->sysinfo_values.push_back(value);
      break;

    case LocalState::IN_CMDLINE:
      lstate->cmdline_values.push_back(value);
      break;

    default:
      break;
  }
}

// Store a Boolean value in the current column.
static void store_bool_value (void* state, uint8_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data[lstate->current_col++]->push_back(bool(value));
}

// Note that we finished processing the current table.
static void end_any_table (void* state)
{
  LocalState* lstate = (LocalState*) state;
  lstate->table_state = LocalState::UNINTERESTING;
}

int main (int argc, char *argv[])
{
  // Store the base filename of the current executable in progname.
  progname = argv[0];
  size_t slash_ofs = progname.rfind('/');
  if (slash_ofs != string::npos)
    progname.erase(0, slash_ofs + 1);

  // Parse the command line.
  LocalState state(argc, argv);

  // Register callbacks.
  bfbin_callback_t callbacks;
  memset(&callbacks, 0, sizeof(bfbin_callback_t));
  callbacks.error_cb = error_callback;
  callbacks.table_begin_basic_cb = begin_any_table;
  callbacks.table_begin_keyval_cb = begin_any_table;
  callbacks.column_uint64_cb = store_uint64_header;
  callbacks.column_string_cb = store_string_header;
  callbacks.column_bool_cb = store_boolean_header;
  callbacks.row_begin_cb = begin_data_row;
  callbacks.data_uint64_cb = store_uint64_value;
  callbacks.data_string_cb = store_string_value;
  callbacks.data_bool_cb = store_bool_value;
  callbacks.table_end_basic_cb = end_any_table;
  callbacks.table_end_keyval_cb = end_any_table;

  // Process the input file.
  bf_process_byfl_file(state.infilename.c_str(), &callbacks, &state, 0);
  if (!state.have_func_table)
    cerr << progname << ": Failed to find a Functions table in "
         << state.infilename
         << "; please re-compile your application with -bf-by-func or, preferably, -bf-call-stack and re-run it\n"
         << die;
  if (!state.have_sysinfo_table)
    cerr << progname << ": Failed to find a System Information table in "
         << state.infilename << '\n'
         << die;
  if (!state.have_cmdline_table)
    cerr << progname << ": Failed to find a Command Line table in "
         << state.infilename << '\n'
         << die;
  state.finalize();
  state.output_callgrind();
  return 0;
}

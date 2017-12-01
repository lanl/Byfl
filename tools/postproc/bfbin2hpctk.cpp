/******************************************
 * Convert Byfl binary data to suitable   *
 * input for HPCToolkit's GUI, hpcviewer  *
 *                                        *
 * By Scott Pakin <pakin@lanl.gov>        *
 ******************************************/

#include <iostream>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include "bfbin.h"
#include "bfbin2hpctk.h"

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
  column_t type;            // Column data type
  union {
    vector<uint64_t>* uint64_data;   // Integer data
    vector<string>* string_data;     // String data
    vector<bool>* bool_data;         // Boolean data
  };

  // Create a column given its name and data type.
  Column(string n, column_t t) {
    name = n;
    type = t;
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
public:
  enum table_state_t {
    PRE_FUNCS,                 // We haven't yet seen the Functions table
    IN_FUNCS,                  // We're currently in the Functions table
    POST_FUNCS                 // We already processed the Functions table
  };

  // Define a node in a trie of call paths (sequences of functions but not
  // finer resolution than that).
  class TrieNode {
  private:
    LocalState* lstate;        // Local state on which to operate

  public:
    unordered_map<string, TrieNode*> children;   // All functions it calls
    size_t row;                                  // Row in table_data representing the node if it's a leaf

    // Construct a TrieNode given the local state it should refer to.
    TrieNode(LocalState* ls) : lstate(ls), row(~0) { }

    // Insert a call path into a trie given its column and row in table_data.
    void insert(size_t func_col, size_t row);

    // Output a call path in XML format given an indentation level.
    void output_xml(ostream& of, int level);
  };

  string infilename;           // Name of the input file
  string short_infilename;     // Shortened version of the above (base name, no extension)
  ostream* xmlfile;            // Handle to experiment.xml file
  table_state_t table_state;   // Whether we're processing the current table or not
  vector<Column*> table_data;  // All columns, all rows of data
  size_t current_col;          // Current column number
  TrieNode* call_forest;       // All function call paths with pointers to data
  size_t id;                   // Unique ID for an arbitrary XML tag
  size_t loadmod_id;           // Unique ID for our one load module (executable)
  unordered_map<string, int> func2id;   // Map from a function name to an ID
  unordered_map<string, int> fname2id;  // Map from a file name to an ID
  Column* func_col;            // Column in table_data corresponding to function names
  Column* file_col;            // Column in table_data corresponding to file names
  Column* lineno_col;          // Column in table_data corresponding to line numbers
  string db_name;              // Name of the database (directory) to generate

  LocalState (int argc, char* argv[]);
  string quote_for_xml(const string& in_str);
  void create_database_dir();
  void copy_file(const string fname);
  void output_xml(ostream& of);
  void output_database();
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
  node->row = row;
}

// Output a call path in XML format.
void LocalState::TrieNode::output_xml (ostream& of, int level)
{
  // Output the load-module ID, function ID, file ID, and line number.
  for (int i = 0; i < level; i++)
    of << "  ";
  of << "      <PF i=\"" << lstate->id++ << '"'
     << " lm=\"" << lstate->loadmod_id << '"'
     << " n=\"" << lstate->func2id[(*lstate->func_col->string_data)[row]] << '"'
     << " f=\"" << lstate->fname2id[(*lstate->file_col->string_data)[row]] << '"'
     << " l=\"" << (*lstate->lineno_col->uint64_data)[row] << "\">\n";

  // Output metrics for the node itself.
  int mid = 1;   // Metric ID (starts at 1; 0 is the ID for the entire profile)
  for (auto citer = lstate->table_data.begin(); citer != lstate->table_data.end(); citer++) {
    // Process only integer columns (except line numbers).
    Column* column = *citer;
    if (column->type != Column::UINT64_T)
      continue;
    string cname(column->name);
    if (cname == "Line number" || cname == "Leaf line number")
      continue;
    for (int i = 0; i < level; i++)
      of << "  ";
    of << "        <M n=\"" << mid++ << "\" v=\""
       << (*column->uint64_data)[row] << "\"/>\n";
  }

  // Output metrics for each of the node's children.
  for (auto titer = children.begin(); titer != children.end(); titer++)
    titer->second->output_xml(of, level + 1);

  // Finish this node.
  for (int i = 0; i < level; i++)
    of << "  ";
  of << "      </PF>\n";
}

// Parse the command line into a LocalState.
LocalState::LocalState(int argc, char* argv[])
{
  // Initialize the current state.
  infilename = "";
  table_state = PRE_FUNCS;
  call_forest = nullptr;

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

    default:
      // More than one argument: Complain.
      cerr << progname << ": Only a single input file is allowed to be specified\n" << die;
      break;
  }

  // Define a short name for the input file.
  short_infilename = infilename;
  size_t last_slash_pos = short_infilename.rfind('/');
  if (last_slash_pos != string::npos)
    short_infilename = short_infilename.substr(last_slash_pos + 1);
  size_t last_dot_pos = short_infilename.rfind('.');
  if (last_dot_pos != string::npos)
    short_infilename = short_infilename.substr(0, last_dot_pos);
}

// Quote a string for XML output.
string LocalState::quote_for_xml (const string& in_str)
{
  string out_str;
  for (auto iter = in_str.cbegin(); iter != in_str.cend(); iter++) {
    switch (*iter) {
      case '<':
        out_str += "&lt;";
        break;

      case '>':
        out_str += "&gt;";
        break;

      case '&':
        out_str += "&amp;";
        break;

      case '"':
        out_str += "&quot;";
        break;

      default:
        out_str += *iter;
        break;
    }
  }
  return out_str;
}

// Output our entire accumulated state as XML.
void LocalState::output_xml (ostream& of)
{
  // Output the header boilerplate.  We use SecCallPathProfile instead of
  // SecFlatProfile because I've not seen an example of the latter and even had
  // trouble producing a SecFlatProfile that was acceptable to HPCViewer.
  id = 0;
  of << hpcviewer_header_boilerplate
     << "<HPCToolkitExperiment version=\"2.0\">\n"
     << "  <Header n=\"" << short_infilename << "\">\n"
     << "    <Info/>\n"
     << "  </Header>\n"
     << "  <SecCallPathProfile i=\"" << id++ << "\" n=\"" << short_infilename << "\">\n"
     << "    <SecHeader>\n";

  // Define each of our metrics (table columns).
  vector<Column*>& table = table_data;
  of << "      <MetricTable>\n";
  for (auto citer = table.begin(); citer != table.end(); citer++) {
    // Include only integer columns (except line numbers).
    Column* column = *citer;
    if (column->type != Column::UINT64_T)
      continue;
    string cname(column->name);
    if (cname == "Line number" || cname == "Leaf line number")
      continue;

    // Output the column name as a metric.
    of << "        <Metric i=\"" << id++ << "\" n=\""
       << cname
       << "\" v=\"raw\" t=\"exclusive\" show=\"1\" show-percent=\"0\" />\n";
  }
  of << "      </MetricTable>\n";

  // Output an empty metric-database table.
  of << "      <MetricDBTable>\n"
     << "      </MetricDBTable>\n";

  // Output a load-module table.
  string load_module(infilename);   // Fabricate a load module name based on the input filename.
  size_t last_dot_pos = load_module.rfind('.');
  if (last_dot_pos != string::npos)
    load_module = load_module.substr(0, last_dot_pos);
  loadmod_id = id++;
  of << "      <LoadModuleTable>\n"
     << "        <LoadModule i=\"" << loadmod_id << "\" n=\""
     << load_module << "\"/>\n"
     << "      </LoadModuleTable>\n";

  // Find the line-number column.
  lineno_col = nullptr;
  for (auto citer = table.begin(); citer != table.end(); citer++) {
    Column* column = *citer;
    string colname(column->name);
    if (colname == "Line number" || colname == "Leaf line number")
      lineno_col = column;
  }
  if (lineno_col == nullptr)
    cerr << progname << ": Failed to find a \"Line number\" or \"Leaf line number\" column in the \"Functions\" table\n" << die;
  if (lineno_col->type != Column::UINT64_T)
    cerr << progname << ": The \""
         << lineno_col->name
         << "\" column does not contain integer data\n" << die;

  // Construct a mapping from file name to ID.
  file_col = nullptr;
  for (size_t i = 0; i < table.size(); i++)
    if (table[i]->name == "File name" || table[i]->name == "Leaf file name") {
      file_col = table[i];
      break;
    }
  if (file_col == nullptr)
    cerr << progname << ": Failed to find a \"File name\" or \"Leaf file name\" column in the \"Functions\" table\n" << die;
  if (file_col->type != Column::STRING_T)
    cerr << progname << ": The \""
         << file_col->name
         << "\" column does not contain string data\n" << die;
  of << "      <FileTable>\n";
  for (auto riter = file_col->string_data->begin();
       riter != file_col->string_data->end();
       riter++) {
    string fname = *riter;
    if (fname2id.find(fname) != fname2id.end())
      continue;
    fname2id[fname] = id;
    if (fname == "")
      of << "        <File i=\"" << id << "\" n=\"~unknown-file~\"/>\n";
    else
      of << "        <File i=\"" << id
         << "\" n=\"./src" << quote_for_xml(fname) << "\"/>\n";
    id++;
  }
  of << "      </FileTable>\n";

  // Construct a mapping from demangled function name or demangled call stack
  // to ID.
  size_t func_col_num = (size_t)(-1);
  func_col = nullptr;
  for (size_t i = 0; i < table.size(); i++)
    if (table[i]->name == "Demangled function name" || table[i]->name == "Demangled call stack") {
      func_col_num = i;
      func_col = table[i];
      break;
    }
  if (func_col == nullptr)
    cerr << progname << ": Failed to find a \"Demangled function name\" or \"Demangled call stack\" column in the \"Functions\" table\n" << die;
  if (func_col->type != Column::STRING_T)
    cerr << progname << ": The \""
         << func_col->name
         << "\" column does not contain string data\n" << die;
  of << "      <ProcedureTable>\n";
  for (auto riter = func_col->string_data->begin();
       riter != func_col->string_data->end();
       riter++) {
    string func = *riter;
    if (func2id.find(func) != func2id.end())
      continue;
    func2id[func] = id;
    of << "        <Procedure i=\"" << id
       << "\" n=\"" << quote_for_xml(func.substr(0, func.find(" # "))) << "\"/>\n";
    id++;
  }
  of << "      </ProcedureTable>\n"
     << "    </SecHeader>\n";

  // Insert each call path into a trie forest then output the trie (except the
  // root, which exists solely to store the forest).
  size_t nrows = func_col->string_data->size();
  call_forest = new LocalState::TrieNode(this);
  for (size_t r = 0; r < nrows; r++)
    call_forest->insert(func_col_num, r);
  of << "    <SecCallPathProfileData>\n";
  for (auto titer = call_forest->children.begin();
       titer != call_forest->children.end();
       titer++)
    titer->second->output_xml(of, 0);
  of << "    </SecCallPathProfileData>\n";

  // Output the trailer boilerplate.
  of << "  </SecCallPathProfile>\n"
    "</HPCToolkitExperiment>\n";
}

// Create an HPCToolkit database directory.
void LocalState::create_database_dir (void)
{
  // First try to use a simple name.
  string base_db_name = string("hpctoolkit-") + short_infilename + string("-database");
  db_name = base_db_name;
  if (mkdir(db_name.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == 0)
    return;

  // That didn't work.  Keep trying different suffixes until the directory is
  // created successfully.
  char* db_name_str = new char[base_db_name.size() + 25];
  int num = getpid();
  while (errno == EEXIST) {
    sprintf(db_name_str, "%s-%d", base_db_name.c_str(), num);
    db_name = db_name_str;
    if (mkdir(db_name.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == 0)
      return;
    num++;
  }
  cerr << progname << ": Failed to create directory " << db_name
       << " (" << strerror(errno) << ")\n" << die;
  delete[] db_name_str;
}

// Copy a source file (named by absolute path) into the database directory.
void LocalState::copy_file (const string fname)
{
  // Create all the directories leading up to the file name proper.
  string dir_name(db_name + "/src");
  if (mkdir(dir_name.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1 &&
      errno != EEXIST)
    cerr << progname << ": Failed to create directory " << dir_name
         << " (" << strerror(errno) << ")\n" << die;
  size_t prev_slash_pos = 0;
  size_t slash_pos;
  for (slash_pos = fname.find('/', prev_slash_pos + 1);
       slash_pos != string::npos;
       prev_slash_pos = slash_pos, slash_pos = fname.find('/', prev_slash_pos + 1)) {
    dir_name += fname.substr(prev_slash_pos, slash_pos - prev_slash_pos);
    if (mkdir(dir_name.c_str(), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1 &&
        errno != EEXIST)
      cerr << progname << ": Failed to create directory " << dir_name
           << " (" << strerror(errno) << ")\n" << die;
  }

  // Copy the file.
  ifstream src_file(fname, ios::binary);
  if (!src_file)
      cerr << progname << ": Failed to open file " << fname
           << " for reading (" << strerror(errno) << ")\n" << die;
  string dest_name = db_name + "/src" + fname;
  ofstream dest_file(dest_name, ios::binary);
  if (!dest_file)
      cerr << progname << ": Failed to open file " << dest_name
           << " for writing (" << strerror(errno) << ")\n" << die;
  dest_file << src_file.rdbuf();
  src_file.close();
  dest_file.close();
  if (src_file.bad() || dest_file.bad())
    cerr << progname << ": Failed to copy file " << fname << " to "
         << dest_name << " (" << strerror(errno) << ")\n" << die;
}

// Create the entire output database.
void LocalState::output_database (void)
{
  // Create the database directory.
  create_database_dir();

  // Write the experiment.xml file.
  string xmlfilename(db_name + "/experiment.xml");
  ofstream xmlfile(xmlfilename);
  if (!xmlfile)
      cerr << progname << ": Failed to open file " << xmlfilename
           << " for writing (" << strerror(errno) << ")\n" << die;
  output_xml(xmlfile);
  xmlfile.close();
  if (!xmlfile)
    cerr << progname << ": Failed to write file " << xmlfilename
         << " (" << strerror(errno) << ")\n" << die;

  // Copy all files referenced by experiment.xml into the database directory.
  for (auto fiter = fname2id.begin(); fiter != fname2id.end(); fiter++) {
    const string& fname = fiter->first;
    if (fname != "")
      copy_file(fname);
  }
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
  if (strcmp(tablename, "Functions") == 0) {
    LocalState* lstate = (LocalState*) state;
    lstate->table_state = LocalState::IN_FUNCS;
  }
}

// Store the name and data type of an integer-typed column.
static void store_uint64_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data.push_back(new Column(colname, Column::UINT64_T));
}

// Store the name and data type of a string-typed column.
static void store_string_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data.push_back(new Column(colname, Column::STRING_T));
}

// Store the name and data type of a Boolean-typed column.
static void store_boolean_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data.push_back(new Column(colname, Column::BOOL_T));
}

// Reset the column counter at the beginning of each row.
static void begin_data_row (void* state)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
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
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data[lstate->current_col++]->push_back(string(value));
}

// Store a Boolean value in the current column.
static void store_bool_value (void* state, uint8_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_data[lstate->current_col++]->push_back(bool(value));
}

// Determine what table we finished.  If it's the Functions table we do all of
// our output.
static void end_any_table (void* state)
{
  // Update the table state.  Return if we're not finishing the Functions table.
  LocalState* lstate = (LocalState*) state;
  if (lstate->table_state != LocalState::IN_FUNCS)
    return;
  lstate->table_state = LocalState::POST_FUNCS;

  // Output the table state as a database suitable for input by hpcviewer.
  lstate->output_database();
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
  if (state.table_state != LocalState::POST_FUNCS)
    cerr << progname << ": Failed to find a Functions table in "
         << state.infilename
         << "; please re-compile your application with -bf-by-func or, preferably, -bf-call-stack and re-run it\n"
         << die;
  return 0;
}

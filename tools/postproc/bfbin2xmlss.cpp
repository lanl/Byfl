/********************************************************
 * Convert Byfl binary data to an Excel XML spreadsheet *
 * By Scott Pakin <pakin@lanl.gov>                      *
 ********************************************************/

#include <iostream>
#include <fstream>
#include <unordered_set>
#include <functional>
#include <bitset>
#include <vector>
#include <unistd.h>
#include <string.h>
#include "bfbin.h"

using namespace std;

// Define the name of the current executable.
string progname;

// Abort the program.  This is expected to be used at the end of a
// stream write.
static ostream& die (ostream& os)
{
  os.flush();
  exit(1);
  return os;
}

// Define a type for our local parsing state.
class LocalState {
private:
  string replace_extension (const string oldfilename, const string newext);
  void show_usage (ostream& os);

public:
  string infilename;       // Name of the input file
  string outfilename;      // Name of the output file
  ostream* outfile;        // Output file stream
  vector<string> colnames; // Name of each column in the current table
  size_t colnum;           // Current column number (maintained while outputting data)
  enum {
    XML_BASIC_TABLE,
    XML_KEYVAL_TABLE
  } tabletype;             // Type of table (basic or key:value)

  LocalState (int argc, char* argv[]);
  ~LocalState();
  string quote_for_xml (const string& in_str);
  string quote_for_xml (const string&& in_str);
};

// Parse the command line into a LocalState.
LocalState::LocalState (int argc, char* argv[])
{
  // Initialize the current state.
  infilename = "";
  outfilename = "";
  outfile = &cout;

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
      outfilename = replace_extension(infilename, ".xml");
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
}

// Flush the output stream and close it if it's a file.
LocalState::~LocalState()
{
  if (outfilename != "")
    delete outfile;
  else
    outfile->flush();
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

// Do the same as the above but accept an rvalue.
string LocalState::quote_for_xml (const string&& in_str)
{
  return quote_for_xml(ref(in_str));
}

// Report a parse error and abort.
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void error_callback (void* state, const char* message)
{
  cerr << progname << ": " << message << endl << die;
}

// Output XML header boilerplate.
static void write_xml_header (LocalState* lstate)
{
  *lstate->outfile << "<?xml version=\"1.0\"?>\n"
                   << "<?mso-application progid=\"Excel.Sheet\"?>\n"
                   << "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n"
                   << "          xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n"
                   << "          xmlns:x=\"urn:schemas-microsoft-com:office:excel\"\n"
                   << "          xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\"\n"
                   << "          xmlns:html=\"http://www.w3.org/TR/REC-html40\">\n"
                   << "  <Styles>\n"
                   << "    <Style ss:ID=\"sty-col-header\">\n"
                   << "      <Alignment ss:Horizontal=\"Center\" />\n"
                   << "      <Font ss:Bold=\"1\" />\n"
                   << "    </Style>\n"
                   << "    <Style ss:ID=\"sty-uint64\">\n"
                   << "      <NumberFormat ss:Format=\"#,##0\" />\n"
                   << "    </Style>\n"
                   << "    <Style ss:ID=\"sty-string\">\n"
                   << "    </Style>\n"
                   << "    <Style ss:ID=\"sty-bool\">\n"
                   << "    </Style>\n"
                   << "  </Styles>\n";
}

// Begin outputting a basic table.
static void begin_basic_table (void* state, const char* tablename)
{
  LocalState* lstate = (LocalState*) state;
  string name(tablename);

  // Begin a new worksheet for the current table.
  lstate->tabletype = LocalState::XML_BASIC_TABLE;
  *lstate->outfile << "  <Worksheet ss:Name=\""
                   << lstate->quote_for_xml(name) << "\">\n"
                   << "    <Table>\n";
}

// Begin outputting a key:value table.
static void begin_keyval_table (void* state, const char* tablename)
{
  LocalState* lstate = (LocalState*) state;
  string name(tablename);

  // Begin a new worksheet for the current table.
  lstate->tabletype = LocalState::XML_KEYVAL_TABLE;
  *lstate->outfile << "  <Worksheet ss:Name=\""
                   << lstate->quote_for_xml(name) << "\">\n"
                   << "    <Table>\n";
}

// Prepare to output a column header.
static void begin_column_header (void* state)
{
  LocalState* lstate = (LocalState*) state;
  lstate->colnames.clear();
  if (lstate->tabletype == LocalState::XML_KEYVAL_TABLE)
    // Key:value tables define exactly two columns, a string column
    // called "Key" and a column of possibly variable type called
    // "Value".
    *lstate->outfile << "      <Column ss:AutoFitWidth=\"1\" ss:StyleID=\"sty-string\" />\n"
                     << "      <Column ss:AutoFitWidth=\"1\" />\n";
}

// Define a column of 64-bit unsigned integers.
static void write_uint64_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  string name(colname);
  lstate->colnames.push_back(lstate->quote_for_xml(name));
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    *lstate->outfile << "      <Column ss:AutoFitWidth=\"1\" ss:StyleID=\"sty-uint64\" />\n";
}

// Define a column of strings.
static void write_string_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  string name(colname);
  lstate->colnames.push_back(lstate->quote_for_xml(name));
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    *lstate->outfile << "      <Column ss:AutoFitWidth=\"1\" ss:StyleID=\"sty-string\" />\n";
}

// Define a column of Booleans.
static void write_boolean_header (void* state, const char* colname)
{
  LocalState* lstate = (LocalState*) state;
  string name(colname);
  lstate->colnames.push_back(lstate->quote_for_xml(name));
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    *lstate->outfile << "      <Column ss:AutoFitWidth=\"1\" ss:StyleID=\"sty-bool\" />\n";
}

// Finish outputting a column header.
static void end_column_header (void* state)
{
  LocalState* lstate = (LocalState*) state;
  *lstate->outfile << "      <Row ss:StyleID=\"sty-col-header\">\n";
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    // Basic table -- output each stored column name.
    for (auto iter = lstate->colnames.cbegin(); iter != lstate->colnames.cend(); iter++)
      *lstate->outfile << "        <Cell><Data ss:Type=\"String\">"
                       << *iter
                       << "</Data></Cell>\n";
  else
    // Key:value table -- output a "Key" column and a "Value" column.
    *lstate->outfile << "        <Cell><Data ss:Type=\"String\">Key</Data></Cell>\n"
                     << "        <Cell><Data ss:Type=\"String\">Value</Data></Cell>\n";
  *lstate->outfile << "      </Row>\n";
}

// Begin outputting a row of data.
static void begin_data_row (void* state)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    *lstate->outfile << "      <Row>\n";
  lstate->colnum = 0;
}

// Write a 64-bit unsigned integer value.
static void write_uint64_value (void* state, uint64_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->tabletype == LocalState::XML_KEYVAL_TABLE)
    // Key:value table -- output a complete two-column row
    *lstate->outfile << "      <Row>\n"
                     << "        <Cell><Data ss:Type=\"String\">"
                     << lstate->colnames[lstate->colnum++]
                     << "</Data></Cell>\n"
                     << "        <Cell ss:StyleID=\"sty-uint64\"><Data ss:Type=\"Number\">"
                     << value
                     << "</Data></Cell>\n"
                     << "      </Row>\n";
  else
    // Basic table -- output a single cell within the existing row
    *lstate->outfile << "        <Cell><Data ss:Type=\"Number\">"
                     << value
                     << "</Data></Cell>\n";
}

// Write a string value.
static void write_string_value (void* state, const char* value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->tabletype == LocalState::XML_KEYVAL_TABLE)
    // Key:value table -- output a complete two-column row
    *lstate->outfile << "      <Row>\n"
                     << "        <Cell><Data ss:Type=\"String\">"
                     << lstate->colnames[lstate->colnum++]
                     << "</Data></Cell>\n"
                     << "        <Cell ss:StyleID=\"sty-string\"><Data ss:Type=\"Number\">"
                     << lstate->quote_for_xml(value)
                     << "</Data></Cell>\n"
                     << "      </Row>\n";
  else
    // Basic table -- output a single cell within the existing row
    *lstate->outfile << "        <Cell><Data ss:Type=\"String\">"
                     << lstate->quote_for_xml(value)
                     << "</Data></Cell>\n";
}

// Write a boolean value.
static void write_bool_value (void* state, uint8_t value)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->tabletype == LocalState::XML_KEYVAL_TABLE)
    // Key:value table -- output a complete two-column row
    *lstate->outfile << "      <Row>\n"
                     << "        <Cell><Data ss:Type=\"String\">"
                     << lstate->colnames[lstate->colnum++]
                     << "</Data></Cell>\n"
                     << "        <Cell ss:StyleID=\"sty-bool\"><Data ss:Type=\"Number\">"
                     << int(value)
                     << "</Data></Cell>\n"
                     << "      </Row>\n";
  else
    // Basic table -- output a single cell within the existing row
    *lstate->outfile << "        <Cell><Data ss:Type=\"Boolean\">"
                     << int(value)
                     << "</Data></Cell>\n";
}

// Finish outputting a row of data.
static void end_row (void* state)
{
  LocalState* lstate = (LocalState*) state;
  if (lstate->tabletype == LocalState::XML_BASIC_TABLE)
    *lstate->outfile << "      </Row>\n";
}

// Finish outputting a table (either type).
static void end_any_table (void* state)
{
  LocalState* lstate = (LocalState*) state;
  *lstate->outfile << "    </Table>\n"
                   << "  </Worksheet>\n";
}

// Output XML trailer boilerplate.
static void write_xml_trailer (LocalState* lstate)
{
  *lstate->outfile << "</Workbook>\n";
}

int main (int argc, char *argv[])
{
  string byfl_filename;      // Name of input file
  string text_filename;      // Name of output file

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
  callbacks.table_begin_basic_cb = begin_basic_table;
  callbacks.table_end_basic_cb = end_any_table;
  callbacks.table_begin_keyval_cb = begin_keyval_table;
  callbacks.table_end_keyval_cb = end_any_table;
  callbacks.column_begin_cb = begin_column_header;
  callbacks.column_uint64_cb = write_uint64_header;
  callbacks.column_string_cb = write_string_header;
  callbacks.column_bool_cb = write_boolean_header;
  callbacks.column_end_cb = end_column_header;
  callbacks.row_begin_cb = begin_data_row;
  callbacks.data_uint64_cb = write_uint64_value;
  callbacks.data_string_cb = write_string_value;
  callbacks.data_bool_cb = write_bool_value;
  callbacks.row_end_cb = end_row;

  // Process the input file.
  write_xml_header(&state);
  bf_process_byfl_file(state.infilename.c_str(), &callbacks, &state);
  write_xml_trailer(&state);
  return 0;
}

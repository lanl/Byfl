/*********************************************
 * Convert Byfl binary data to NetCDF format *
 * By Scott Pakin <pakin@lanl.gov>           *
 *********************************************/

#include <iostream>
#include <vector>
#include <string>
#include <H5Cpp.h>
#include <string.h>
#include "bfbin.h"

using namespace H5;
using namespace std;

// Enumerate the Byfl data types we expect to encounter.
typedef enum {
  BYFL_UINT64,
  BYFL_STRING,
  BYFL_BOOL
} byfl_datatype_t;

// Associate a column name and a column type.
typedef pair<string, byfl_datatype_t> column_info_t;

// Define our local program state.
typedef struct {
  H5File hdf5file;               // HDF5 file to write to
  string table_name;             // Name of the current table
  vector<column_info_t> column_info;   // Information about each column
  CompType datatype;             // HDF5 compound datatype for the current table's lone column
  DataSet dataset;               // HDF5 dataset for the current table's data
  hsize_t current_dims;          // Current dimensions of the HDF5 dataset (1-D)
  vector<uint8_t> row_data;      // A single row's worth of data
} program_state_t;

// Store the name of the current executable.
string progname;

// Define a chunk size to use for our data (required for
// unlimited-extent data).
const hsize_t chunk_size = 1024;

// Define a variable-length string datatype and a boolean datatype.
StrType strtype;
EnumType booltype;

// Define a single-row memory dataspace.
DataSpace mem_dataspace;

// Abort the program.  This is expected to be used at the end of a
// stream write.
ostream& die (ostream& os)
{
  os.flush();
  exit(1);
  return os;
}

// Replace a file extension.
string replace_extension (const string oldfilename, const string newext)
{
  size_t dot_ofs = oldfilename.rfind('.');
  if (dot_ofs == string::npos)
    return oldfilename + newext;
  string newfilename(oldfilename);
  return newfilename.replace(dot_ofs, string::npos, newext);
}

// Construct an HDF5 datatype based on a Byfl column header.
void construct_hdf5_datatype (program_state_t* s)
{
  // Compute the total size of the datatype we'll create.
  size_t datatype_bytes = 0;
  for (auto iter = s->column_info.cbegin(); iter != s->column_info.cend(); iter++)
    switch (iter->second) {
      case BYFL_UINT64:
        datatype_bytes += sizeof(uint64_t);
        break;

      case BYFL_STRING:
        datatype_bytes += sizeof(char*);
        break;

      case BYFL_BOOL:
        datatype_bytes += sizeof(uint8_t);
        break;

      default:
        cerr << progname << ": Internal error in " << __FILE__
             << ", line " << __LINE__ << endl << die;
        break;
    }

  // Construct an HDF5 datatype.
  CompType datatype(datatype_bytes);
  size_t byte_offset = 0;    // Running total of bytes consumed by each element
  for (auto iter = s->column_info.cbegin(); iter != s->column_info.cend(); iter++) {
    string colname = iter->first;
    byfl_datatype_t coltype = iter->second;
    switch (coltype) {
      case BYFL_UINT64:
        datatype.insertMember(colname, byte_offset, PredType::NATIVE_UINT64);
        byte_offset += sizeof(uint64_t);
        break;
      case BYFL_STRING:
        datatype.insertMember(colname, byte_offset, strtype);
        byte_offset += sizeof(char*);
        break;

      case BYFL_BOOL:
        datatype.insertMember(colname, byte_offset, booltype);
        byte_offset += sizeof(uint8_t);
        break;

      default:
        cerr << progname << ": Internal error at "
             << __FILE__ << ", line " << __LINE__ << endl
             << die;
        break;
    }
  }
  s->datatype = datatype;
}

// Free C strings that appear in raw row data.
void free_c_strings (program_state_t* s)
{
  uint8_t* rawptr = s->row_data.data();    // Pointer into the raw data
  for (auto iter = s->column_info.cbegin();
       iter != s->column_info.cend();
       iter++) {
    byfl_datatype_t coltype = iter->second;
    switch (coltype) {
      case BYFL_UINT64:
        rawptr += sizeof(uint64_t);
        break;

      case BYFL_STRING:
        {
          char* charptr = *(char**) rawptr;
          free((void*) charptr);
          rawptr += sizeof(char*);
        }
        break;

      case BYFL_BOOL:
        rawptr += sizeof(uint8_t);
        break;

      default:
        cerr << progname << ": Internal error at "
             << __FILE__ << ", line " << __LINE__ << endl
             << die;
        break;
    }
  }
}

// Report a parse error and abort.
#pragma GCC diagnostic ignored "-Wunused-parameter"
void error_callback (void* state, const char* message)
{
  cerr << progname << ": " << message << endl << die;
}

// Begin a new basic table.
void begin_basic_table (void* state, const char* table_name)
{
  program_state_t* s = (program_state_t*)state;
  s->table_name = table_name;
}

// Buffer information about a column of any type.
template<byfl_datatype_t coltype>
void add_column(void* state, const char* column_name)
{
  program_state_t* s = (program_state_t*)state;
  string name(column_name);
  s->column_info.push_back(column_info_t(name, coltype));
}

// When the column header is complete, create a table with
// appropriately typed columns and prepare to write data to it.
void end_column (void* state)
{
  program_state_t* s = (program_state_t*)state;

  // Create a global dataspace.
  s->current_dims = 0;
  hsize_t max_dims = H5S_UNLIMITED;
  DataSpace global_dataspace(1, &s->current_dims, &max_dims);

  // Define an HDF5 datatype based on the Byfl column header.
  construct_hdf5_datatype(s);

  // Create a dataset.  Enable chunking (required because of the
  // H5S_UNLIMITED dimension) and deflate compression (optional).
  DSetCreatPropList proplist;
  proplist.setChunk(1, &chunk_size);
  proplist.setDeflate(9);    // Maximal compression
  s->dataset = s->hdf5file.createDataSet(s->table_name, s->datatype,
                                         global_dataspace, proplist);
}

// Push a 64-bit unsigned integer onto the current row.
void write_uint64 (void* state, uint64_t value)
{
  program_state_t* s = (program_state_t*)state;
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    uint8_t onebyte = ((uint8_t*)&value)[i];
    s->row_data.push_back(onebyte);
  }
}

// Push a pointer to a C string onto the current row.
void write_string (void* state, const char *str)
{
  program_state_t* s = (program_state_t*)state;
  const char* sptr = strdup(str);                 // String pointer
  const uint8_t* sptrp = (const uint8_t*) &sptr;  // Address of the above, cast to an array of bytes
  for (size_t i = 0; i < sizeof(uint8_t*); i++)
    s->row_data.push_back(sptrp[i]);
}

// Push a Boolean value onto the current row as an 8-bit unsigned integer.
void write_bool (void* state, uint8_t value)
{
  program_state_t* s = (program_state_t*)state;
  s->row_data.push_back(value);
}

// At the end of row of data, write the data to the HDF5 file.
void end_row (void* state)
{
  program_state_t* s = (program_state_t*)state;

  // Extend the dataset by one row.
  s->current_dims++;
  s->dataset.extend(&s->current_dims);

  // Create a file dataspace.
  DataSpace file_dataspace = s->dataset.getSpace();

  // Point the file dataspace to the last row of the dataset.
  hsize_t data_offset = s->current_dims - 1;
  hsize_t num_rows = 1;
  file_dataspace.selectHyperslab(H5S_SELECT_SET, &num_rows, &data_offset);

  // Write the raw row data to the dataspace.
  uint8_t* raw_row_data = s->row_data.data();
  s->dataset.write(raw_row_data, s->datatype, mem_dataspace, file_dataspace);

  // Free the memory consumed by all of the C strings and by the row data.
  free_c_strings(s);
  s->row_data.clear();
}

// Clean up when we finish writing a basic table.
void end_basic_table (void* state)
{
  program_state_t* s = (program_state_t*)state;
  s->column_info.clear();
}

// Begin a new key:value table.
void begin_keyval_table (void* state, const char* table_name)
{
  program_state_t* s = (program_state_t*)state;
  s->table_name = table_name;
}

// Buffer unsigned 64-bit integer key:value data for later.
void store_keyval_uint64 (void* state, const char* column_name, uint64_t value)
{
  program_state_t* s = (program_state_t*)state;
  string name(column_name);
  s->column_info.push_back(column_info_t(name, BYFL_UINT64));
  for (size_t i = 0; i < sizeof(uint64_t); i++) {
    uint8_t onebyte = ((uint8_t*)&value)[i];
    s->row_data.push_back(onebyte);
  }
}

// Buffer string key:value data for later.
void store_keyval_string (void* state, const char* column_name, const char* str)
{
  program_state_t* s = (program_state_t*)state;
  string name(column_name);
  s->column_info.push_back(column_info_t(name, BYFL_STRING));
  const char* sptr = strdup(str);                 // String pointer
  const uint8_t* sptrp = (const uint8_t*) &sptr;  // Address of the above, cast to an array of bytes
  for (size_t i = 0; i < sizeof(uint8_t*); i++)
    s->row_data.push_back(sptrp[i]);
}

// Buffer boolean key:value data for later.
void store_keyval_bool (void* state, const char* column_name, uint8_t value)
{
  program_state_t* s = (program_state_t*)state;
  string name(column_name);
  s->column_info.push_back(column_info_t(name, BYFL_BOOL));
  s->row_data.push_back(value);
}

// When a key:value table is complete, do all of the work of writing
// it to HDF5.
void end_keyval_table (void* state)
{
  program_state_t* s = (program_state_t*)state;

  // Create a global dataspace.
  s->current_dims = 1;
  DataSpace global_dataspace(1, &s->current_dims);

  // Define an HDF5 datatype based on the Byfl column header.
  construct_hdf5_datatype(s);

  // Create a dataset.  We enable neither compression nor chunking
  // because we'll be writing only a single row, and these features
  // are unlikely to be of much use for a single-row dataset.
  s->dataset = s->hdf5file.createDataSet(s->table_name, s->datatype, global_dataspace);

  // Create a file dataspace.
  DataSpace file_dataspace = s->dataset.getSpace();

  // Point the file dataspace to the last row of the dataset.
  hsize_t data_offset = s->current_dims - 1;
  hsize_t num_rows = 1;
  file_dataspace.selectHyperslab(H5S_SELECT_SET, &num_rows, &data_offset);

  // Write the raw row data to the dataspace.
  uint8_t* raw_row_data = s->row_data.data();
  s->dataset.write(raw_row_data, s->datatype, mem_dataspace, file_dataspace);

  // Free the memory consumed by all of the C strings, by the row
  // data, and by the column data.
  free_c_strings(s);
  s->row_data.clear();
  s->column_info.clear();
}

// Convert a Byfl binary output file to an HDF5 file.
static void convert_byfl_to_hdf5 (string byflfilename, string hdf5filename)
{
  // Register an error-handling callback.
  bfbin_callback_t callbacks;
  memset(&callbacks, 0, sizeof(bfbin_callback_t));
  callbacks.error_cb = error_callback;

  // Register callbacks for processing basic tables.
  callbacks.table_begin_basic_cb = begin_basic_table;
  callbacks.column_uint64_cb = add_column<BYFL_UINT64>;
  callbacks.column_string_cb = add_column<BYFL_STRING>;
  callbacks.column_bool_cb = add_column<BYFL_BOOL>;
  callbacks.column_end_cb = end_column;
  callbacks.data_uint64_cb = write_uint64;
  callbacks.data_string_cb = write_string;
  callbacks.data_bool_cb = write_bool;
  callbacks.row_end_cb = end_row;
  callbacks.table_end_basic_cb = end_basic_table;

  // Register callbacks for processing key:value tables.
  callbacks.table_begin_keyval_cb = begin_keyval_table;
  callbacks.keyval_uint64_cb = store_keyval_uint64;
  callbacks.keyval_string_cb = store_keyval_string;
  callbacks.keyval_bool_cb = store_keyval_bool;
  callbacks.table_end_keyval_cb = end_keyval_table;

  // Create the output file.
  H5File hdf5file(hdf5filename, H5F_ACC_TRUNC);

  // Define our local state.
  program_state_t state;
  state.hdf5file = hdf5file;

  // Convert a Byfl binary output file to an HDF5 file.
  bf_process_byfl_file(byflfilename.c_str(),
                       &callbacks, sizeof(bfbin_callback_t),
                       &state);
}

int main (int argc, const char *argv[])
{
  string byflfilename;      // Name of input file
  string h5filename;        // Name of output file

  // Store the base filename of the current executable in progname.
  progname = argv[0];
  size_t slash_ofs = progname.rfind('/');
  if (slash_ofs != string::npos)
    progname.erase(0, slash_ofs + 1);

  // Parse the command line.
  if (argc < 2 || argc > 3) {
    cerr << "Usage: " << progname << " <input.byfl> [<output.h5>]\n";
    exit(1);
  }
  byflfilename = argv[1];
  if (argc > 2)
    h5filename = argv[2];
  else
    h5filename = replace_extension(byflfilename, ".h5");

  // Define a variable-length string datatype and a boolean datatype.
  strtype = StrType(PredType::C_S1, H5T_VARIABLE);
  booltype = EnumType(PredType::NATIVE_UINT8);
  uint8_t boolval = 0;
  booltype.insert("No", &boolval);
  boolval = 1;
  booltype.insert("Yes", &boolval);

  // Define a single-row memory dataspace.
  hsize_t num_rows = 1;
  mem_dataspace = DataSpace(1, &num_rows);

  // Convert the file format.
  convert_byfl_to_hdf5(byflfilename, h5filename);
  return 0;
}

/*********************************************
 * Convert Byfl binary data to NetCDF format *
 * By Scott Pakin <pakin@lanl.gov>           *
 *********************************************/

#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <H5Cpp.h>
#include "binarytagdefs.h"

using namespace H5;
using namespace std;

// Store the name of the current executable.
string progname;

// Define a chunk size to use for our data (required for
// unlimited-extent data).
const hsize_t chunk_size = 1024;

// Abort the program.  This is expected to be used at the end of a
// stream write.
ostream& die (ostream& os)
{
  os.flush();
  exit(1);
  return os;
}

// Inject a string version of errno into a stream.
ostream& errnoText (ostream& os)
{
  os << strerror(errno);
  return os;
}

// Represent an open and memory-mapped binary Byfl file.
class ByflFile
{
public:
  ByflFile (string filename);   // Map a file into memory.
  ~ByflFile (void);             // Unmap a file from memory.
  ByflFile& operator>> (string& str);      // Read a string from the file.
  ByflFile& operator>> (uint64_t& value);  // Read a uint64_t from the file.
  ByflFile& operator>> (uint8_t& value);   // Read a uint8_t from the file.
  ByflFile& read (char* str, streamsize n);  // Read a given number of bytes from the file.

private:
  void* base_ptr;        // Pointer to the start of the memory-mapped file contents
  uint8_t* ptr;          // Current pointer into the memory-mapped file contents
  int fd;                // Underlying file descriptor
  size_t mapped_bytes;   // Number of bytes mapped
};

// Map a file into memory.
ByflFile::ByflFile (string filename)
{
  struct stat fileinfo;

  fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1)
    cerr << progname << ": open: " << errnoText << " (" << filename << ")\n"
         << die;
  if (fstat(fd, &fileinfo) == -1)
    cerr << progname << ": fstat: " << errnoText << endl
         << die;
  mapped_bytes = fileinfo.st_size;
  base_ptr = mmap(NULL, mapped_bytes, PROT_READ, MAP_SHARED, fd, 0);
  if (base_ptr == MAP_FAILED)
    cerr << progname << ": mmap: " << errnoText << endl
         << die;
  ptr = (uint8_t*) base_ptr;
}

// Unmap a file from memory.
ByflFile::~ByflFile (void)
{
  if (munmap(base_ptr, mapped_bytes) == -1)
    cerr << progname << ": munmap: " << errnoText << endl
         << die;
  if (close(fd) == -1)
    cerr << progname << ": close: " << errnoText << endl
         << die;
}

// Read a string from a Byfl file.
ByflFile& ByflFile::operator>> (string& str)
{
  // Read a 16-bit big-endian string length.
  uint16_t len = (ptr[0] << 8) | ptr[1];
  ptr += 2;

  // Read the string itself.
  string newStr((const char*) ptr, len);
  ptr += len;
  str = newStr;
  return *this;
}

// Read a uint64_t from a Byfl file.
ByflFile& ByflFile::operator>> (uint64_t& value)
{
  int shift = 56;        // Shift for current byte
  value = 0;
  for (int i = 0; i < 8; i++, shift -= 8)
    value |= *ptr++ << shift;
  return *this;
}

// Read a uint8_t from a Byfl file.
ByflFile& ByflFile::operator>> (uint8_t& value)
{
  value = *ptr++;
  return *this;
}

// Read a given number of bytes from a Byfl file.
ByflFile& ByflFile::read (char* str, streamsize n)
{
  for (streamsize i = 0; i < n; ++i)
    str[i] = *ptr++;
  return *this;
}

// ----------------------------------------------------------------------

// Replace a file extension.
string replace_extension (const string oldfilename, const string newext)
{
  size_t dot_ofs = oldfilename.rfind('.');
  if (dot_ofs == string::npos)
    return oldfilename + newext;
  string newfilename(oldfilename);
  return newfilename.replace(dot_ofs, string::npos, newext);
}

// Convert a basic Byfl table to HDF5 format.
void convert_basic_table (ByflFile& byflfile, H5File& hdf5file)
{
  // Create a global dataspace.
  hsize_t current_dims = 0;
  hsize_t max_dims = H5S_UNLIMITED;
  DataSpace global_dataspace(1, &current_dims, &max_dims);

  // Define a variable-length string datatype.
  StrType strtype(PredType::C_S1, H5T_VARIABLE);

  // Read the table name.  This will be used as the name of an HDF5 dataset.
  string tablename;
  byflfile >> tablename;

  // Read the entire Byfl column header (column names and types).
  typedef pair<string, BINOUT_COL_T> byfl_column_t;
  vector<byfl_column_t> byfl_column_header;
  size_t datatype_bytes = 0;      // Number of bytes in the compound datatype
  uint8_t column_tag;             // Type of Byfl column
  for (byflfile >> column_tag; column_tag != BINOUT_COL_NONE; byflfile >> column_tag) {
    switch (column_tag) {
      case BINOUT_COL_UINT64:
        datatype_bytes += sizeof(uint64_t);
        break;

      case BINOUT_COL_STRING:
        datatype_bytes += sizeof(char*);
        break;

      default:
        cerr << progname << ": Unexpected column type " << column_tag
             << " encountered" << endl
             << die;
        break;
    }
    string column_name;
    byflfile >> column_name;
    byfl_column_header.push_back(byfl_column_t(column_name, BINOUT_COL_T(column_tag)));
  }

  // Construct an HDF5 datatype based on the Byfl column header.
  CompType datatype(datatype_bytes);
  size_t byte_offset = 0;    // Running total of bytes consumed by each element
  for (auto iter = byfl_column_header.cbegin();
       iter != byfl_column_header.cend();
       ++iter) {
    string colname = iter->first;
    BINOUT_COL_T coltype = iter->second;
    switch (coltype) {
      case BINOUT_COL_UINT64:
        datatype.insertMember(colname, byte_offset, PredType::STD_U64BE);
        byte_offset += sizeof(uint64_t);
        break;

      case BINOUT_COL_STRING:
        datatype.insertMember(colname, byte_offset, strtype);
        byte_offset += sizeof(char*);
        break;

      default:
        cerr << progname << ": Internal error at "
             << __FILE__ << ':' << __LINE__ << endl
             << die;
        break;
    }
  }

  // Create a dataset.  Enable chunking (required because of the
  // H5S_UNLIMITED dimension) and deflate compression (optional).
  DSetCreatPropList proplist;
  proplist.setChunk(1, &chunk_size);
  proplist.setDeflate(9);    // Maximal compression
  DataSet dataset = hdf5file.createDataSet(tablename, datatype, global_dataspace, proplist);

  // Define a single-row memory dataspace.
  hsize_t num_rows = 1;
  DataSpace mem_dataspace(1, &num_rows);

  // Write each row of Byfl data in turn to the HDF5 file.
  uint8_t row_tag;             // Type of Byfl row
  for (byflfile >> row_tag; row_tag != BINOUT_ROW_NONE; byflfile >> row_tag) {
    // Alocate space for the row data.
    vector<uint8_t> row_data;           // Raw data comprising the row
    row_data.reserve(datatype_bytes);

    // Read the entire row's raw data into row_data.
    for (auto iter = byfl_column_header.cbegin();
         iter != byfl_column_header.cend();
         ++iter) {
      BINOUT_COL_T coltype = iter->second;
      switch (coltype) {
        case BINOUT_COL_UINT64:
          // Push a big-endian unsigned 64-bit integer.
          for (int i = 0; i < sizeof(uint64_t); ++i) {
            uint8_t onebyte;
            byflfile >> onebyte;
            row_data.push_back(onebyte);
          }
          break;

        case BINOUT_COL_STRING:
          // Push a pointer to a C string.
          {
            string str;
            byflfile >> str;
            const char* sptr = strdup(str.c_str());         // String pointer
            const uint8_t* sptrp = (const uint8_t*) &sptr;  // Address of the above, cast to an array of bytes
            for (int i = 0; i < sizeof(uint8_t *); ++i)
              row_data.push_back(sptrp[i]);
          }
          break;

        default:
          cerr << progname << ": Internal error at "
               << __FILE__ << ':' << __LINE__ << endl
               << die;
          break;
      }
    }

    // Extend the dataset by one row.
    current_dims++;
    dataset.extend(&current_dims);

    // Create a file dataspace.
    DataSpace file_dataspace = dataset.getSpace();

    // Point the file dataspace to the last row of the dataset.
    hsize_t data_offset = current_dims - 1;
    file_dataspace.selectHyperslab(H5S_SELECT_SET, &num_rows, &data_offset);

    // Write the raw row data to the dataspace.
    uint8_t* raw_row_data = row_data.data();
    dataset.write(raw_row_data, datatype, mem_dataspace, file_dataspace);

    // Free the memory consumed by all of the C strings.
    uint8_t* rawptr = raw_row_data;    // Pointer into the raw data
    for (auto iter = byfl_column_header.cbegin();
         iter != byfl_column_header.cend();
         ++iter) {
      BINOUT_COL_T coltype = iter->second;
      switch (coltype) {
        case BINOUT_COL_UINT64:
          rawptr += sizeof(uint64_t);
          break;

        case BINOUT_COL_STRING:
          {
            char* charptr = *(char**) rawptr;
            free((void*) charptr);
            rawptr += sizeof(char*);
          }
          break;

        default:
          cerr << progname << ": Internal error at "
               << __FILE__ << ':' << __LINE__ << endl
               << die;
          break;
      }
    }
  }
}

// Convert a Byfl binary output file to an HDF5 file.
void convert_byfl_to_hdf5 (string byflfilename, string hdf5filename)
{
  // Open both the input and the output file.
  ByflFile byflfile(byflfilename);
  H5File hdf5file(hdf5filename, H5F_ACC_TRUNC);
  char fileMagic[7];
  byflfile.read(fileMagic, 7);
  if (memcmp(fileMagic, "BYFLBIN", 7) != 0)
    cerr << progname << ": File " << byflfilename
         << " does not appear to contain Byfl binary output\n"
         << die;

  // Process in turn each table we encounter.
  uint8_t tabletype;
  for (byflfile >> tabletype; tabletype != BINOUT_TABLE_NONE; byflfile >> tabletype)
    switch (tabletype) {
      case BINOUT_TABLE_BASIC:
        convert_basic_table(byflfile, hdf5file);
        break;

      default:
        cerr << progname << ": Unexpected table type " << tabletype
             << " found in " << byflfilename << endl
             << die;
        break;
    }
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

  // Convert the file format.
  convert_byfl_to_hdf5(byflfilename, h5filename);
  return 0;
}

/*
 * Helper library for computing bytes:flops ratios
 * (tracking operations by data structure)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

namespace bytesflops {

// Define the types of strides we intend to track.
#define MAX_POW2_STRIDE 6                  // log_2 of the maximum word stride to track precisely
#define ZERO_STRIDE (MAX_POW2_STRIDE + 1)  // Zero word stride
#define OTHER_STRIDE (ZERO_STRIDE + 1)     // Non-zero and non-power-of-two word stride
#define NUM_STRIDES (OTHER_STRIDE + 1)     // Array elements to allocate for all of the above

// Track a single call point's data-access pattern.
class AccessPattern {
public:
  bf_symbol_info_t syminfo;             // Call-point source information
  uint64_t prev_addr;                   // Previous data address
  uint64_t num_bytes;                   // Bytes per access (i.e., word size)
  uint64_t stride_tally[NUM_STRIDES];   // Tally by word stride
  uint64_t backward_strides;            // Tally of backward strides, any distance
  uint64_t total_strides;               // Sum across stride_tally[]
  bool is_store;                        // true=store; false=load

  // Initialize an AccessPattern with all zero tallies.
  AccessPattern(bf_symbol_info_t sinfo, uint64_t addr, uint64_t nbytes, bool st) :
    syminfo(sinfo), prev_addr(addr), num_bytes(nbytes),  backward_strides(0),
    total_strides(0), is_store(st) {
    memset(stride_tally, 0, NUM_STRIDES*sizeof(uint64_t));
  }

  // Given a new address, increment the appropriate stride tally.
  void increment_tally(uint64_t new_addr) {
    // Increase the total number of strides observed.
    total_strides++;

    // Check for a zero stride.
    if (new_addr == prev_addr) {
      stride_tally[ZERO_STRIDE]++;
      return;
    }

    // Tally the number of backward strides.
    if (prev_addr > new_addr)
      backward_strides++;

    // Check for a non-multiple of the word size.
    uint64_t abs_stride = uint64_t(abs(int64_t(new_addr) - int64_t(prev_addr)));
    if (abs_stride % num_bytes != 0) {
      stride_tally[OTHER_STRIDE]++;
      return;
    }

    // Convert from a byte stride to a word stride.
    abs_stride /= num_bytes;

    // Check if the word stride is a power of two.
    if ((abs_stride & (abs_stride - 1)) == 0) {
      size_t log2_stride = 0;
      while (abs_stride >>= 1)
        log2_stride++;
      if (log2_stride <= MAX_POW2_STRIDE)
        stride_tally[log2_stride]++;
      else
        stride_tally[OTHER_STRIDE]++;
      return;
    }

    // Categorize as "other".
    stride_tally[OTHER_STRIDE]++;
  }
};

// Define this file's main data structure.
static CachedOrderedMap<uint64_t, AccessPattern*>* stride_data;  // Map from a location ID to stride information.

// Gain access to our binary output stream.
extern BinaryOStream* bfbin;

// Initialize our internal data structure.
void initialize_strides (void)
{
  if (stride_data == nullptr)
    stride_data = new CachedOrderedMap<uint64_t, AccessPattern*>;
}

// Track a call point's strided access pattern.
extern "C"
void bf_track_stride (bf_symbol_info_t* syminfo, uint64_t baseaddr,
                      uint64_t numaddrs, uint8_t load0store1)
{
  // Determine if we've previously seen this call point.
  auto iter = stride_data->find(syminfo->ID);
  if (iter == stride_data->end()) {
    // First access from this call point: Create a new tally entry.
    (*stride_data)[syminfo->ID] = new AccessPattern(*syminfo, baseaddr, numaddrs, bool(load0store1));
    return;
  }

  // We've seen this call point before.  Determine the new stride and update
  // our information accordingly.
  AccessPattern* info = iter->second;
  info->increment_tally(baseaddr);
  info->prev_addr = baseaddr;
}

// This function is used by sort() to sort stride information in decreasing
// order of the total number of strides observed.  Break ties using the
// filename, then line number, and finally the instruction string, all in
// increasing order.
static bool compare_total_strides (const AccessPattern* one,
                                   const AccessPattern* two)
{
  if (two->total_strides != one->total_strides)
    return two->total_strides < one->total_strides;
  int fname_diff = strcmp(one->syminfo.file, two->syminfo.file);
  if (fname_diff != 0)
    return fname_diff < 0;
  if (one->syminfo.line != two->syminfo.line)
    return one->syminfo.line < two->syminfo.line;
  return strcmp(one->syminfo.origin, two->syminfo.origin) < 0;
}

// Output strides by call point.
void bf_report_strides_by_call_point (void)
{
  // Output a binary table header.
  *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Strided accesses";
  *bfbin << uint8_t(BINOUT_COL_STRING) << "Instruction"
         << uint8_t(BINOUT_COL_UINT64) << "Word size"
         << uint8_t(BINOUT_COL_BOOL)   << "Load"
         << uint8_t(BINOUT_COL_STRING) << "Demangled symbol reference"
         << uint8_t(BINOUT_COL_STRING) << "Mangled function name"
         << uint8_t(BINOUT_COL_STRING) << "Demangled function name"
         << uint8_t(BINOUT_COL_STRING) << "File name"
         << uint8_t(BINOUT_COL_UINT64) << "Line number"
         << uint8_t(BINOUT_COL_UINT64) << "0 word strides";
  for (size_t i = 0; i <= MAX_POW2_STRIDE; i++) {
    string label = to_string(1 << i) + string(" word strides");
    *bfbin << uint8_t(BINOUT_COL_UINT64) << label;
  }
  *bfbin << uint8_t(BINOUT_COL_UINT64) << "Other strides"
         << uint8_t(BINOUT_COL_UINT64) << "Total backward strides"
         << uint8_t(BINOUT_COL_NONE);

  // Sort the stride information in decreasing order of invocation count.
  vector<AccessPattern*> access_pats;
  for (auto iter = stride_data->begin(); iter != stride_data->end(); iter++)
    access_pats.push_back(iter->second);
  sort(access_pats.begin(), access_pats.end(), compare_total_strides);

  // Output all the information we have.
  for (auto iter = access_pats.begin(); iter != access_pats.end(); iter++) {
    AccessPattern* info = *iter;
    bf_symbol_info_t* syminfo = &info->syminfo;
    string reference(demangle_func_name(syminfo->origin));
    size_t refpos = reference.find(" referencing ");
    if (refpos == string::npos)
      reference = "";
    else
      reference.erase(0, refpos + 13);
    *bfbin << uint8_t(BINOUT_ROW_DATA)
           << syminfo->origin
           << info->num_bytes
           << !info->is_store
           << reference
           << syminfo->function
           << demangle_func_name(syminfo->function)
           << (strcmp(syminfo->file, "??") == 0 ? "" : syminfo->file)
           << uint64_t(syminfo->line)
           << info->stride_tally[ZERO_STRIDE];
    for (size_t i = 0; i <= MAX_POW2_STRIDE; i++)
      *bfbin << info->stride_tally[i];
    *bfbin << info->stride_tally[OTHER_STRIDE]
           << info->backward_strides;
  }
  *bfbin << uint8_t(BINOUT_ROW_NONE);
}

} // namespace bytesflops

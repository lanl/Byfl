/*
 * Helper library for computing bytes:flops ratios
 * (tracking operations by data structure)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"

using namespace std;

// Define an {ID, tag} pair.
class ID_tag {
public:
  uint64_t ID;     // Unique call-point ID
  string tag;      // User-specified tag

  ID_tag(uint64_t i=0, string t="") : ID(i), tag(t) { }

  bool operator==(const ID_tag& other) const {
    return ID == other.ID && tag == other.tag;
  }
};
namespace std {
  template <>
  struct hash<ID_tag> {
    size_t operator()(const ID_tag& idt) const {
      return hash<uint64_t>()(idt.ID) ^ hash<string>()(idt.tag);
    }
  };
}

namespace bytesflops {

extern ostream* bfout;
extern BinaryOStream* bfbin;
static bool output_ds_tags = false;  // true: user called bf_tag_data_region() at least once; false=no calls
static uint64_t dstruct_time = 1;    // Current allocation "time"

// Define an interval-tree type.
template<typename T>
class Interval
{
public:
  T lower;    // Lower limit
  T upper;    // Upper limit

  Interval() { }

  Interval(T ll, T ul) : lower(ll), upper(ul) {
  }

  bool operator<(const Interval<T>& other) const {
    return upper < other.lower;
  }

  bool operator>(const Interval<T>& other) const {
    return lower > other.upper;
  }

  // Equality is defined as not less than and not greater than.
  bool operator==(const Interval<T>& other) const {
    return !(*this < other) && !(*this > other);
  }

  friend ostream& operator<<(ostream& os, const Interval<T>& val) {
    os << '[' << val.lower << ", " << val.upper << ']';
    return os;
  }
};

// Define all of the counters and other information we keep track of
// per data structure.
class DataStructCounters
{
public:
  bf_symbol_info_t syminfo;   // Dynamic data structure source information
  uint64_t current_size = 0;  // Current total memory footprint in bytes
  uint64_t max_size = 0;      // Largest memory footprint in bytes ever seen
  uint64_t bytes_loaded = 0;  // Number of bytes loaded
  uint64_t bytes_stored = 0;  // Number of bytes stored
  uint64_t load_ops = 0;      // Number of load operations
  uint64_t store_ops = 0;     // Number of store operations
  bool allocation = true;     // true=known allocation; false=access (unknown allocation)
  uint64_t bytes_alloced = 0; // Total number of bytes allocated (always >= max_size)
  uint64_t num_allocs = 0;    // Number of allocation calls
  string tag = "";            // User-specified tag
  uint64_t alloc_time = 0;    // Allocation "time" on a global counter
  uint64_t access1_time = 0;  // First access "time" on a global counter
  uint64_t accessN_time = 0;  // Last access "time" on a global counter
  uint64_t free_time = 0;     // Deallocation "time" on a global counter

  // The minimum we need to initialize are the data structure's initial size
  // (which can grow), symbol information, and whether the data structure comes
  // from an explicit allocation or an access to an unknown address.
  DataStructCounters(bf_symbol_info_t sinfo, uint64_t sz, bool alloc) :
    syminfo(sinfo), current_size(sz), max_size(sz), allocation(alloc),
    bytes_alloced(sz), num_allocs(1), tag(""),
    access1_time(0), accessN_time(0), free_time(0)
  {
    alloc_time = dstruct_time++;
  }

  // Generate a description of a data structure.
  string generate_symbol_desc(void) const;
};

// Generate a description of a data structure.
string DataStructCounters::generate_symbol_desc (void) const
{
  stringstream locstr;
  bool is_global = strcmp(syminfo.function, "*GLOBAL*") == 0;
  if (syminfo.symbol[0] == '[')
    locstr << (allocation ? "Unnamed" : "Unknown") << " data structure";
  else
    locstr << "Variable " << demangle_func_name(syminfo.symbol);
  if (tag != "")
    locstr << " with tag \"" << tag << '"';
  if (is_global) {
    if (strcmp(syminfo.file, "??") != 0)
      locstr << " declared";
  }
  else
    locstr << (allocation ? " allocated in" : " accessed in")
           << ' ' << demangle_func_name(syminfo.function);
  if (strcmp(syminfo.file, "??") != 0) {
    locstr << " at " << syminfo.file;
    if (syminfo.line > 0)
      locstr << ':' << syminfo.line;
  }
  return locstr.str();
}

// Define this file's two main data structures.
static CachedOrderedMap<Interval<uint64_t>, DataStructCounters*>* data_structs;  // Interval tree with information about each data structure
static CachedUnorderedMap<ID_tag, DataStructCounters*>* id_tag_to_counters;  // Map from a symbol identifier to data-structure counters

// Construct an interval tree of symbol addresses.
void initialize_data_structures (void)
{
  if (data_structs != nullptr)
    return;    // Already initialized
  data_structs = new CachedOrderedMap<Interval<uint64_t>, DataStructCounters*>;
  id_tag_to_counters = new CachedUnorderedMap<ID_tag, DataStructCounters*>;
}

// Disassociate a range of previously allocated addresses (given the address
// at the beginning of the range) from the data structure to which it used to
// belong.  Return the address to the right of the disassociated range.
static void* disassoc_addresses_with_dstruct (void* baseptr)
{
  // Find the address interval and set of counters.
  static Interval<uint64_t> search_addr(0, 0);
  search_addr.lower = search_addr.upper = uint64_t(uintptr_t(baseptr));
  auto iter = data_structs->find(search_addr);
  if (iter == data_structs->end())
    return (void*)((char*)baseptr + 1);  // Address was not previously allocated (or somehow snuck by us).
  Interval<uint64_t> interval = iter->first;
  DataStructCounters* counters = iter->second;

  // Reduce the size of the data structure by the size of the address range and
  // break the link from the address interval to the counters.  Note that
  // id_tag_to_counters still points to the counters; we don't want to forget
  // that the data structure ever existed just because it was deallocated.
  uint64_t interval_length = interval.upper - interval.lower + 1;
  counters->current_size -= interval_length;
  if (counters->current_size == 0)
    counters->free_time = dstruct_time;
  dstruct_time++;      // Deallocation is an event, even if we haven't freed the entire data structure.
  data_structs->erase(interval);
  return (void *)(interval.upper + 1);
}

// For access from user code, wrap disassoc_addresses_with_dstruct() and
// discard the return value.
extern "C"
void bf_disassoc_addresses_with_dstruct (void* baseptr)
{
  (void) disassoc_addresses_with_dstruct(baseptr);
}

// Associate a range of addresses with a statically allocated data structure.
extern "C"
void bf_assoc_addresses_with_sstruct (const bf_symbol_info_t* syminfo,
                                      void* baseptr, uint64_t numaddrs)
{
  // Convert some of our arguments to slightly different forms.
  uint64_t first_addr = uint64_t(uintptr_t(baseptr));
  uint64_t last_addr = first_addr + numaddrs - 1;
  string symname(syminfo->symbol);

  // Insert the symbol into the interval tree and into the mapping from
  // data-structure name to counters.
  DataStructCounters* info = new DataStructCounters(*syminfo, numaddrs, true);
  dstruct_time--;   // Undo the time increment when we're allocating statically.
  info->alloc_time = 0;   // Static data are always allocated at time 0.
  (*data_structs)[Interval<uint64_t>(first_addr, last_addr)] = info;
  (*id_tag_to_counters)[ID_tag(syminfo->ID)] = info;
}

// Associate a range of addresses with a dynamically allocated data structure.
static void assoc_addresses_with_dstruct (const bf_symbol_info_t* syminfo,
                                          void* old_baseptr, void* baseptr,
                                          uint64_t numaddrs,
                                          bool known_alloc)
{
  // Find an existing set of counters for the same source-code location.  If no
  // such counters exist, allocate a new set.
  DataStructCounters* counters;      // Counters associated with the data structure
  if (old_baseptr == nullptr) {
    // Common case -- we haven't seen the old base address before (because it's
    // presumably the same as the new address, and that's what was just
    // allocated).
    auto count_iter = id_tag_to_counters->find(ID_tag(syminfo->ID));
    if (count_iter == id_tag_to_counters->end()) {
      // Not found -- allocate new counters.
      counters = new DataStructCounters(*syminfo, numaddrs, known_alloc);
      (*id_tag_to_counters)[ID_tag(syminfo->ID)] = counters;
    }
    else {
      // Found -- increment the size of the data structure and its tallies.
      counters = count_iter->second;
      counters->current_size += numaddrs;
      if (counters->current_size > counters->max_size)
        counters->max_size = counters->current_size;
      counters->bytes_alloced += numaddrs;
      counters->num_allocs++;
    }
  }
  else {
    // Case of realloc -- reuse the old counters, but remove the old address
    // range, and subtract off the bytes previously allocated.
    static Interval<uint64_t> search_addr(0, 0);
    search_addr.lower = search_addr.upper = uint64_t(uintptr_t(old_baseptr));
    auto old_iter = data_structs->find(search_addr);
    counters = old_iter->second;
    Interval<uint64_t> old_interval = old_iter->first;
    counters->current_size -= old_interval.upper - old_interval.lower + 1;
    counters->current_size += numaddrs;
    if (counters->current_size > counters->max_size)
      counters->max_size = counters->current_size;
    counters->bytes_alloced += numaddrs;
    counters->num_allocs++;
    data_structs->erase(old_interval);
  }

  // Associate the new range of addresses with the old (or just created)
  // counters.
  uint64_t baseaddr = uint64_t(uintptr_t(baseptr));
  Interval<uint64_t> ival(baseaddr, baseaddr + numaddrs - 1);
  (*data_structs)[ival] = counters;
}

// Associate a range of addresses with a dynamically allocated data structure.
extern "C"
void bf_assoc_addresses_with_dstruct (const bf_symbol_info_t* syminfo,
                                      void* old_baseptr, void* baseptr,
                                      uint64_t numaddrs)
{
  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Associate the given addresses with the data structure.
  assoc_addresses_with_dstruct(syminfo, old_baseptr, baseptr, numaddrs, true);
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically by posix_memalign().
extern "C"
void bf_assoc_addresses_with_dstruct_pm (const bf_symbol_info_t* syminfo,
                                         void* old_baseptr, void** baseptrptr,
                                         uint64_t numaddrs, int retcode)
{
  // Ignore this data structure if posix_memalign() failed.
  if (retcode != 0)
    return;

  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Associate the given addresses with the data structure.
  assoc_addresses_with_dstruct(syminfo, old_baseptr, *baseptrptr, numaddrs, true);
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically on the stack.
extern "C"
void bf_assoc_addresses_with_dstruct_stack (const bf_symbol_info_t* syminfo,
                                            void* baseptr, uint64_t numaddrs)
{
  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Disassociate all overlapping data structures.  For example if a function
  // declares "int32_t x,y;" then returns, then another function delcares
  // "int64_t foo;" and gets the same base address as x, we'll need to
  // associate foo's address range with foo from now on, not with x and y.
  void* lastaddr = (void*)((char*)baseptr + numaddrs);
  for (void* addr = baseptr;
       addr < lastaddr;
       addr = disassoc_addresses_with_dstruct(addr))
    ;

  // Associate the given addresses with the data structure.
  assoc_addresses_with_dstruct(syminfo, nullptr, baseptr, numaddrs, true);
}

// Increment access counts for a data structure.
extern "C"
void bf_access_data_struct (const bf_symbol_info_t* syminfo, uint64_t baseaddr,
                            uint64_t numaddrs, uint8_t load0store1)
{
  // Do nothing if counting is suppressed.
  if (bf_suppress_counting)
    return;

  // Find the interval containing the base address.  Use a set of counts
  // representing unknown data structures if we failed to find an interval.
  static Interval<uint64_t> search_addr(0, 0);
  search_addr.lower = search_addr.upper = baseaddr;
  DataStructCounters* counters;
  auto iter = data_structs->find(search_addr);
  if (iter == data_structs->end()) {
    // The data structure wasn't found.  For example, it was allocated by a
    // non-Byfl-instrumented function (say, strdup(), for example).  "Allocate"
    // it so it'll be found the next time.
    void* lastaddr = (void*)(baseaddr + numaddrs);
    for (void* addr = (void*)(uintptr_t(baseaddr));
         addr < lastaddr;
         addr = disassoc_addresses_with_dstruct(addr))
      ;

    // "Allocate" an unknown data structure.
    assoc_addresses_with_dstruct(syminfo, nullptr, (void*)uintptr_t(baseaddr),
                                 numaddrs, false);
    iter = data_structs->find(search_addr);
  }
  counters = iter->second;

  // Increment the appropriate counters.
  if (iter == data_structs->end())
    abort();    // Internal error searching data_structs (bad interval?)
  if (load0store1 == 0) {
    counters->load_ops++;
    counters->bytes_loaded += numaddrs;
  }
  else {
    counters->store_ops++;
    counters->bytes_stored += numaddrs;
  }
  if (counters->access1_time == 0)
    counters->access1_time = dstruct_time;
  counters->accessN_time = dstruct_time++;
}

// Associate an arbitrary tag with a fragment of a data structure, given an
// address within an interval.
extern "C"
void bf_tag_data_region (void* address, const char *tag)
{
  // Find the data structure associated with the given address.
  static Interval<uint64_t> search_addr(0, 0);
  search_addr.lower = search_addr.upper = uint64_t(uintptr_t(address));
  auto diter = data_structs->find(search_addr);
  if (diter == data_structs->end())
    return;
  DataStructCounters* old_counters = diter->second;
  uint64_t id = old_counters->syminfo.ID;

  // Find the set of counters associated with the symbol ID and tag.  If no
  // such set exists, create a new one.
  DataStructCounters* new_counters;
  auto titer = id_tag_to_counters->find(ID_tag(id, tag));
  if (titer == id_tag_to_counters->end()) {
    // Create a new set of counters.
    new_counters = new DataStructCounters(old_counters->syminfo, 0, old_counters->allocation);
    new_counters->num_allocs = 0;   // This will be incremented below.
    new_counters->tag = tag;
    (*id_tag_to_counters)[ID_tag(id, tag)] = new_counters;
    output_ds_tags = true;
  }
  else
    new_counters = titer->second;

  // Transfer allocation values (but not load/store counters) from the old
  // counters to the new counters.
  uint64_t numaddrs = diter->first.upper - diter->first.lower + 1;
  old_counters->num_allocs--;
  new_counters->num_allocs++;
  old_counters->bytes_alloced -= numaddrs;
  new_counters->bytes_alloced += numaddrs;
  old_counters->current_size -= numaddrs;
  new_counters->current_size += numaddrs;
  if (old_counters->current_size < old_counters->max_size)
    old_counters->max_size = old_counters->current_size;
  if (new_counters->current_size > new_counters->max_size)
    new_counters->max_size = new_counters->current_size;

  // Associate the original address interval with the new set of counters.
  (*data_structs)[diter->first] = new_counters;
}

// Compare two counters with the intention of sorted them in decreasing
// order of interestingness.  To that end, we sort first by decreasing
// access count, then by decreasing memory footprint, then by increasing
// data-structure name, and finally by increasing allocation function.
static bool compare_counter_interest (const DataStructCounters* a,
                                      const DataStructCounters* b)
{
  uint64_t a_accesses = a->bytes_loaded + a->bytes_stored;
  uint64_t b_accesses = b->bytes_loaded + b->bytes_stored;

  if (a_accesses != b_accesses)
    return a_accesses > b_accesses;
  if (a->max_size != b->max_size)
    return a->max_size > b->max_size;
  int name_comp = strcmp(a->syminfo.symbol, b->syminfo.symbol);
  if (name_comp != 0)
    return name_comp < 0;
  return strcmp(a->syminfo.origin, b->syminfo.origin) < 0;
}

// Output load and store counters by data structure.
void bf_report_data_struct_counts (void)
{
  // Sort all data structures in the interval tree by decreasing order
  // of total bytes accessed.  Ignore any unaccessed data structures.
  vector<DataStructCounters*> interesting_data;
  for (auto iter = id_tag_to_counters->begin(); iter != id_tag_to_counters->end(); iter++) {
    DataStructCounters* counters = iter->second;
    if (counters->bytes_loaded + counters->bytes_stored > 0)
      interesting_data.push_back(counters);
  }
  sort(interesting_data.begin(), interesting_data.end(), compare_counter_interest);

  // Output a textual header line.
  *bfout << "BYFL_DATA_STRUCT_HEADER: "
         << setw(20) << "Size" << ' '
         << setw(20) << "LD_bytes" << ' '
         << setw(20) << "ST_bytes" << ' '
         << setw(20) << "LD_ops" << ' '
         << setw(20) << "ST_ops" << ' '
         << setw(29) << left << "Origin" << internal << ' '
         << "Description" << '\n';

  // Output a binary table header.
  *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Data-structure accesses";
  *bfbin << uint8_t(BINOUT_COL_UINT64) << "Number of allocations"
         << uint8_t(BINOUT_COL_UINT64) << "Total bytes allocated"
         << uint8_t(BINOUT_COL_UINT64) << "Maximum memory footprint"
         << uint8_t(BINOUT_COL_UINT64) << "First allocation time"
         << uint8_t(BINOUT_COL_UINT64) << "First access time"
         << uint8_t(BINOUT_COL_UINT64) << "Last access time"
         << uint8_t(BINOUT_COL_UINT64) << "Last deallocation time"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes stored"
         << uint8_t(BINOUT_COL_UINT64) << "Load operations"
         << uint8_t(BINOUT_COL_UINT64) << "Store operations"
         << uint8_t(BINOUT_COL_BOOL)   << "Known allocation point"
         << uint8_t(BINOUT_COL_STRING) << "Mangled origin"
         << uint8_t(BINOUT_COL_STRING) << "Demangled origin";
  if (output_ds_tags)
    *bfbin << uint8_t(BINOUT_COL_STRING) << "Tag";
  *bfbin << uint8_t(BINOUT_COL_STRING) << "Mangled variable name"
         << uint8_t(BINOUT_COL_STRING) << "Demangled variable name"
         << uint8_t(BINOUT_COL_STRING) << "Mangled function name"
         << uint8_t(BINOUT_COL_STRING) << "Demangled function name"
         << uint8_t(BINOUT_COL_STRING) << "File name"
         << uint8_t(BINOUT_COL_UINT64) << "Line number"
         << uint8_t(BINOUT_COL_STRING) << "Description"
         << uint8_t(BINOUT_COL_NONE);

  // Output both textual and binary data.
  for (auto iter = interesting_data.cbegin(); iter != interesting_data.cend(); iter++) {
    // Demangle the origin name.
    const DataStructCounters* counters = *iter;
    const bf_symbol_info_t* syminfo = &counters->syminfo;
    string demangled_origin(demangle_func_name(syminfo->origin));
    string short_demangled_origin(demangled_origin);
    size_t refpos = short_demangled_origin.find(" referencing ");
    if (refpos != string::npos)
      short_demangled_origin.erase(refpos);
    const string description = counters->generate_symbol_desc();

    // Output textual data-structure information.
    *bfout << "BYFL_DATA_STRUCT:        "
           << setw(20) << counters->max_size << ' '
           << setw(20) << counters->bytes_loaded << ' '
           << setw(20) << counters->bytes_stored << ' '
           << setw(20) << counters->load_ops << ' '
           << setw(20) << counters->store_ops << ' '
           << setw(29) << left << short_demangled_origin << internal << ' '
           << description << '\n';

    // Output binary data-structure information.
    *bfbin << uint8_t(BINOUT_ROW_DATA)
           << counters->num_allocs
           << counters->bytes_alloced
           << counters->max_size
           << counters->alloc_time
           << counters->access1_time
           << counters->accessN_time
           << (counters->free_time == 0 ? dstruct_time : counters->free_time)
           << counters->bytes_loaded
           << counters->bytes_stored
           << counters->load_ops
           << counters->store_ops
           << counters->allocation
           << string(syminfo->origin)
           << demangled_origin;
    if (output_ds_tags)
      *bfbin << counters->tag;
    *bfbin << (string(syminfo->symbol[0] == '[' ? "" : syminfo->symbol))
           << (string(syminfo->symbol[0] == '[' ? "" : demangle_func_name(syminfo->symbol)))
           << (strcmp(syminfo->function, "*GLOBAL*") == 0 ? "" : syminfo->function)
           << (strcmp(syminfo->function, "*GLOBAL*") == 0 ? "" : demangle_func_name(syminfo->function))
           << (strcmp(syminfo->file, "??") == 0 ? "" : syminfo->file)
           << uint64_t(syminfo->line)
           << description;
  }
  *bfbin << uint8_t(BINOUT_ROW_NONE);
}

} // namespace bytesflops

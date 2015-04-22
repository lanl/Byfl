/*
 * Helper library for computing bytes:flops ratios
 * (tracking operations by data structure)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"
#ifdef USE_BFD
# include "findsrc.h"
#endif

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

namespace bytesflops {

extern ostream* bfout;
extern BinaryOStream* bfbin;
#ifdef USE_BFD
extern ProcessSymbolTable* procsymtab;  // The calling process's symbol table
#endif

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
  void* alloc_addr = nullptr;       // Instruction address that allocated this data structure
  const char *var_prefix = nullptr; // String with which to prefix the data structure's name
  string name;                // Name of this data structure
  string demangled_name;      // Same as the above but demangled
  uint64_t current_size = 0;  // Current total memory footprint in bytes
  uint64_t max_size = 0;      // Largest memory footprint in bytes ever seen
  string origin;              // Who allocated the memory
  uint64_t bytes_loaded = 0;  // Number of bytes loaded
  uint64_t bytes_stored = 0;  // Number of bytes stored
  uint64_t load_ops = 0;      // Number of load operations
  uint64_t store_ops = 0;     // Number of store operations
  uint64_t bytes_alloced = 0; // Total number of bytes allocated (always >= max_size)
  uint64_t num_allocs = 0;    // Number of allocation calls

  // For a static data structure, the minimum we need to initialize are the
  // mangled and demangled names of the data structure, its size, and its
  // origin.
  DataStructCounters(string nm, string dnm, uint64_t sz, string org) :
    name(nm), demangled_name(dnm), current_size(sz), max_size(sz), origin(org),
    bytes_alloced(sz), num_allocs(1)
  {
  }

  // For a dynamic data structure, the minimum we need to initialize are the
  // allocating instruction address and the data structure's string prefix,
  // initial size (which can grow), and origin.
  DataStructCounters(void* aaddr, const char *vpref, uint64_t sz, string org) :
    alloc_addr(aaddr), var_prefix(vpref), current_size(sz), max_size(sz),
    origin(org), bytes_alloced(sz), num_allocs(1)
  {
  }

  // Generate a symbol name for a dynamic data structure.
  void generate_symbol_name(void);
};

// Generate a symbol name for a dynamic data structure.
void DataStructCounters::generate_symbol_name (void)
{
#ifdef HAVE_BACKTRACE
  // Do nothing if we already have a name or if we don't know where we're
  // coming from.
  if (name != "" || alloc_addr == nullptr)
    return;

  // Derive the symbol name from the allocation location and the specified
  // prefix.
  string allocated_text(origin == "unknown" ? " accessed at " : " allocated at ");
# ifdef USE_BFD
  // If we have the BFD library, use that to get a more precise
  // source-code location.
  SourceCodeLocation* srcloc = procsymtab->find_address((uintptr_t)alloc_addr);
  if (srcloc == nullptr) {
    // Location wasn't found -- use the address instead.
    char *alloc_loc = new char[100];
    sprintf(alloc_loc, (string(var_prefix) + allocated_text + "%p").c_str(), alloc_addr);
    name = demangled_name = alloc_loc;
    delete[] alloc_loc;
  }
  else {
    // Location was found -- format it and use it.
    stringstream locstr;
    locstr << var_prefix << allocated_text
           << srcloc->file_name << ':' << srcloc->line_number
           << ", function " << srcloc->function_name
           << ", address " << hex << alloc_addr << dec;
    name = locstr.str();
    locstr.str(string());
    locstr << var_prefix << allocated_text
           << srcloc->file_name << ':' << srcloc->line_number
           << ", function " << demangle_func_name(srcloc->function_name)
           << ", address " << hex << alloc_addr << dec;
    demangled_name = locstr.str();
  }
# else
  const char* alloc_loc = bf_address_to_location_string(alloc_addr);
  if (!strcmp(alloc_loc, "??:0")) {
    // Location wasn't found -- use the address instead.
    char *alt_alloc_loc = new char[100];
    sprintf(alt_alloc_loc, (string(var_prefix) + allocated_text + "%p").c_str(), alloc_addr);
    name = demangled_name = alt_alloc_loc;
    delete[] alt_alloc_loc;
  }
  else
    // Location was found -- use it.
    name = demangled_name = string(var_prefix) + allocated_text + alloc_loc;
# endif
#endif
}

// Define this file's two main data structures.
static CachedOrderedMap<Interval<uint64_t>, DataStructCounters*>* data_structs;  // Interval tree with information about each data structure
static CachedUnorderedMap<void*, DataStructCounters*>* location_to_counters;  // Map from a source-code location to data-structure counters

// Construct an interval tree of symbol addresses.  If the BFD library
// isn't available we proceed without information about statically
// allocated data structures.
void initialize_data_structures (void)
{
  data_structs = new CachedOrderedMap<Interval<uint64_t>, DataStructCounters*>;
  location_to_counters = new CachedUnorderedMap<void*, DataStructCounters*>;
#ifdef USE_BFD
  // I don't know if there's a more automatic way to find symbol
  // lengths, but the following seems to work: Sort all symbols by
  // value (starting address) then use deltas between values as the
  // symbol length.
  bfd* bfd_self;
  asymbol** symtable;
  ssize_t numsyms = 0;

  // Read the symbol table into a vector.
  procsymtab->get_raw_bfd_data(&bfd_self, &symtable, &numsyms);
  typedef tuple<uint64_t, string, string> addr_sym_sec_t ;   // Base address, name, and section of a symbol
  vector<addr_sym_sec_t> local_symtable;    // Sortable version of the symbol table
  for (ssize_t i = 0; i < numsyms; i++) {
    asymbol* thissym = symtable[i];
    asection* thissect = bfd_get_section(thissym);
    local_symtable.push_back(addr_sym_sec_t(bfd_asymbol_value(thissym),
                                            bfd_asymbol_name(thissym),
                                            bfd_get_section_name(bfd_self, thissect)));
  }

  // Add a dummy symbol at the end of each section.
  for (asection* sect = bfd_self->sections; sect != NULL ; sect = sect->next) {
    bfd_vma vma = bfd_get_section_vma(bfd_self, sect);
    bfd_size_type vma_size = bfd_get_section_size(sect);
    string section_name(bfd_get_section_name(bfd_self, sect));
    local_symtable.push_back(addr_sym_sec_t(vma + vma_size,
                                            string("*DUMMY END* ") + section_name,
                                            section_name));
  }

  // Sort the symbols by starting address (VMA).
  sort(local_symtable.begin(), local_symtable.end());

  // Populate the data_structs interval tree with symbol information.
  for (auto iter = local_symtable.begin(); iter != local_symtable.end(); iter++) {
    // Extract the symbol name, starting and ending addresses, and section name.
    auto next_iter = iter + 1;
    if (next_iter == local_symtable.end())
      break;
    uint64_t first_addr = get<0>(*iter);
    if (first_addr == get<0>(*next_iter))
      continue;   // Zero-sized data structure
    uint64_t last_addr = get<0>(*next_iter) - 1;
    string symname(get<1>(*iter));
    if (symname.compare(0, 11, "*DUMMY END*") == 0)
      continue;   // Dummy value we inserted above
    if (symname.substr(0, 3) == "bf_")
      continue;   // Byfl-inserted symbol
    string demangled_symname = demangle_func_name(symname);
    string sectname(get<2>(*iter));

    // Insert the symbol into the interval tree and into the mapping from
    // data-structure name to counters.
    DataStructCounters* info =
      new DataStructCounters(string("Static variable ") + symname,
                             string("Static variable ") + demangled_symname,
                             last_addr - first_addr + 1,
                             sectname);
    (*data_structs)[Interval<uint64_t>(first_addr, last_addr)] = info;
    (*location_to_counters)[(void*)uintptr_t(first_addr)] = info;  // Use data address as code-allocation point.
  }
#endif
}

  /*
   * Most of this file requires HAVE_BACKTRACE.  Rather than check for
   * that individually in each function, we check once here and stub
   * all the functions that require backtrace(): everything dealing
   * with dynamic (either heap- or stack-allocated) data structures.
   */
#ifdef HAVE_BACKTRACE

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

  // Reduce the size of the data structure by the size of the address range
  // and break the link from the address interval to the counters.  Note
  // that location_to_counters still points to the counters; we don't want
  // to forget that the data structure ever existed just because it was
  // deallocated.
  uint64_t interval_length = interval.upper - interval.lower + 1;
  counters->current_size -= interval_length;
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

// Associate a range of addresses with a dynamically allocated data structure.
  static void assoc_addresses_with_dstruct (void *caller_addr, const char* origin,
                                            void* old_baseptr, void* baseptr,
                                            uint64_t numaddrs, const char* var_prefix)
{
  // Find an existing set of counters for the same source-code location.  If no
  // such counters exist, allocate a new set.
  DataStructCounters* counters;      // Counters associated with the data structure
  if (old_baseptr == nullptr) {
    // Common case -- we haven't seen the old base address before (because it's
    // presumably the same as the new address, and that's what was just
    // allocated).
    auto count_iter = location_to_counters->find(caller_addr);
    if (count_iter == location_to_counters->end()) {
      // Not found -- allocate new counters.
      counters = new DataStructCounters(caller_addr,
                                        bf_string_to_symbol(var_prefix),
                                        numaddrs, origin);
      (*location_to_counters)[caller_addr] = counters;
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
void bf_assoc_addresses_with_dstruct (const char* origin, void* old_baseptr,
                                      void* baseptr, uint64_t numaddrs)
{
  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Ignore this data structure if we don't know where we're coming from.
  void* caller_addr = bf_find_caller_address();
  if (caller_addr == NULL)
    return;

  // Associate the caller's address with the data structure.
  assoc_addresses_with_dstruct(caller_addr, origin, old_baseptr, baseptr, numaddrs, "Data");
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically by posix_memalign().
extern "C"
void bf_assoc_addresses_with_dstruct_pm (const char* origin, void* old_baseptr,
                                         void** baseptrptr, uint64_t numaddrs, int retcode)
{
  // Ignore this data structure if posix_memalign() failed.
  if (retcode != 0)
    return;

  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Ignore this data structure if we don't know where we're coming from.
  void* caller_addr = bf_find_caller_address();
  if (caller_addr == NULL)
    return;

  // Associate the caller's address with the data structure.
  assoc_addresses_with_dstruct(caller_addr, origin, old_baseptr, *baseptrptr, numaddrs, "Data");
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically on the stack.
extern "C"
void bf_assoc_addresses_with_dstruct_stack (const char* origin, void* baseptr,
                                            uint64_t numaddrs, const char* varname)
{
  // Ignore this data structure if it consumes no space.
  if (numaddrs == 0)
    return;

  // Ignore this data structure if we don't know where we're coming from.
  void* caller_addr = bf_find_caller_address();
  if (caller_addr == NULL)
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

  // Associate the caller's address with the data structure.
  string prefix(varname);
  prefix = prefix == "*UNNAMED*" ? "Compiler-generated variable" : string("Variable ") + prefix;
  assoc_addresses_with_dstruct(caller_addr, origin, nullptr, baseptr, numaddrs, prefix.c_str());
}

  /*
   * If HAVE_BACKTRACE is not defined, stub all of the functions that
   * keep track of dynamic data structures.
   */
#else

extern "C"
void bf_disassoc_addresses_with_dstruct (void* baseptr)
{
}

extern "C"
void bf_assoc_addresses_with_dstruct (const char* origin, void* old_baseptr,
                                      void* baseptr, uint64_t numaddrs)
{
}

extern "C"
void bf_assoc_addresses_with_dstruct_pm (const char* origin, void* old_baseptr,
                                         void** baseptrptr, uint64_t numaddrs, int retcode)
{
}

extern "C"
void bf_assoc_addresses_with_dstruct_stack (const char* origin, void* baseptr,
                                            uint64_t numaddrs, const char* varname)
{
}

#endif

  /* The rest of this file works either with or without HAVE_BACKTRACE. */

// Increment access counts for a data structure.
extern "C"
void bf_access_data_struct (uint64_t baseaddr, uint64_t numaddrs, uint8_t load0store1)
{
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
#ifdef HAVE_BACKTRACE
    void* caller_addr = bf_find_caller_address();
    if (caller_addr == NULL)
      return;  // Ignore this access if we don't know where we're coming from.
    void* lastaddr = (void*)(baseaddr + numaddrs);
    for (void* addr = (void*)(uintptr_t(baseaddr));
         addr < lastaddr;
         addr = disassoc_addresses_with_dstruct(addr))
      ;
    assoc_addresses_with_dstruct(caller_addr, "unknown", nullptr,
                                 (void*)uintptr_t(baseaddr),
                                 numaddrs, "Unknown data structure");
    iter = data_structs->find(search_addr);
#else
    return;  // Ignore this access if we don't know where we're coming from.
#endif
  }
  counters = iter->second;

  // Increment the appropriate counters.
  if (iter == data_structs->end())
    return;  // Couldn't find data structure (no backtrace() function?)
  if (load0store1 == 0) {
    counters->load_ops++;
    counters->bytes_loaded += numaddrs;
  }
  else {
    counters->store_ops++;
    counters->bytes_stored += numaddrs;
  }
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
  if (a->name != b->name)
    return a->name < b->name;
  return a->origin < b->origin;
}

// Output load and store counters by data structure.
void bf_report_data_struct_counts (void)
{
  // Sort all data structures in the interval tree by decreasing order
  // of total bytes accessed.  Ignore any unaccessed data structures.
  vector<DataStructCounters*> interesting_data;
  for (auto iter = location_to_counters->begin(); iter != location_to_counters->end(); iter++) {
    DataStructCounters* counters = iter->second;
    if (counters->bytes_loaded + counters->bytes_stored > 0)
      interesting_data.push_back(counters);
  }
  sort(interesting_data.begin(), interesting_data.end(), compare_counter_interest);

  // Assign a name to all data structures that lack one (i.e., all dynamic data
  // structures).
  for (auto iter = interesting_data.begin(); iter != interesting_data.end(); iter++) {
    DataStructCounters* counters = *iter;
    counters->generate_symbol_name();
  }

  // Output a textual header line.
  *bfout << "BYFL_DATA_STRUCT_HEADER: "
         << setw(20) << "Size" << ' '
         << setw(20) << "LD_bytes" << ' '
         << setw(20) << "ST_bytes" << ' '
         << setw(20) << "LD_ops" << ' '
         << setw(20) << "ST_ops" << ' '
         << setw(29) << left << "Origin" << internal << ' '
         << "Name" << '\n';

  // Output a binary table header.
  *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Data-structure accesses";
  *bfbin << uint8_t(BINOUT_COL_UINT64) << "Number of allocations"
         << uint8_t(BINOUT_COL_UINT64) << "Total bytes allocated"
         << uint8_t(BINOUT_COL_UINT64) << "Maximum memory footprint"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes stored"
         << uint8_t(BINOUT_COL_UINT64) << "Load operations"
         << uint8_t(BINOUT_COL_UINT64) << "Store operations"
         << uint8_t(BINOUT_COL_STRING) << "Allocation point (mangled)"
         << uint8_t(BINOUT_COL_STRING) << "Allocation point (demangled)"
         << uint8_t(BINOUT_COL_UINT64) << "Data-structure instruction address"
         << uint8_t(BINOUT_COL_STRING) << "Data-structure name (mangled)"
         << uint8_t(BINOUT_COL_STRING) << "Data-structure name (demangled)"
         << uint8_t(BINOUT_COL_NONE);

  // Output both textual and binary data.
  for (auto iter = interesting_data.cbegin(); iter != interesting_data.cend(); iter++) {
    // Demangle the origin name.
    const DataStructCounters* counters = *iter;
    string demangled_origin(demangle_func_name(counters->origin));

    // Output textual data-structure information.
    *bfout << "BYFL_DATA_STRUCT:        "
           << setw(20) << counters->max_size << ' '
           << setw(20) << counters->bytes_loaded << ' '
           << setw(20) << counters->bytes_stored << ' '
           << setw(20) << counters->load_ops << ' '
           << setw(20) << counters->store_ops << ' '
           << setw(29) << left << demangled_origin << internal << ' '
           << counters->demangled_name << '\n';

    // Output binary data-structure information.
    *bfbin << uint8_t(BINOUT_ROW_DATA)
           << counters->num_allocs
           << counters->bytes_alloced
           << counters->max_size
           << counters->bytes_loaded
           << counters->bytes_stored
           << counters->load_ops
           << counters->store_ops
           << counters->origin
           << demangled_origin
           << uint64_t(uintptr_t(counters->alloc_addr))
           << counters->name
           << counters->demangled_name;
  }
  *bfbin << uint8_t(BINOUT_ROW_NONE);
}

} // namespace bytesflops

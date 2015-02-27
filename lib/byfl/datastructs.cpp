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
#ifdef HAVE_BACKTRACE
extern void* bf_find_caller_address (void);
extern const char* bf_address_to_location_string (void* addrp);
#endif

// Define an interval-tree type.
template<typename T>
class Interval
{
public:
  T lower;    // Lower limit
  T upper;    // Upper limit

  Interval(T ll, T ul) : lower(ll), upper(ul) { }

  bool operator<(const Interval<T>& other) const {
    return upper < other.lower;
  }
  bool operator>(const Interval<T>& other) const {
    return lower > other.upper;
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
  string name;            // Name of this data structure
  string demangled_name;  // Same as the above but demangled
  uint64_t current_size;  // Current total memory footprint in bytes
  uint64_t max_size;      // Largest memory footprint in bytes ever seen
  string origin;          // Who allocated the memory
  uint64_t bytes_loaded;  // Number of bytes loaded
  uint64_t bytes_stored;  // Number of bytes stored
  uint64_t load_ops;      // Number of load operations
  uint64_t store_ops;     // Number of store operations

  // The minimum we need to initialize are the mangled and demangled names of
  // the data structure, its initial size (which can grow), and its origin.
  DataStructCounters(string nm, string dnm, uint64_t sz, string org) :
    name(nm), demangled_name(dnm), current_size(sz), max_size(sz), origin(org),
    bytes_loaded(0), bytes_stored(0), load_ops(0), store_ops(0)
  {
  }
};

#ifdef USE_BFD
extern ProcessSymbolTable* procsymtab;  // The calling process's symbol table
#endif
static map<Interval<uint64_t>, DataStructCounters*>* data_structs;  // Information about each data structure
static map<string, DataStructCounters*>* location_to_counters;  // Map from a source-code location to data-structure counters

// Construct an interval tree of symbol addresses.  If the BFD library
// isn't available we proceed without information about statically
// allocated data structures.
void initialize_data_structures (void)
{
  data_structs = new map<Interval<uint64_t>, DataStructCounters*>;
  location_to_counters = new map<string, DataStructCounters*>;
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
    (*location_to_counters)[symname] = info;
  }
#endif
}

// Disassociate a range of previously allocated addresses (given the address
// at the beginning of the range) from the data structure to which it used to
// belong.
static uint64_t disassoc_addresses_with_dstruct (void* baseptr)
{
#ifdef HAVE_BACKTRACE
  // Find the address interval and set of counters.
  static Interval<uint64_t> search_addr(0, 0);
  search_addr.lower = search_addr.upper = uint64_t(uintptr_t(baseptr));
  auto iter = data_structs->find(search_addr);
  if (iter == data_structs->end())
    return 0;  // Address was not previously allocated (or somehow snuck by us).
  Interval<uint64_t> interval = iter->first;
  DataStructCounters* counters = iter->second;

  // Reduce the size of the data structure by the size of the address range
  // and break the link from the address interval to the counters.  Note
  // that location_to_counters still points to the counters; we don't want
  // to forget that the data structure ever existed just because it was
  // deallocated.
  uint64_t interval_length = interval.upper - interval.lower + 1;
  counters->current_size -= interval_length;
  data_structs->erase(iter);
  return interval_length;
#else
  return 0;
#endif
}

// For access from user code, wrap disassoc_addresses_with_dstruct() and
// discard the return value.
extern "C"
void bf_disassoc_addresses_with_dstruct (void* baseptr)
{
  (void) disassoc_addresses_with_dstruct(baseptr);
}

// Associate a range of addresses with a dynamically allocated data structure.
static void assoc_addresses_with_dstruct (const char* origin, void* old_baseptr,
                                          void* baseptr, uint64_t numaddrs,
                                          const char* var_prefix)
{
#ifdef HAVE_BACKTRACE
  // Use the caller's location as the symbol name.
  string dstruct_name;             // Fabricated data-structure name
  string dstruct_demangled_name;   // Same as the above but demangled
  void* caller_addr = bf_find_caller_address();
  if (caller_addr == NULL)
    // We don't know where we're coming from -- ignore this data structure.
    return;
# ifdef USE_BFD
  // If we have the BFD library, use that to get a more precise
  // source-code location.
  SourceCodeLocation* srcloc = procsymtab->find_address((uintptr_t)caller_addr);
  if (srcloc == nullptr) {
    // Location wasn't found -- use the address instead.
    char *caller_loc = new char[100];
    sprintf(caller_loc, (string(var_prefix) + " allocated at %p").c_str(), caller_addr);
    dstruct_name = caller_loc;
    delete[] caller_loc;
  }
  else {
    // Location was found -- format it and use it.
    stringstream locstr;
    locstr << var_prefix << " allocated at "
           << srcloc->file_name << ':' << srcloc->line_number
           << ", function " << srcloc->function_name
           << ", address " << hex << caller_addr << dec;
    dstruct_name = locstr.str();
    locstr.str(string());
    locstr << var_prefix << " allocated at "
           << srcloc->file_name << ':' << srcloc->line_number
           << ", function " << demangle_func_name(srcloc->function_name)
           << ", address " << hex << caller_addr << dec;
    dstruct_demangled_name = locstr.str();
  }
# else
  const char* caller_loc = bf_address_to_location_string(caller_addr);
  if (!strcmp(caller_loc, "??:0")) {
    // Location wasn't found -- use the address instead.
    char *alt_caller_loc = new char[100];
    sprintf(alt_caller_loc, (string(var_prefix) + " allocated at %p").c_str(), caller_addr);
    dstruct_name = dstruct_demangled_name = alt_caller_loc;
    delete[] alt_caller_loc;
  }
  else
    // Location was found -- use it.
    dstruct_name = dstruct_demangled_name = var_prefix + " allocated at " + caller_loc;
# endif

  // Find an existing set of counters for the same source-code location.  If no
  // such counters exist, allocate a new set.
  DataStructCounters* counters;      // Counters associated with the data structure
  bool new_allocation = true;   // false==realloc; true=other allocation call
  map<Interval<uint64_t>, DataStructCounters*>::iterator old_iter;  // Memory range to reallocate
  if (old_baseptr != nullptr) {
    static Interval<uint64_t> search_addr(0, 0);
    search_addr.lower = search_addr.upper = uint64_t(uintptr_t(old_baseptr));
    old_iter = data_structs->find(search_addr);
    new_allocation = old_iter == data_structs->end();
  }
  if (new_allocation) {
    // Common case -- we haven't seen the old base address before (because it's
    // presumably the same as the new address, and that's what was just
    // allocated).
    auto count_iter = location_to_counters->find(dstruct_name);
    if (count_iter == location_to_counters->end()) {
      // Not found -- allocate new counters.
      counters = new DataStructCounters(dstruct_name, dstruct_demangled_name,
                                        numaddrs, origin);
      (*location_to_counters)[dstruct_name] = counters;
    }
    else {
      // Found -- increment the size of the data structure.
      counters = count_iter->second;
      counters->current_size += numaddrs;
      if (counters->current_size > counters->max_size)
        counters->max_size = counters->current_size;
    }
  }
  else {
    // Case of realloc -- reuse the old counters, but remove the old address
    // range, and subtract off the bytes previously allocated.
    counters = old_iter->second;
    Interval<uint64_t> old_interval = old_iter->first;
    counters->current_size -= old_interval.upper - old_interval.lower + 1;
    counters->current_size += numaddrs;
    if (counters->current_size > counters->max_size)
      counters->max_size = counters->current_size;
    data_structs->erase(old_iter);
  }

  // Associate the new range of addresses with the old (or just created)
  // counters.
  uint64_t baseaddr = uint64_t(uintptr_t(baseptr));
  (*data_structs)[Interval<uint64_t>(baseaddr, baseaddr + numaddrs - 1)] = counters;
#endif
}

// Associate a range of addresses with a dynamically allocated data structure.
extern "C"
void bf_assoc_addresses_with_dstruct (const char* origin, void* old_baseptr,
                                      void* baseptr, uint64_t numaddrs)
{
  assoc_addresses_with_dstruct(origin, old_baseptr, baseptr, numaddrs, "Data");
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically by posix_memalign().
extern "C"
void bf_assoc_addresses_with_dstruct_pm (const char* origin, void* old_baseptr,
                                         void** baseptrptr, uint64_t numaddrs, int retcode)
{
#ifdef HAVE_BACKTRACE
  if (retcode == 0)
    assoc_addresses_with_dstruct(origin, old_baseptr, *baseptrptr, numaddrs, "Data");
#endif
}

// Associate a range of addresses with a dynamically allocated data structure
// allocated specifically on the stack.
extern "C"
void bf_assoc_addresses_with_dstruct_stack (const char* origin, void* baseptr,
                                            uint64_t numaddrs, const char* varname)
{
  // Disassociate all overlapping data structures.  For example if a function
  // declares "int32_t x,y;" then returns, then another function delcares
  // "int64_t foo;" and gets the same base address as x, we'll need to
  // associate foo's address range with foo from now on, not with x and y.
  for (uint64_t ofs = 0; ofs < numaddrs; ) {
    uint64_t freed = disassoc_addresses_with_dstruct((char*)baseptr + ofs);
    ofs += freed == 0 ? 1 : freed;
  }

  // Establish the new association.
  string prefix(varname);
  prefix = prefix == "*UNNAMED*" ? "Compiler-generated variable" : string("Variable ") + prefix;
  assoc_addresses_with_dstruct(origin, nullptr, baseptr, numaddrs, prefix.c_str());
}

// Increment access counts for a data structure.
extern "C"
void bf_access_data_struct (uint64_t baseaddr, uint64_t numaddrs, uint8_t load0store1)
{
  // Find the interval containing the base address.  Use a set of counts
  // representing unknown data structures if we failed to find an interval
  // (e.g., because the address represents a stack variable).
  static Interval<uint64_t> search_addr(0, 0);
  search_addr.lower = search_addr.upper = baseaddr;
  auto iter = data_structs->find(search_addr);
  DataStructCounters* counters;
  if (iter == data_structs->end()) {
    // The data structure wasn't found.  Frankly, I don't know how this can
    // happen, but since it does, try at least to record where it's coming
    // from.
    for (uint64_t ofs = 0; ofs < numaddrs; ) {
      uint64_t freed = disassoc_addresses_with_dstruct((void*)uintptr_t(baseaddr + ofs));
      ofs += freed == 0 ? 1 : freed;
    }
    assoc_addresses_with_dstruct("unknown", nullptr, (void*)uintptr_t(baseaddr),
                                 numaddrs, "Unknown data structure");
    iter = data_structs->find(search_addr);
  }
  counters = iter->second;

  // Increment the appropriate counters.
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
  vector<const DataStructCounters*> interesting_data;
  for (auto iter = location_to_counters->cbegin(); iter != location_to_counters->cend(); iter++) {
    const DataStructCounters* counters = iter->second;
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
         << "Name" << '\n';

  // Output a binary table header.
  *bfbin << uint8_t(BINOUT_TABLE_BASIC) << "Data-structure accesses";
  *bfbin << uint8_t(BINOUT_COL_UINT64) << "Maximum memory footprint"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes loaded"
         << uint8_t(BINOUT_COL_UINT64) << "Bytes stored"
         << uint8_t(BINOUT_COL_UINT64) << "Load operations"
         << uint8_t(BINOUT_COL_UINT64) << "Store operations"
         << uint8_t(BINOUT_COL_STRING) << "Allocation point (mangled)"
         << uint8_t(BINOUT_COL_STRING) << "Allocation point (demangled)"
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
           << counters->max_size
           << counters->bytes_loaded
           << counters->bytes_stored
           << counters->load_ops
           << counters->store_ops
           << counters->origin
           << demangled_origin
           << counters->name
           << counters->demangled_name;
  }
  *bfbin << uint8_t(BINOUT_ROW_NONE);
}

} // namespace bytesflops

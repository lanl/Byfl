/*
 * Helper library for computing bytes:flops ratios
 * (tallying unique bytes)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

// Define an equality functor for use in constructing various hash tables.
struct eqaddr
{
  bool operator()(uintptr_t p1, uintptr_t p2) const { return p1 == p2; }
};

// Define a mapping from a page-aligned memory address to a vector of
// byte tallies.
static const size_t logical_page_size = 8192;      // Arbitrary; not tied to the OS page size
class PageTableEntry {
private:
  bytecount_t* byte_counter;   // One counter per byte on the page
  size_t bytes_touched;        // Number of nonzeroes in the above

public:
  // Count the number of bytes that were touched.
  size_t count() const {
    return bytes_touched;
  }

  // Return the list of counts.
  const bytecount_t* get_counts() const {
    return byte_counter;
  }

  // Increment multiple bytes' counter.
  void increment(size_t pos1, size_t pos2) {
    for (size_t pos = pos1; pos <= pos2; pos++)
      switch (byte_counter[pos]) {
        case bf_max_bytecount:
          /* Maxed out our counter -- don't increment it further. */
          break;

        case 0:
          /* First time a byte was touched */
          bytes_touched++;
          /* No break */

        default:
          /* Common case -- increment the counter. */
          byte_counter[pos]++;
          break;
      }
  }

  PageTableEntry() {
    bytes_touched = 0;
    byte_counter = new bytecount_t[logical_page_size];
    memset((void *)byte_counter, 0, sizeof(bytecount_t)*logical_page_size);
  }

  ~PageTableEntry() {
    delete[] byte_counter;
  }
};
typedef CachedUnorderedMap<uintptr_t, PageTableEntry*, hash<uintptr_t>, eqaddr> page_to_counts_t;
typedef CachedUnorderedMap<const char*, page_to_counts_t*> func_to_page_t;

// Keep track of the unique bytes touched by each function and by the
// program as a whole.
static page_to_counts_t* global_unique_bytes = NULL;
static func_to_page_t* function_unique_bytes = NULL;

namespace bytesflops {

// Initialize some of our variables at first use.
void initialize_tallybytes (void)
{
  global_unique_bytes = new page_to_counts_t();
  function_unique_bytes = new func_to_page_t();
}

// Return the number of unique addresses in a given set of addresses.
static uint64_t tally_unique_addresses (const page_to_counts_t& mapping)
{
  uint64_t unique_addrs = 0;
  for (page_to_counts_t::const_iterator page_iter = mapping.begin();
       page_iter != mapping.end();
       page_iter++) {
    const PageTableEntry* counters = page_iter->second;
    unique_addrs += counters->count();
  }
  return unique_addrs;
}

// Return the number of unique addresses referenced by a given function.
uint64_t bf_tally_unique_addresses_tb (const char* funcname)
{
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    return 0;
  else
    return tally_unique_addresses(*map_iter->second);
}

// Return the number of unique addresses referenced by the entire program.
uint64_t bf_tally_unique_addresses_tb (void)
{
  return tally_unique_addresses(*global_unique_bytes);
}

// Given a mapping of page numbers to bit vectors and a page number,
// return a bit vector, creating it if not found.
static PageTableEntry* find_or_create_page (page_to_counts_t& mapping, uint64_t pagenum)
{
  page_to_counts_t::iterator counts_iter = mapping.find(pagenum);
  if (counts_iter == mapping.end()) {
    // This is the first bit we've touched on the page.
    mapping[pagenum] = new PageTableEntry();
    return mapping[pagenum];
  }
  else
    // We've seen other counts on this page.
    return counts_iter->second;
}

// Mark every bit in a given range as having been accessed.
static void flag_bytes_in_range (page_to_counts_t& mapping, uint64_t baseaddr, uint64_t numaddrs)
{
  uint64_t first_page = baseaddr / logical_page_size;
  uint64_t last_page = (baseaddr + numaddrs - 1) / logical_page_size;
  if (first_page == last_page) {
    // Common case (we hope) -- all addresses lie on the same logical page.
    PageTableEntry* counts = find_or_create_page(mapping, first_page);
    uint64_t pagebase = baseaddr % logical_page_size;
    counts->increment(pagebase, pagebase + numaddrs - 1);
  }
  else
    // Less common case -- addresses may span logical pages.
    for (uint64_t i = 0; i < numaddrs; i++) {
      uint64_t address = baseaddr + i;
      uint64_t pagenum = address / logical_page_size;
      uint64_t bitoffset = address % logical_page_size;
      PageTableEntry* counts = find_or_create_page(mapping, pagenum);
      counts->increment(bitoffset, bitoffset);
    }
}

// Associate a set of memory locations with a given function.  Return
// the page-to-bit-vector mapping for the given function.
static page_to_counts_t* assoc_addresses_with_func (const char* funcname,
                                                  uint64_t baseaddr,
                                                  uint64_t numaddrs)
{
  page_to_counts_t* unique_bytes;
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    // This is the first time we've seen this function.
    (*function_unique_bytes)[funcname] = unique_bytes = new page_to_counts_t();
  else
    // We've seen this function before.
    unique_bytes = map_iter->second;
  flag_bytes_in_range(*unique_bytes, baseaddr, numaddrs);
  return unique_bytes;
}

// Associate a set of memory locations with a given function.  This
// function basically wraps assoc_addresses_with_func() with a quick
// cache lookup.
extern "C"
void bf_assoc_addresses_with_func_tb (const char* funcname, uint64_t baseaddr, uint64_t numaddrs)
{
  // Keep track of the two most recently used page-to-bit-vector maps.
  typedef struct {
    const char* funcname;
    page_to_counts_t* unique_bytes;
  } prev_value_t;
  static prev_value_t prev_values[2] = {{NULL, NULL}, {NULL, NULL}};

  // Find the given function's mapping from page number to bit list.
  if (bf_call_stack)
    funcname = bf_func_and_parents;
  else
    funcname = bf_string_to_symbol(funcname);
  if (funcname == prev_values[0].funcname)
    // Fastest case: same function as last time
    flag_bytes_in_range(*prev_values[0].unique_bytes, baseaddr, numaddrs);
  else
    // Second-fastest case: same function as the time before last
    if (funcname == prev_values[1].funcname) {
      prev_value_t swap = prev_values[0];
      prev_values[0] = prev_values[1];
      prev_values[1] = swap;
      flag_bytes_in_range(*prev_values[0].unique_bytes, baseaddr, numaddrs);
    }
    else {
      // Slowest case: different function from the last two times
      prev_values[1] = prev_values[0];
      prev_values[0].funcname = funcname;
      prev_values[0].unique_bytes = assoc_addresses_with_func(funcname, baseaddr, numaddrs);
    }
}

// Associate a set of memory locations with the program as a whole.
extern "C"
void bf_assoc_addresses_with_prog_tb (uint64_t baseaddr, uint64_t numaddrs)
{
  flag_bytes_in_range(*global_unique_bytes, baseaddr, numaddrs);
}

// Return true if one {count, multiplier} pair has a greater
// count than another.
bool greater_count_than (bf_addr_tally_t a, bf_addr_tally_t b)
{
  return a.first > b.first;
}

// Convert a collection of tallies to a histogram, freeing the former
// as we build the latter.
void get_address_tally_hist (page_to_counts_t& mapping, vector<bf_addr_tally_t>& histogram, uint64_t* total)
{
  // Process each page of counts in turn.
  typedef CachedUnorderedMap<bytecount_t, uint64_t> count_to_mult_t;
  count_to_mult_t count2mult;               // Number of times each count was seen
  page_to_counts_t::iterator counts_iter;
  for (counts_iter = mapping.begin(); counts_iter != mapping.end(); ) {
    // Increment the iterator now so we can safely delete it at the
    // bottom of this block.
    uintptr_t baseaddr = counts_iter->first;
    PageTableEntry* pte = counts_iter->second;
    counts_iter++;

    // Increment the multiplier for each count.
    const bytecount_t* byte_counter = pte->get_counts();
    for (size_t i = 0; i < logical_page_size; i++)
      if (byte_counter[i] > 0)
        count2mult[byte_counter[i]]++;

    // Free the memory occupied by the page table entry.
    delete pte;
    mapping.erase(baseaddr);
  }

  // Convert count2mult from a map to a vector.
  for (count_to_mult_t::iterator c2m_iter = count2mult.begin(); c2m_iter != count2mult.end(); c2m_iter++) {
    histogram.push_back(*c2m_iter);
    *total += c2m_iter->second;
  }

  // Sort the resulting vector by decreasing access count.  That is x
  // accesses to each of y unique bytes will appear before x-a
  // accesses to each of z unique bytes, regardless of y and z.
  sort(histogram.begin(), histogram.end(), greater_count_than);
}

// Convert a collection of global tallies to a histogram, freeing the
// former as we build the latter.
void bf_get_address_tally_hist (vector<bf_addr_tally_t>& histogram, uint64_t* total)
{
  get_address_tally_hist(*global_unique_bytes, histogram, total);
}

} // namespace bytesflops

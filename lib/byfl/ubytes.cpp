/*
 * Helper library for computing bytes:flops ratios
 * (tracking unique bytes)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

// Define an equality functors for use in constructing various hash tables.
struct eqaddr
{
  bool operator()(uintptr_t p1, uintptr_t p2) const { return p1 == p2; }
};

// Define a mapping from a page-aligned memory address to a vector of
// bits touched on that page.
static const size_t logical_page_size = 8192;      // Arbitrary; not tied to the OS page size
class PageTableEntry {
private:
  uint64_t* bit_vector;           // One bit per byte on the page, packed into words
  size_t bits_set;                // Number of 1 bits in the above

public:
  // Count the number of bits that are set.
  size_t count() const {
    return bits_set;
  }

  // Set multiple bits to 1.
  void set(size_t pos1, size_t pos2) {
    // Do nothing if the page is full.
    if (!bit_vector)
      return;

    // Determine if all bits lie in the same word.
    size_t word_ofs1 = pos1/64;              // Offset of word representing pos1
    size_t word_ofs2 = pos2/64;              // Offset of word representing pos2
    if (word_ofs1 == word_ofs2) {
      // Fast case -- we have only one word to deal with.
      uint64_t word = bit_vector[word_ofs1]; // Vector of 64 bits
      size_t bit_ofs1 = pos1%64;             // First bit to set
      size_t bit_ofs2 = pos2%64;             // Last bit to set
      uint64_t mask;                         // All 0s except for the bits to set
      mask = ((2ULL<<(bit_ofs2 - bit_ofs1)) - 1ULL) << bit_ofs1;
      uint64_t new_word = word | mask;       // Word with new bits set
      bits_set += __builtin_popcountll(word ^ new_word);   // Tally the number of bits that changed.
      bit_vector[word_ofs1] = new_word;
    }
    else {
      // Slow case -- positions span multiple words.
      for (size_t pos = pos1; pos <= pos2; pos++) {
        size_t word_ofs = pos/64;             // Word containing the bit of interest
        size_t bit_ofs = pos%64;              // Bit offset in the target word
        uint64_t word = bit_vector[word_ofs]; // Vector of 64 bits
        uint64_t mask = 1ULL<<bit_ofs;        // All 0s except for bitofs
        if ((word&mask) == 0ULL) {
          // Bit was previously 0.  Set it to 1.
          bit_vector[word_ofs] |= mask;
          bits_set++;
        }
      }
    }

    // If we filled the page, deallocate the memory used by the bit
    // vector, as we won't be setting any more bits.
    if (bits_set == logical_page_size) {
      delete[] bit_vector;
      bit_vector = NULL;
    }
  }

  PageTableEntry() {
    bits_set = 0;
    bit_vector = new uint64_t[logical_page_size/64];
    memset((void *)bit_vector, 0, sizeof(uint64_t)*logical_page_size/64);
  }

  ~PageTableEntry() {
    delete[] bit_vector;
  }
};
typedef CachedUnorderedMap<uintptr_t, PageTableEntry*, hash<uintptr_t>, eqaddr> page_to_bits_t;
typedef CachedUnorderedMap<const char*, page_to_bits_t*> func_to_page_t;

// Keep track of the unique bytes touched by each function and by the
// program as a whole.
static page_to_bits_t* global_unique_bytes = NULL;
static func_to_page_t* function_unique_bytes = NULL;

namespace bytesflops {

// Initialize some of our variables at first use.
void initialize_ubytes (void)
{
  global_unique_bytes = new page_to_bits_t();
  function_unique_bytes = new func_to_page_t();
}


// Return the number of unique addresses in a given set of addresses.
static uint64_t tally_unique_addresses (const page_to_bits_t& mapping)
{
  uint64_t unique_addrs = 0;
  for (page_to_bits_t::const_iterator page_iter = mapping.begin();
       page_iter != mapping.end();
       page_iter++) {
    const PageTableEntry* bits = page_iter->second;
    unique_addrs += bits->count();
  }
  return unique_addrs;
}


// Return the number of unique addresses referenced by a given function.
uint64_t bf_tally_unique_addresses (const char* funcname)
{
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    return 0;
  else
    return tally_unique_addresses(*map_iter->second);
}


// Return the number of unique addresses referenced by the entire program.
uint64_t bf_tally_unique_addresses (void)
{
  return tally_unique_addresses(*global_unique_bytes);
}


// Given a mapping of page numbers to bit vectors and a page number,
// return a bit vector, creating it if not found.
static PageTableEntry* find_or_create_page (page_to_bits_t& mapping, uint64_t pagenum)
{
  page_to_bits_t::iterator bits_iter = mapping.find(pagenum);
  if (bits_iter == mapping.end()) {
    // This is the first bit we've touched on the page.
    mapping[pagenum] = new PageTableEntry();
    return mapping[pagenum];
  }
  else
    // We've seen other bits on this page.
    return bits_iter->second;
}


// Mark every bit in a given range as having been accessed.
static void flag_bytes_in_range (page_to_bits_t& mapping, uint64_t baseaddr, uint64_t numaddrs)
{
  uint64_t first_page = baseaddr / logical_page_size;
  uint64_t last_page = (baseaddr + numaddrs - 1) / logical_page_size;
  if (first_page == last_page) {
    // Common case (we hope) -- all addresses lie on the same logical page.
    PageTableEntry* bits = find_or_create_page(mapping, first_page);
    uint64_t pagebase = baseaddr % logical_page_size;
    bits->set(pagebase, pagebase + numaddrs - 1);
  }
  else
    // Less common case -- addresses may span logical pages.
    for (uint64_t i = 0; i < numaddrs; i++) {
      uint64_t address = baseaddr + i;
      uint64_t pagenum = address / logical_page_size;
      uint64_t bitoffset = address % logical_page_size;
      PageTableEntry* bits = find_or_create_page(mapping, pagenum);
      bits->set(bitoffset, bitoffset);
    }
}


// Associate a set of memory locations with a given function.  Return
// the page-to-bit-vector mapping for the given function.
static page_to_bits_t* assoc_addresses_with_func (const char* funcname,
                                                  uint64_t baseaddr,
                                                  uint64_t numaddrs)
{
  page_to_bits_t* unique_bytes;
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    // This is the first time we've seen this function.
    (*function_unique_bytes)[funcname] = unique_bytes = new page_to_bits_t();
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
void bf_assoc_addresses_with_func (const char* funcname, uint64_t baseaddr, uint64_t numaddrs)
{
  // Do nothing if counting is suppressed.
  if (bf_suppress_counting)
    return;

  // Keep track of the two most recently used page-to-bit-vector maps.
  typedef struct {
    const char* funcname;
    page_to_bits_t* unique_bytes;
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
void bf_assoc_addresses_with_prog (uint64_t baseaddr, uint64_t numaddrs)
{
  if (bf_suppress_counting)
    return;
  flag_bytes_in_range(*global_unique_bytes, baseaddr, numaddrs);
}

} // namespace bytesflops

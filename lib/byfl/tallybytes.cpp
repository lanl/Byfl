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

namespace bytesflops {

// Keep track of the unique bytes touched by each function and by the
// program as a whole.
typedef CachedUnorderedMap<const char*, WordPageTable*> func_to_page_t;
static WordPageTable* global_unique_bytes = nullptr;
static func_to_page_t* function_unique_bytes = nullptr;

// Define a logical page size to use throughout this file.
static const size_t logical_page_size = 8192;

// Initialize some of our variables at first use.
void initialize_tallybytes (void)
{
  global_unique_bytes = new WordPageTable(logical_page_size);
  function_unique_bytes = new func_to_page_t();
}

// Return the number of unique addresses referenced by a given function.
uint64_t bf_tally_unique_addresses_tb (const char* funcname)
{
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    return 0;
  else
    return map_iter->second->tally_unique();
}

// Return the number of unique addresses referenced by the entire program.
uint64_t bf_tally_unique_addresses_tb (void)
{
  return global_unique_bytes->tally_unique();
}

// Associate a set of memory locations with a given function.  Return the
// page-to-bit-vector mapping for the given function.
static WordPageTable* assoc_addresses_with_func (const char* funcname,
                                                 uint64_t baseaddr,
                                                 uint64_t numaddrs)
{
  WordPageTable* unique_bytes;
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    // This is the first time we've seen this function.
    (*function_unique_bytes)[funcname] = unique_bytes = new WordPageTable(logical_page_size);
  else
    // We've seen this function before.
    unique_bytes = map_iter->second;
  unique_bytes->access(baseaddr, numaddrs);
  return unique_bytes;
}

// Associate a set of memory locations with a given function.  This function
// basically wraps assoc_addresses_with_func() with a quick cache lookup.
extern "C"
void bf_assoc_addresses_with_func_tb (const char* funcname, uint64_t baseaddr, uint64_t numaddrs)
{
  WordPageTable* unique_bytes;
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    // This is the first time we've seen this function.
    (*function_unique_bytes)[funcname] = unique_bytes = new WordPageTable(logical_page_size);
  else
    // We've seen this function before.
    unique_bytes = map_iter->second;
  unique_bytes->access(baseaddr, numaddrs);
}

// Associate a set of memory locations with the program as a whole.
extern "C"
void bf_assoc_addresses_with_prog_tb (uint64_t baseaddr, uint64_t numaddrs)
{
  if (bf_suppress_counting)
    return;
  global_unique_bytes->access(baseaddr, numaddrs);
}

// Return true if one {count, multiplier} pair has a greater
// count than another.
bool greater_count_than (bf_addr_tally_t a, bf_addr_tally_t b)
{
  return a.first > b.first;
}

// Convert a collection of tallies to a histogram.
void get_address_tally_hist (WordPageTable& mapping, vector<bf_addr_tally_t>& histogram, uint64_t* total)
{
  // Process each page of counts in turn.
  typedef CachedUnorderedMap<bytecount_t, uint64_t> count_to_mult_t;
  count_to_mult_t count2mult;               // Number of times each count was seen
  for (auto counts_iter = mapping.begin(); counts_iter != mapping.end(); ) {
    // Increment the iterator now so we can safely delete it at the bottom of
    // this block.
    uintptr_t baseaddr = counts_iter->first;
    WordPageTableEntry* pte = counts_iter->second;
    counts_iter++;

    // Increment the multiplier for each count.
    const bytecount_t* byte_counter = pte->raw_counts();
    for (size_t i = 0; i < logical_page_size; i++)
      if (byte_counter[i] > 0)
        count2mult[byte_counter[i]]++;
  }

  // Convert count2mult from a map to a vector.
  for (count_to_mult_t::iterator c2m_iter = count2mult.begin(); c2m_iter != count2mult.end(); c2m_iter++) {
    histogram.push_back(*c2m_iter);
    *total += c2m_iter->second;
  }

  // Sort the resulting vector by decreasing access count.  That is x accesses
  // to each of y unique bytes will appear before x-a accesses to each of z
  // unique bytes, regardless of y and z.
  sort(histogram.begin(), histogram.end(), greater_count_than);
}

// Convert a collection of global tallies to a histogram, freeing the former as
// we build the latter.
void bf_get_address_tally_hist (vector<bf_addr_tally_t>& histogram, uint64_t* total)
{
  get_address_tally_hist(*global_unique_bytes, histogram, total);
}

} // namespace bytesflops

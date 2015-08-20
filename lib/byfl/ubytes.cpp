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

namespace bytesflops {

// Keep track of the unique bytes touched by each function and by the program
// as a whole.
typedef CachedUnorderedMap<const char*, BitPageTable*> func_to_page_t;
static BitPageTable* global_unique_bytes = nullptr;
static func_to_page_t* function_unique_bytes = nullptr;

// Define a logical page size to use throughout this file.
static const size_t logical_page_size = 8192;

// Initialize some of our variables at first use.
void initialize_ubytes (void)
{
  global_unique_bytes = new BitPageTable(logical_page_size);
  function_unique_bytes = new func_to_page_t();
}

// Return the number of unique addresses referenced by a given function.
uint64_t bf_tally_unique_addresses (const char* funcname)
{
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    return 0;
  else
    return map_iter->second->tally_unique();
}

// Return the number of unique addresses referenced by the entire program.
uint64_t bf_tally_unique_addresses (void)
{
  return global_unique_bytes->tally_unique();
}

// Associate a set of memory locations with a given function.  Return
// the page-to-bit-vector mapping for the given function.
static BitPageTable* assoc_addresses_with_func (const char* funcname,
                                                uint64_t baseaddr,
                                                uint64_t numaddrs)
{
  BitPageTable* unique_bytes;
  func_to_page_t::iterator map_iter = function_unique_bytes->find(funcname);
  if (map_iter == function_unique_bytes->end())
    // This is the first time we've seen this function.
    (*function_unique_bytes)[funcname] = unique_bytes = new BitPageTable(logical_page_size);
  else
    // We've seen this function before.
    unique_bytes = map_iter->second;
  unique_bytes->access(baseaddr, numaddrs);
  return unique_bytes;
}

// Associate a set of memory locations with a given function.
extern "C"
void bf_assoc_addresses_with_func (const char* funcname, uint64_t baseaddr, uint64_t numaddrs)
{
  // Do nothing if counting is suppressed.
  if (bf_suppress_counting)
    return;

  // Find the given function's mapping from page number to bit list.
  if (bf_call_stack)
    funcname = bf_func_and_parents;
  else
    funcname = bf_string_to_symbol(funcname);

  // Associate the range of addresses with the function's page table.
  assoc_addresses_with_func(funcname, baseaddr, numaddrs);
}

// Associate a set of memory locations with the program as a whole.
extern "C"
void bf_assoc_addresses_with_prog (uint64_t baseaddr, uint64_t numaddrs)
{
  if (bf_suppress_counting)
    return;
  global_unique_bytes->access(baseaddr, numaddrs);
}

} // namespace bytesflops

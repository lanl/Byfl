/*
 * Helper library for computing bytes:flops ratios
 * (page-table class declarations)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _PAGETABLE_H_
#define _PAGETABLE_H_

#include "byfl.h"

using namespace std;

namespace bytesflops {

// Define a mapping from a page-aligned memory address to a vector of counters,
// one per byte.
class BasePageTableEntry {
protected:
  size_t logical_page_size;    // Logical page size in bytes represented
  size_t bytes_touched;        // Number of bytes accessed at least once

public:
  // Store the logical page size.
  BasePageTableEntry(size_t pg_size) : logical_page_size(pg_size) { }

  // Return the number of bytes that were accessed.
  size_t count() const {
    return bytes_touched;
  }

  // Increment the tallies associated with a range of bytes, clamping each at
  // the maximum allowed value.
  virtual void increment(size_t pos1, size_t pos2) = 0;
};

// Specialize BasePageTableEntry for bit-sized counters.
class BitPageTableEntry : public BasePageTableEntry {
private:
  uint64_t* bit_vector;           // One bit per byte on the page, packed into words

public:
  // Increment the tallies associated with a range of bytes, clamping each at 1.
  void increment(size_t pos1, size_t pos2);

  // Define a constructor and destructor.
  BitPageTableEntry(size_t pg_size);
  ~BitPageTableEntry();
};

// Specialize BasePageTableEntry for word-sized counters.
class WordPageTableEntry : public BasePageTableEntry {
private:
  bytecount_t* byte_counter;      // One counter per byte on the page

public:
  // Increment the tallies associated with a range of bytes, clamping each at
  // the maximum word value.
  void increment(size_t pos1, size_t pos2);

  // Expose the raw counts.
  bytecount_t* raw_counts() { return byte_counter; }

  // Define a constructor and destructor.
  WordPageTableEntry(size_t pg_size);
  ~WordPageTableEntry();
};

// Define a page table that associates a counter with each byte of program
// memory.
template<typename PTE>
class PageTable {
private:
  // Define an equality functors for use in constructing various hash tables.
  struct eqaddr {
    bool operator()(uintptr_t p1, uintptr_t p2) const { return p1 == p2; }
  };

  // Define a mapping from a page number to the associated PageTableEntry.
  typedef CachedUnorderedMap<uintptr_t, PTE*, hash<uintptr_t>, eqaddr> page_to_PTE_t;
  page_to_PTE_t mapping;

  // Logical page size in bytes represented
  size_t logical_page_size;

  // Given a mapping of page numbers to counter vectors and a page number,
  // return a counter vector, creating it if not found.
  PTE* find_or_create_page (page_to_PTE_t& mapping, uint64_t pagenum) {
    auto counters_iter = mapping.find(pagenum);
    if (counters_iter == mapping.end()) {
      // This is the first byte we've touched on the page.
      mapping[pagenum] = new PTE(logical_page_size);
      return mapping[pagenum];
    }
    else
      // We've seen other bytes on this page.
      return counters_iter->second;
  }

public:
  // Store the logical page size.
  PageTable(size_t pg_size) : logical_page_size(pg_size) { }

  // Expose iterators to our underlying address-to-PTE mapping.
  typename page_to_PTE_t::iterator begin() { return mapping.begin(); }
  typename page_to_PTE_t::iterator end() { return mapping.end(); }

  // Increment each counter in a given range.
  void access (uint64_t baseaddr, uint64_t numaddrs) {
    uint64_t first_page = baseaddr / logical_page_size;
    uint64_t last_page = (baseaddr + numaddrs - 1) / logical_page_size;
    if (first_page == last_page) {
      // Common case (we hope) -- all addresses lie on the same logical page.
      PTE* counters = find_or_create_page(mapping, first_page);
      uint64_t pagebase = baseaddr % logical_page_size;
      counters->increment(pagebase, pagebase + numaddrs - 1);
    }
    else
      // Less common case -- addresses may span logical pages.
      for (uint64_t i = 0; i < numaddrs; i++) {
        uint64_t address = baseaddr + i;
        uint64_t pagenum = address / logical_page_size;
        uint64_t bitoffset = address % logical_page_size;
        PTE* counters = find_or_create_page(mapping, pagenum);
        counters->increment(bitoffset, bitoffset);
      }
  }

  // Return the number of unique addresses accessed.
  uint64_t tally_unique (void) {
    uint64_t unique_addrs = 0;
    for (auto page_iter = mapping.begin();
         page_iter != mapping.end();
         page_iter++) {
      const PTE* counters = page_iter->second;
      unique_addrs += counters->count();
    }
    return unique_addrs;
  }
};

// For convenience, define concrete BitPageTable and WordPageTable classes.
typedef PageTable<BitPageTableEntry> BitPageTable;
typedef PageTable<WordPageTableEntry> WordPageTable;

} // namespace bytesflops

#endif

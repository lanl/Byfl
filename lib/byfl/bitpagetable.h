/*
 * Helper library for computing bytes:flops ratios
 * (page-table class definitions)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _BITPAGETABLE_H_
#define _BITPAGETABLE_H_

#include "byfl.h"

using namespace std;

// Define a logical page size for the page table.  This is an arbitrary value
// that is not tied to the OS page size.
static const size_t bit_page_size = 8192;

// Define a mapping from a page-aligned memory address to a vector of
// bits touched on that page.
class BitPageTableEntry {
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
    if (bits_set == bit_page_size) {
      delete[] bit_vector;
      bit_vector = NULL;
    }
  }

  BitPageTableEntry() {
    bits_set = 0;
    bit_vector = new uint64_t[bit_page_size/64];
    memset((void *)bit_vector, 0, sizeof(uint64_t)*bit_page_size/64);
  }

  ~BitPageTableEntry() {
    delete[] bit_vector;
  }
};

// Define a page table that associates a bit with every byte of program memory.
class BitPageTable {
 private:
  // Define an equality functors for use in constructing various hash tables.
  struct eqaddr {
    bool operator()(uintptr_t p1, uintptr_t p2) const { return p1 == p2; }
  };

  // Define a mapping from a page number to the associated BitPageTableEntry.
  typedef CachedUnorderedMap<uintptr_t, BitPageTableEntry*, hash<uintptr_t>, eqaddr> page_to_bits_t;
  page_to_bits_t mapping;

  // Given a mapping of page numbers to bit vectors and a page number, return a
  // bit vector, creating it if not found.
  BitPageTableEntry* find_or_create_page (page_to_bits_t& mapping, uint64_t pagenum) {
    page_to_bits_t::iterator bits_iter = mapping.find(pagenum);
    if (bits_iter == mapping.end()) {
      // This is the first bit we've touched on the page.
      mapping[pagenum] = new BitPageTableEntry();
      return mapping[pagenum];
    }
    else
      // We've seen other bits on this page.
      return bits_iter->second;
  }

 public:
  // Mark every bit in a given range as having been accessed.
  void access (uint64_t baseaddr, uint64_t numaddrs) {
    uint64_t first_page = baseaddr / bit_page_size;
    uint64_t last_page = (baseaddr + numaddrs - 1) / bit_page_size;
    if (first_page == last_page) {
      // Common case (we hope) -- all addresses lie on the same logical page.
      BitPageTableEntry* bits = find_or_create_page(mapping, first_page);
      uint64_t pagebase = baseaddr % bit_page_size;
      bits->set(pagebase, pagebase + numaddrs - 1);
    }
    else
      // Less common case -- addresses may span logical pages.
      for (uint64_t i = 0; i < numaddrs; i++) {
	uint64_t address = baseaddr + i;
	uint64_t pagenum = address / bit_page_size;
	uint64_t bitoffset = address % bit_page_size;
	BitPageTableEntry* bits = find_or_create_page(mapping, pagenum);
	bits->set(bitoffset, bitoffset);
      }
  }

  // Return the number of unique addresses accessed.
  uint64_t tally_unique (void) {
    uint64_t unique_addrs = 0;
    for (page_to_bits_t::const_iterator page_iter = mapping.begin();
	 page_iter != mapping.end();
	 page_iter++) {
      const BitPageTableEntry* bits = page_iter->second;
      unique_addrs += bits->count();
    }
    return unique_addrs;
  }
};

#endif

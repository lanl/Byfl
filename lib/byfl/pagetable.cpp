/*
* Helper library for computing bytes:flops ratios
* (page-table class implementations)
*
* By Scott Pakin <pakin@lanl.gov>
*/

#include "byfl.h"

using namespace std;

namespace bytesflops {

// Construct a page-table entry for bit-sized counters.
BitPageTableEntry::BitPageTableEntry(size_t pg_size) : BasePageTableEntry(pg_size)
{
  bytes_touched = 0;
  bit_vector = new uint64_t[logical_page_size/64];
  memset((void *)bit_vector, 0, sizeof(uint64_t)*logical_page_size/64);
}

// Copy an existing page-table entry.
BitPageTableEntry::BitPageTableEntry(const BitPageTableEntry& other) : BasePageTableEntry(other)
{
  bit_vector = new uint64_t[logical_page_size/64];
  memcpy((void *)bit_vector, other.bit_vector, sizeof(uint64_t)*logical_page_size/64);
}

// Destruct a bit-sized page-table entry.
BitPageTableEntry::~BitPageTableEntry()
{
  delete[] bit_vector;
}

// Increment the tallies associated with a range of bytes, clamping each at 1.
void BitPageTableEntry::increment(size_t pos1, size_t pos2)
{
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
    bytes_touched += __builtin_popcountll(word ^ new_word);   // Tally the number of bits that changed.
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
        bytes_touched++;
      }
    }
  }

  // If we filled the page, deallocate the memory used by the bit
  // vector, as we won't be setting any more bits.
  if (bytes_touched == logical_page_size) {
    delete[] bit_vector;
    bit_vector = NULL;
  }
}

// Merge the counts from another BitPageTableEntry into ours.
void BitPageTableEntry::merge(BitPageTableEntry* other)
{
  // Do nothing if our page is full.
  if (!bit_vector)
    return;

  // If the other PTE is full, set all of our bits to 1.
  if (other->bit_vector == nullptr) {
    for (size_t w = 0; w < logical_page_size/64; w++) {
      uint64_t word0 = bit_vector[w];
      for (size_t b = 0; b < 64; b++) {
        uint64_t bit0 = word0 & (1ULL<<b);
        uint64_t bit1 = 1ULL<<b;
        if (bit0 == 0ULL) {
          word0 |= bit1;
          bytes_touched++;
        }
      }
      bit_vector[w] = word0;
    }
    return;
  }

  // Merge bit-by-bit, incrementing bytes_touched on any transition
  // from zero to one.
  for (size_t w = 0; w < logical_page_size/64; w++) {
    uint64_t word0 = bit_vector[w];
    uint64_t word1 = other->bit_vector[w];
    for (size_t b = 0; b < 64; b++) {
      uint64_t bit0 = word0 & (1ULL<<b);
      uint64_t bit1 = word1 & (1ULL<<b);
      if (bit0 == 0ULL && bit1 != 0ULL) {
        word0 |= bit1;
        bytes_touched++;
      }
    }
    bit_vector[w] = word0;
  }
}

// Construct a page-table entry for word-sized counters.
WordPageTableEntry::WordPageTableEntry(size_t pg_size) : BasePageTableEntry(pg_size)
{
  bytes_touched = 0;
  byte_counter = new bytecount_t[logical_page_size];
  memset((void *)byte_counter, 0, sizeof(bytecount_t)*logical_page_size);
}

// Copy an existing page-table entry.
WordPageTableEntry::WordPageTableEntry(const WordPageTableEntry& other) : BasePageTableEntry(other)
{
  byte_counter = new bytecount_t[logical_page_size];
  memcpy((void *)byte_counter, other.byte_counter, sizeof(bytecount_t)*logical_page_size);
}

// Destruct a word-sized page-table entry.
WordPageTableEntry::~WordPageTableEntry()
{
  delete[] byte_counter;
}

// Increment the tallies associated with a range of bytes, clamping each at the
// maximum word value.
void WordPageTableEntry::increment(size_t pos1, size_t pos2)
{
  for (size_t pos = pos1; pos <= pos2; pos++)
    switch (byte_counter[pos]) {
      case bf_max_bytecount:
        // Maxed out our counter -- don't increment it further.
        break;

      case 0:
        // First time a byte was touched
        bytes_touched++;
        byte_counter[pos]++;
        break;

      default:
        // Common case -- increment the counter.
        byte_counter[pos]++;
        break;
    }
}

// Merge the counts from another WordPageTableEntry into ours.
void WordPageTableEntry::merge(WordPageTableEntry* other)
{
  for (size_t pos = 0; pos < logical_page_size; pos++) {
    bytecount_t count0 = byte_counter[pos];
    bytecount_t count1 = other->byte_counter[pos];
    if (count1 == 0)
      continue;
    if (count0 != 0) {
      byte_counter[pos] = count1;
      bytes_touched++;
    }
    else
      byte_counter[pos] = (count1 > bf_max_bytecount - count0) ? bf_max_bytecount : count0 + count1;
  }
}

} // namespace bytesflops

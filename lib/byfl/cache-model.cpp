/*
 * Simple cache model for predicting miss rates.
 *
 * By Eric Anger <eanger@lanl.gov>
 */

#include <algorithm>
#include <iterator>

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

class Cache {
  public:
    void access(uint64_t baseaddr, uint64_t numaddrs);
    Cache(uint64_t line_size) : line_size_{line_size}, accesses_{0},
      cold_misses_{0} {}
    uint64_t getAccesses() const { return accesses_; }
    vector<uint64_t> getHits() const { return hits_; }
    uint64_t getColdMisses() const { return cold_misses_; }

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    uint64_t accesses_;
    uint64_t cold_misses_;
    vector<uint64_t> hits_;  // back is lru, front is mru
};

void Cache::access(uint64_t baseaddr, uint64_t numaddrs){
  uint64_t num_accesses = 0; // running total of number of lines accessed
  for(uint64_t addr = baseaddr / line_size_ * line_size_;
      addr <= (baseaddr + numaddrs ) / line_size_ * line_size_;
      addr += line_size_){
    ++num_accesses;
    auto line = lines_.rbegin();
    auto hit = begin(hits_);
    bool found = false;
    for(; line != lines_.rend(); ++line, ++hit){
      if(addr == *line){
        found = true;
        ++(*hit);
        // erase the line pointed to by this reverse iterator. see
        // stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
        lines_.erase((line + 1).base());
        break;
      }
    }

    if(!found){
      // add a new hit entry with this hit
      hits_.push_back(1);
      // this is a cold miss since we've never seen the line before.
      ++cold_misses_;
    }

    // move up this address to mru position
    lines_.push_back(addr);
  }

  // we've made all our accesses
  accesses_ += num_accesses;
}

static Cache* cache = NULL;

namespace bytesflops{

void initialize_cache(void){
  cache = new Cache(bf_line_size);
}

// Access the cache model with this address.
void bf_touch_cache(uint64_t baseaddr, uint64_t numaddrs){
  cache->access(baseaddr, numaddrs);
}

// Get cache accesses
uint64_t bf_get_cache_accesses(void){
  return cache->getAccesses();
}

// Get cache hits
vector<uint64_t> bf_get_cache_hits(void){
  // The total hits to a cache size N is equal to the sum of unique hits to 
  // all caches sized N or smaller.
  auto hits = cache->getHits();
  auto cold_misses = cache->getColdMisses();
  vector<uint64_t> tot_hits(hits.size());
  uint64_t prev_hits = 0;
  for(uint64_t i = 0; i < hits.size(); ++i){
    tot_hits[i] = hits[i] + prev_hits;
    if(i == 0){
      // remove cold misses from all cache sizes
      tot_hits[i] -= cold_misses;
    }
    prev_hits = tot_hits[i];
  }
  return tot_hits;
}

uint64_t bf_get_cold_misses(void){
  return cache->getColdMisses();
}

} // namespace bytesflops

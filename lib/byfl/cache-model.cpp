/*
 * Simple cache model for predicting miss rates.
 *
 * By Eric Anger <eanger@lanl.gov>
 */

#include <algorithm>
#include <iterator>
#include <thread>
#include <mutex>

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

class Cache {
  public:
    void access(uint64_t baseaddr, uint64_t numaddrs);
    Cache(uint64_t line_size) : line_size_{line_size}, accesses_{0},
      split_accesses_{0} {}
    uint64_t getAccesses() const { return accesses_; }
    vector<uint64_t> getHits() const { return hits_; }
    uint64_t getColdMisses() const { return hits_.size(); }
    uint64_t getSplitAccesses() const { return split_accesses_; }

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    uint64_t accesses_;
    vector<uint64_t> hits_;  // back is lru, front is mru
    uint64_t split_accesses_;
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
      // add a new hit entry
      hits_.push_back(0);
    }

    // move up this address to mru position
    lines_.push_back(addr);
  }

  // we've made all our accesses
  accesses_ += num_accesses;
  if(num_accesses != 1){
    ++split_accesses_;
  }
}

namespace bytesflops{

static __thread Cache* cache = nullptr;
static vector<Cache*>* caches = nullptr;
static mutex mymutex;

void initialize_cache(void){
  if(caches == nullptr){
    caches = new vector<Cache*>();
  }
}

// Access the cache model with this address.
void bf_touch_cache(uint64_t baseaddr, uint64_t numaddrs){
  if(cache == nullptr){
    // Only let one thread update caches at a time.
    lock_guard<mutex> guard(mymutex);
    cache = new Cache(bf_line_size);
    caches->push_back(cache);
  }
  cache->access(baseaddr, numaddrs);
}

// Get cache accesses
uint64_t bf_get_cache_accesses(void){
  uint64_t res = 0;
  for(auto& cache: *caches){
    res += cache->getAccesses();
  }
  return res;
}

// Get cache hits
vector<uint64_t> bf_get_cache_hits(void){
  // The total hits to a cache size N is equal to the sum of unique hits to all
  // caches sized N or smaller.  We'll aggregate the cache performance across
  // all threads; global L1 accesses is equivalent to the sum of individual L1
  // accesses, etc.
  size_t longest = 0;
  for(auto& cache: *caches){
    auto cur_size = cache->getHits().size();
    if(cur_size > longest){
      longest = cur_size;
    }
  }
  // Initialize all to zero. As long as the longest thread cache.
  vector<uint64_t> tot_hits(longest, 0);
  for(auto& cache: *caches){
    auto hits = cache->getHits();
    for(uint64_t i = 0; i < hits.size(); ++i){
      tot_hits[i] += hits[i];
    }
  }

  // Compute rolling sum of all previous entries.
  uint64_t prev_hits = 0;
  for(uint64_t i = 0; i < tot_hits.size(); ++i){
    tot_hits[i] += prev_hits;
    prev_hits = tot_hits[i];
  }
  return tot_hits;
}

uint64_t bf_get_cold_misses(void){
  uint64_t res = 0;
  for(auto& cache: *caches){
    res += cache->getColdMisses();
  }
  return res;
}

uint64_t bf_get_split_accesses(void){
  uint64_t res = 0;
  for(auto& cache: *caches){
    res += cache->getSplitAccesses();
  }
  return res;
}

} // namespace bytesflops

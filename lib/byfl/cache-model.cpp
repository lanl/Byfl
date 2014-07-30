/*
 * Simple cache model for predicting miss rates.
 *
 * By Eric Anger <eanger@lanl.gov>
 */

#include <algorithm>
#include <iterator>
#include <thread>
#include <mutex>
#include <fstream>

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

class Cache {
  public:
    void access(uint64_t baseaddr, uint64_t numaddrs);
    Cache(uint64_t line_size, uint64_t max_set_bits) : line_size_{line_size},
      accesses_{0}, split_accesses_{0}, log2_line_size_{0},
      max_set_bits_{max_set_bits}, cold_misses_{0} {
        auto lsize = line_size_;
        while(lsize >>= 1) ++log2_line_size_;
    }
    uint64_t getAccesses() const { return accesses_; }
    vector<vector<uint64_t> > getHits() const { return hits_; }
    uint64_t getColdMisses() const { return cold_misses_; }
    uint64_t getSplitAccesses() const { return split_accesses_; }
    int getRightMatch(uint64_t a, uint64_t b);

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    uint64_t accesses_;
    vector<vector<uint64_t> > hits_;  // back is lru, front is mru
    uint64_t split_accesses_;
    uint64_t log2_line_size_; // log base 2 of line size
    uint64_t max_set_bits_; // log base 2 of max number of sets
    uint64_t cold_misses_;
};

inline int Cache::getRightMatch(uint64_t a, uint64_t b){
  // number of 0s, counting from left, ignoring all line bits.
  // also need to mask off all bits higher than max_set_bits_.
  auto diff_bits = ((a ^ b) >> log2_line_size_) | (1 << (max_set_bits_ - 1));
  return __builtin_ctzll(diff_bits);
}

void Cache::access(uint64_t baseaddr, uint64_t numaddrs){
  uint64_t num_accesses = 0; // running total of number of lines accessed
  for(uint64_t addr = baseaddr / line_size_ * line_size_;
      addr <= (baseaddr + numaddrs ) / line_size_ * line_size_;
      addr += line_size_){
    ++num_accesses;
    bool found = false;
    vector<uint64_t> right_match_tally(max_set_bits_, 0);
    for(auto line = lines_.rbegin(); line != lines_.rend(); ++line){
      int right_match = getRightMatch(addr, *line); // returns 0 <= val = max_set_bits_
      ++right_match_tally[right_match];
      if(addr == *line){
        found = true;
        // erase the line pointed to by this reverse iterator. see
        // stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
        lines_.erase((line + 1).base());
        break;
      }
    }

    if(found){
      // rolling sum of right match tally for reuse dists
      uint64_t sum = 0;
      for(auto tally = right_match_tally.rbegin();
          tally != right_match_tally.rend(); 
          ++tally){
        *tally += sum;
        sum = *tally;
      }
      for(uint64_t i = 0; i < max_set_bits_; ++i){
        auto idx = right_match_tally[i] * (1 << i);
        if(hits_.size() < (idx + 1) ){
          hits_.resize(idx + 1, vector<uint64_t>(max_set_bits_, 0));
        }
        ++hits_[idx][i];
      }
    } else {
      ++cold_misses_;
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
    cache = new Cache(bf_line_size, bf_max_set_bits);
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

template<typename T>
vector<T> vecsum(const vector<T>& a, const vector<T>& b){
  vector<T> out(a.size());
  transform(begin(a), end(a), begin(b), begin(out),
            [](const T& a, const T& b){ return a + b; });
  return out;
}

// Get cache hits
vector<vector<uint64_t> > bf_get_cache_hits(void){
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
  auto assoc_bits = bf_max_set_bits;

  // Initialize all to zero. As long as the longest thread cache.
  vector<vector<uint64_t> > tot_hits(longest, vector<uint64_t>(assoc_bits + 1, 0));
  for(auto& cache: *caches){
    auto hits = cache->getHits();
    transform(begin(hits), end(hits), begin(tot_hits), begin(tot_hits), vecsum<uint64_t>);
  }

  vector<uint64_t> prev_hits(assoc_bits + 1, 0);
  for(auto& hits : tot_hits){
    hits = vecsum(hits, prev_hits);
    prev_hits = hits;
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

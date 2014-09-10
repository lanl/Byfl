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

namespace bytesflops {
static __thread unsigned cache_id = 0;
}
using namespace bytesflops;
using namespace std;

class Cache {
  public:
    void access(uint64_t baseaddr, uint64_t numaddrs);
    Cache(uint64_t line_size, uint64_t max_set_bits, bool record_thread_id) :
      line_size_{line_size}, accesses_{0}, unaligned_accesses_{0},
      log2_line_size_{0}, max_set_bits_{max_set_bits}, cold_misses_{0},
      hits_(max_set_bits_), record_thread_id_{record_thread_id},
      remote_hits_(max_set_bits_) {
        auto lsize = line_size_;
        while(lsize >>= 1) ++log2_line_size_;
    }
    uint64_t getAccesses() const { return accesses_; }
    vector<unordered_map<uint64_t,uint64_t> > getHits() const { return hits_; }
    uint64_t getColdMisses() const { return cold_misses_; }
    uint64_t getUnalignedAccesses() const { return unaligned_accesses_; }
    int getRightMatch(uint64_t a, uint64_t b);
    vector<unordered_map<uint64_t,uint64_t> > getRemoteHits() const { return remote_hits_; }

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    uint64_t accesses_;
    uint64_t unaligned_accesses_;
    uint64_t log2_line_size_; // log base 2 of line size
    uint64_t max_set_bits_; // log base 2 of max number of sets
    uint64_t cold_misses_;
    // for each set count, a map of distance to access count
    vector<unordered_map<uint64_t,uint64_t> > hits_;  // back is lru, front is mru
    bool record_thread_id_;
    // associate thread id with each line in cache. only used if record_thread_id_.
    vector<unsigned> thread_ids_;
    // for each set count, a map of distance to access count
    vector<unordered_map<uint64_t,uint64_t> > remote_hits_;  // back is lru, front is mru
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
      addr <= (baseaddr + numaddrs - 1) / line_size_ * line_size_;
      addr += line_size_){
    ++num_accesses;
    bool found = false;
    unsigned last_thread = 0;
    vector<uint64_t> right_match_tally(max_set_bits_, 0);
    for(int line_idx = lines_.size() - 1; line_idx >= 0; --line_idx){
      auto& line = lines_[line_idx];
      int right_match = getRightMatch(addr, line); // returns 0 <= val = max_set_bits_
      ++right_match_tally[right_match];
      if(addr == line){
        found = true;
        // erase this line.
        lines_.erase(begin(lines_) + line_idx);
        if(record_thread_id_){
          last_thread = thread_ids_[line_idx];
          thread_ids_.erase(begin(thread_ids_) + line_idx);
        }
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
      for(uint64_t set = 0; set < max_set_bits_; ++set){
        auto idx = right_match_tally[set];
        ++hits_[set][idx];
        if(record_thread_id_ && 
           last_thread != cache_id){
          ++remote_hits_[set][idx];
        }
      }
    } else {
      ++cold_misses_;
    }

    // move up this address to mru position
    lines_.push_back(addr);
    if(record_thread_id_){
      thread_ids_.push_back(cache_id);
    }
  }

  // we've made all our accesses
  accesses_ += num_accesses;
  if(num_accesses != 1)
    unaligned_accesses_ += num_accesses - (numaddrs + line_size_ - 1)/line_size_;
}

namespace bytesflops{

static __thread Cache* cache = nullptr;
static vector<Cache*>* caches = nullptr;
static Cache* global_cache = nullptr;
static mutex cache_vector_mutex, global_cache_mutex;
static unsigned thread_counter = 0;

void initialize_cache(void){
  if(caches == nullptr){
    caches = new vector<Cache*>();
  }
  global_cache = new Cache(bf_line_size, bf_max_set_bits, true);
}

// Access the cache model with this address.
void bf_touch_cache(uint64_t baseaddr, uint64_t numaddrs){
  if(cache == nullptr){
    // Only let one thread update caches at a time.
    lock_guard<mutex> guard(cache_vector_mutex);
    cache = new Cache(bf_line_size, bf_max_set_bits, false);
    caches->push_back(cache);
    cache_id = thread_counter++;
  }
  cache->access(baseaddr, numaddrs);
  lock_guard<mutex> guard(global_cache_mutex);
  global_cache->access(baseaddr, numaddrs);
}

// Get cache accesses
uint64_t bf_get_private_cache_accesses(void){
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

template<typename T>
T mapsum(const T& a, const T& b){
  T out(a);
  for(const auto& elem : b){
    out[elem.first] += elem.second;
  }
  return out;
}

// Get cache hits
uint64_t bf_get_shared_cache_accesses(void){
  return global_cache->getAccesses();
}

// Get cache hits
vector<unordered_map<uint64_t,uint64_t> > bf_get_private_cache_hits(void){
  // The total hits to a cache size N is equal to the sum of unique hits to all
  // caches sized N or smaller.  We'll aggregate the cache performance across
  // all threads; global L1 accesses is equivalent to the sum of individual L1
  // accesses, etc.
  vector<unordered_map<uint64_t,uint64_t> > tot_hits(bf_max_set_bits + 1);
  for(auto& cache: *caches){
    auto hits = cache->getHits();
    transform(begin(hits), end(hits), begin(tot_hits), begin(tot_hits), mapsum<unordered_map<uint64_t,uint64_t> >);
  }

  return tot_hits;
}

vector<unordered_map<uint64_t,uint64_t> > bf_get_shared_cache_hits(void){
  return global_cache->getHits();
}

vector<unordered_map<uint64_t,uint64_t> > bf_get_remote_shared_cache_hits(void){
  return global_cache->getRemoteHits();
}

uint64_t bf_get_private_cold_misses(void){
  uint64_t res = 0;
  for(auto& cache: *caches){
    res += cache->getColdMisses();
  }
  return res;
}

uint64_t bf_get_shared_cold_misses(void){
  return global_cache->getColdMisses();
}

uint64_t bf_get_private_unaligned_accesses(void){
  uint64_t res = 0;
  for(auto& cache: *caches){
    res += cache->getUnalignedAccesses();
  }
  return res;
}

uint64_t bf_get_shared_unaligned_accesses(void){
  return global_cache->getUnalignedAccesses();
}

} // namespace bytesflops

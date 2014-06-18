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
    Cache(uint64_t line_size) : line_size_{line_size}, accesses_{0} {}
    uint64_t getAccesses() const { return accesses_; }
    vector<uint64_t> getHits() const { return hits_; }

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    uint64_t accesses_;
    vector<uint64_t> hits_;  // back is lru, front is mru
};

void Cache::access(uint64_t baseaddr, uint64_t numaddrs){
  for(uint64_t addr = baseaddr / line_size_ * line_size_;
      addr <= (baseaddr + numaddrs ) / line_size_ * line_size_;
      addr += line_size_){
    auto first = max(baseaddr, addr);
    auto last = min(baseaddr + numaddrs, addr + line_size_);
    auto line = lines_.rbegin();
    auto hit = begin(hits_);
    bool found = false;
    for(; line != lines_.rend(); ++line, ++hit){
      if(addr == *line){
        found = true;
        transform(hit, end(hits_), hit,
                  [=](const uint64_t cur_hits){return cur_hits + last - first;});
        // erase the line pointed to by this reverse iterator. see
        // stackoverflow.com/questions/1830158/how-to-call-erase-with-a-reverse-iterator
        lines_.erase((line + 1).base());
        break;
      }
    }

    if(!found){
      // make a new entry containing all previous hits plus this one
      hits_.push_back(accesses_ + last - first);
    }

    // move up this address to mru position
    lines_.push_back(addr);
  }

  // we've made all our accesses
  accesses_ += numaddrs;
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
  return cache->getHits();
}

} // namespace bytesflops

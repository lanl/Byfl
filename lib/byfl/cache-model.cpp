/*
 * Simple cache model for predicting miss rates.
 *
 * By Eric Anger <eanger@lanl.gov>
 */

#include <algorithm>

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

class Cache {
  public:
    void access(uint64_t baseaddr, uint64_t numaddrs);
    Cache(uint64_t num_lines, uint64_t line_size) : lines_(num_lines), 
      line_size_{line_size}, accesses_(num_lines,0), hits_(num_lines,0) {}
    vector<uint64_t> getAccesses() const { return accesses_; }
    vector<uint64_t> getHits() const { return hits_; }

  private:
    vector<uint64_t> lines_; // back is mru, front is lru
    uint64_t line_size_;
    vector<uint64_t> accesses_;
    vector<uint64_t> hits_;
};

void Cache::access(uint64_t baseaddr, uint64_t numaddrs){
  //accesses_ += numaddrs;
  for(uint64_t addr = baseaddr / line_size_ * line_size_;
      addr <= (baseaddr + numaddrs ) / line_size_ * line_size_;
      addr += line_size_){
    auto first = max(baseaddr, addr);
    auto last = min(baseaddr + numaddrs, addr + line_size_);
    //auto line = find(begin(lines_), end(lines_), addr);
    vector<uint64_t>::iterator line = end(lines_);
    for(uint64_t i = 0; i < lines_.size(); ++i){
      accesses_[i] += last - first;
      if(addr == lines_[i]){
        line = begin(lines_) + i;
        break;
      }
    }
    if(line != end(lines_)){
      transform(line, end(hits_),
                line, 
                [=](const uint64_t cur_hits){ return cur_hits + last - first; });
      //hits_ += last - first;
      // move to mru
      lines_.erase(line);
    } else {
      //replace lru
      lines_.erase(begin(lines_));
    }
    lines_.push_back(addr);
  }
}

static Cache* cache = NULL;

namespace bytesflops{

void initialize_cache(void){
  cache = new Cache(bf_cache_lines, bf_line_size);
}

// Access the cache model with this address.
void bf_touch_cache(uint64_t baseaddr, uint64_t numaddrs){
  cache->access(baseaddr, numaddrs);
}

// Get cache accesses
vector<uint64_t> bf_get_cache_accesses(void){
  return cache->getAccesses();
}

// Get cache hits
vector<uint64_t> bf_get_cache_hits(void){
  return cache->getHits();
}

} // namespace bytesflops

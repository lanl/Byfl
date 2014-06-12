/*
 * Simple cache model for predicting miss rates.
 *
 * By Eric Anger <eanger@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

bool debug = true;

class Cache {
  public:
    void access(uint64_t addr);
    Cache(uint64_t num_lines, uint64_t line_size) : lines_(num_lines), 
      accesses_{0}, hits_{0},
      mask_{numeric_limits<uint64_t>::max() ^ (line_size - 1)} {}

    vector<uint64_t> lines_; // back is mru, front is lru
    long long accesses_;
    long long hits_;
    uint64_t mask_;
};

void Cache::access(uint64_t addr){
  ++accesses_;
  if(debug){
    cout << "Trying to access " << hex << (addr & mask_) << dec << endl;
    cout << "Cache contents: ";
    for(const auto& line : lines_){
      cout << hex << line << dec << " ";
    }
    cout << endl;
  }
  auto line = find(begin(lines_), end(lines_), addr & mask_);
  if(line != end(lines_)){
    if(debug){
      cout << "Hit" << endl;
    }
    ++hits_;
    // move to mru
    lines_.erase(line);
  } else {
    if(debug){
    cout << "Miss" << endl;
    }
    //replace lru
    lines_.erase(begin(lines_));
  }
  lines_.push_back(addr & mask_);
}

static Cache* cache = NULL;

namespace bytesflops{

void initialize_cache(void){
  if(debug){
    cout << "lines: " << bf_cache_lines << "\tsize: " << bf_line_size << endl;
  }
  cache = new Cache(bf_cache_lines, bf_line_size);
  /*
  cache = new Cache(10,1);
  */
}

// Access the cache model with this address.
void bf_touch_cache(uint64_t baseaddr, uint64_t numaddrs){
  for(uint64_t i = 0; i < numaddrs; ++i){
    cache->access(baseaddr + i);
  }
}

} // namespace bytesflops

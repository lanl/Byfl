/*
 * Instrument code to keep track of run-time behavior:
 * Mersenne Twister pseudorandom number generator (interface)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

/*
 * The MT code is a C++ port of the 64-bit version of the
 * Mersenne Twister C code at
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt64.html
 */

#ifndef _MERSENNETWISTER_H_
#define _MERSENNETWISTER_H_

#include <vector>
#include "byfl-common.h"

namespace bytesflops_pass
{

  class MersenneTwister
  {
  public:
    typedef KeyType_t  Value_t;

    MersenneTwister(const std::string& salt);
    MersenneTwister(const Value_t &);

    Value_t next();

  private:
    typedef std::vector<Value_t> ValueList_t;
    ValueList_t mt;
    size_t mti;
    void init(const Value_t &);
  };

} /* namespace bytesflops_pass */

#endif /* _MERSENNETWISTER_H_ */

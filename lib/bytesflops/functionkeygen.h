/*
 * Instrument code to keep track of run-time behavior:
 * generate unique function IDs (interface)
 *
 * By Rob Aulwes <rta@lanl.gov>
 */

#ifndef _FUNCTIONKEYGEN_H_
#define _FUNCTIONKEYGEN_H_

#include <string>
#include "mersennetwister.h"

namespace bytesflops_pass
{
    class FunctionKeyGen
    {
    public:
        typedef MersenneTwister::Value_t    KeyID;
        typedef MersenneTwister::Value_t    Seed_t;

        /**
         * Use this constructor if you will use random keys and
         * want to initialize the RNG with a seed.
         */
        FunctionKeyGen(const Seed_t &);

        KeyID nextRandomKey();

        KeyID generateKey(const std::string &);

    private:
        MersenneTwister     m_rng;
    };

} /* namespace bytesflops_pass */
#endif /* _FUNCTIONKEYGEN_H_ */

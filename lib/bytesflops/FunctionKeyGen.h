/*
 * FunctionKeyGen.h
 *
 *  Created on: Dec 13, 2013
 *      Author: rta
 */

#ifndef FUNCTIONKEYGEN_H_
#define FUNCTIONKEYGEN_H_

#include <string>
#include "MersenneTwister.h"

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
#endif /* FUNCTIONKEYGEN_H_ */

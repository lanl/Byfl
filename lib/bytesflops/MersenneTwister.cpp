/*
 * MersenneTwister.cpp
 *
 *  Created on: Dec 13, 2013
 *      Author: rta
 *
 *  The MT code is a C++ port of the 64-bit version of the
 *  Mersenne Twister C code at
 *  http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt64.html
 */

/*
   A C-program for MT19937-64 (2004/9/29 version).
   Coded by Takuji Nishimura and Makoto Matsumoto.

   This is a 64-bit version of Mersenne Twister pseudorandom number
   generator.

   Before using, initialize the state by using init_genrand64(seed)
   or init_by_array64(init_key, key_length).

   Copyright (C) 2004, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote
        products derived from this software without specific prior written
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   References:
   T. Nishimura, ``Tables of 64-bit Mersenne Twisters''
     ACM Transactions on Modeling and
     Computer Simulation 10. (2000) 348--357.
   M. Matsumoto and T. Nishimura,
     ``Mersenne Twister: a 623-dimensionally equidistributed
       uniform pseudorandom number generator''
     ACM Transactions on Modeling and
     Computer Simulation 8. (Jan. 1998) 3--30.

   Any feedback is very welcome.
   http://www.math.hiroshima-u.ac.jp/~m-mat/MT/emt.html
   email: m-mat @ math.sci.hiroshima-u.ac.jp (remove spaces)
*/

#include <stdlib.h>  // for size_t
#include <algorithm> // for std::max
#include "MersenneTwister.h"

#define NN 312
#define MM 156
#define MATRIX_A 0xB5026F5AA96619E9ULL
#define UM 0xFFFFFFFF80000000ULL /* Most significant 33 bits */
#define LM 0x7FFFFFFFULL /* Least significant 31 bits */

static bytesflops_pass::MersenneTwister::Value_t mag01[2]={0ULL, MATRIX_A};

namespace bytesflops_pass
{

    void MersenneTwister::generateNextSet()
    {
        Value_t x = 0ULL;

        int i;

        for (i = 0; i< NN - MM; i++) {
            x = ( m_state[i]& UM )|( m_state[i+1] & LM );
            m_state[i] = m_state[i+MM] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
        }

        for (; i < NN - 1; i++) {
            x = (m_state[i] & UM)|(m_state[i+1] & LM);
            m_state[i] = m_state[i+(MM-NN)] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
        }

        x = (m_state[NN-1] & UM)|(m_state[0] & LM);
        m_state[NN-1] = m_state[MM-1] ^ (x >> 1) ^ mag01[(int)(x & 1ULL)];
    }

    void MersenneTwister::init(const Value_t & seed)
    {
        m_next_idx = 0;
        m_state[0] = seed;
        for ( size_t i = 1;
                i < m_state.size(); i++ )
        {
            m_state[i] = (6364136223846793005ULL *
                    (m_state[i-1] ^ (m_state[i-1] >> 62)) + i);
        }

        generateNextSet();
   }

    MersenneTwister::MersenneTwister() : m_state(NN)
    {
        init(5489ULL);
    }

    MersenneTwister::MersenneTwister(const Value_t & seed) : m_state(NN)
    {
        init(seed);
    }

    MersenneTwister::MersenneTwister(const ValueList_t & seeds) : m_state(NN)
    {
        Value_t i, j, k;

        init(19650218ULL);

        i = 1;
        j = 0;

        k = std::max((size_t)NN, seeds.size());

        for ( ; k; k--)
        {
            m_state[i] = (m_state[i] ^ ((m_state[i-1] ^ (m_state[i-1] >> 62)) * 3935559000370003845ULL))
              + seeds[j] + j; /* non linear */
            i++; j++;
            if (i >= NN)
            {
                m_state[0] = m_state[NN-1];
                i = 1;
            }
            if (j >= seeds.size()) j = 0;
        }

        for (k = NN - 1; k; k--) {
            m_state[i] = (m_state[i] ^ ((m_state[i-1] ^ (m_state[i-1] >> 62)) * 2862933555777941757ULL))
              - i; /* non linear */
            i++;
            if (i >= NN)
            {
                m_state[0] = m_state[NN-1];
                i = 1;
            }
        }

        m_state[0] = 1ULL << 63; /* MSB is 1; assuring non-zero initial array */

    }

    MersenneTwister::~MersenneTwister()
    {
        // TODO Auto-generated destructor stub
    }

    MersenneTwister::Value_t MersenneTwister::next()
    {
        Value_t x;

        if ( m_next_idx >= NN )
        {
            generateNextSet();
        }

        x = m_state[m_next_idx++];

        x ^= (x >> 29) & 0x5555555555555555ULL;
        x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
        x ^= (x << 37) & 0xFFF7EEE000000000ULL;
        x ^= (x >> 43);

        return x;

    }

} /* namespace bytesflops_pass */

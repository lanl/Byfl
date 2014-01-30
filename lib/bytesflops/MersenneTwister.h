/*
 * MersenneTwister.h
 *
 *  Created on: Dec 13, 2013
 *      Author: rta
 */

#ifndef MERSENNETWISTER_H_
#define MERSENNETWISTER_H_

#include <vector>
#include <stdint.h>

#include "byfl-common.h"

namespace bytesflops_pass
{

    class MersenneTwister
    {
    public:
        typedef KeyType_t  Value_t;
        typedef std::vector<Value_t> ValueList_t;

        MersenneTwister();
        MersenneTwister(const Value_t &);
        MersenneTwister(const ValueList_t &);

        Value_t next();

        ~MersenneTwister();

    private:
        ValueList_t     m_state;
        int             m_next_idx;

        void generateNextSet();
        void init(const Value_t & seed);

    };

} /* namespace bytesflops_pass */
#endif /* MERSENNETWISTER_H_ */

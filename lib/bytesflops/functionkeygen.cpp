/*
 * Instrument code to keep track of run-time behavior:
 * generate unique function IDs
 *
 * By Rob Aulwes <rta@lanl.gov>
 *    Scott Pakin <pakin@lanl.gov>
 */

#include "functionkeygen.h"

namespace bytesflops_pass
{

    /**
     * The Mersenne Twister code is taken from
     * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt64.html
     *
     * This is a 64-bit version of the MT code and has been ported
     * to C++ from the original C.
     */

    FunctionKeyGen::FunctionKeyGen(const std::string& salt)
        : m_rng(salt) {}

    FunctionKeyGen::KeyID FunctionKeyGen::nextRandomKey()
    {
        return m_rng.next();
    }

} /* namespace bytesflops_pass */

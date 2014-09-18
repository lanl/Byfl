/*
 * Instrument code to keep track of run-time behavior:
 * helper methods (interface)
 *
 * By Rob Aulwes <rta@lanl.gov>
 */

#ifndef _HELPERS_H_
#define _HELPERS_H_

#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace bytesflops_pass {
    void mark_as_used(Module& module, Constant* protected_var);

    // Create and initialize a global uint64_t constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           uint64_t value);

    // Create and initialize a global bool constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           bool value);

    // Create and initialize a global char* constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           const char* value);
}

#endif /* _HELPERS_H_ */

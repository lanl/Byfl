/*
 * BytesMP.h
 *
 *  Created on: Mar 6, 2014
 *      Author: rta
 */

#ifndef BYTESMP_H_
#define BYTESMP_H_

#include <string>
#include <map>
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "byfl-common.h"
#include "bytesflops.h"

using namespace llvm;

namespace bytesflops_pass
{

    class BytesMP : public ModulePass
    {
    public:
        static char ID;

        BytesMP() : ModulePass(ID) {}
        ~BytesMP();

        // Insert code for module.
        virtual bool runOnModule(Module & module);

        // Indicate that we need access to DataLayout.
//        virtual void getAnalysisUsage(AnalysisUsage &AU) const {
//            AU.setPreservesAll();
//          ModulePass::getAnalysisUsage(AU);
//          AU.addRequiredTransitive<BytesFlops>();
//        }


    private:

    };

} /* namespace bytesflops_pass */
#endif /* BYTESMP_H_ */

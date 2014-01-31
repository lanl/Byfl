/*
 * CallStack.h
 *
 *  Created on: Dec 16, 2013
 *      Author: rta
 */

#ifndef CALLSTACK_H_
#define CALLSTACK_H_

#include <vector>
//#include <stdlib.h>
#include <string>
#include <cstring>
#include <iostream>

#include "byfl-common.h"

namespace bytesflops
{

    // Maintain a function call stack.
    class CallStack {
    public:
      typedef std::pair<const char*, KeyType_t> StackItem_t;

      size_t max_depth;   // Maximum depth achieved by complete_call_stack

      CallStack() {
        max_depth = 0;
      }

      ~CallStack() {}

      const char* push_function (const char* funcname, KeyType_t key);
      StackItem_t pop_function (void);

      inline size_t depth() {return complete_call_stack.size();}

    private:
        std::vector<StackItem_t> complete_call_stack;  // Stack of function and ancestor names

        //std::vector<std::string> complete_call_stack;  // Stack of function and ancestor names

    };

} /* namespace bytesflops */
#endif /* CALLSTACK_H_ */

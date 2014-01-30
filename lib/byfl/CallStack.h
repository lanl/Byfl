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

namespace bytesflops
{

    // Maintain a function call stack.
    class CallStack {
    private:
        std::vector<const char *> complete_call_stack;  // Stack of function and ancestor names
        //std::vector<std::string> complete_call_stack;  // Stack of function and ancestor names

    public:

      size_t max_depth;   // Maximum depth achieved by complete_call_stack

      CallStack() {
        max_depth = 0;
      }

      ~CallStack() {}

      const char* push_function (const char* funcname);
      const char* pop_function (void);
    };

} /* namespace bytesflops */
#endif /* CALLSTACK_H_ */

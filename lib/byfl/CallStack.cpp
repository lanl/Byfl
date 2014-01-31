/*
 * CallStack.cpp
 *
 *  Created on: Dec 16, 2013
 *      Author: rta
 */

#include "CallStack.h"

extern "C"
const char * bf_string_to_symbol(const char *);


namespace bytesflops
{

    const char* CallStack::push_function (const char* funcname, KeyType_t key) {
        // Push both the current function name and the combined name of the
        // function and its call stack.
        static char* combined_name = NULL;
        static size_t len_combined_name = 0;
        const char* unique_combined_name;      // Interned version of combined_name
        size_t current_stack_depth = complete_call_stack.size();
        if (current_stack_depth == 0) {
          // First function on the call stack
          len_combined_name = strlen(funcname) + 1;
          combined_name = (char*) malloc(len_combined_name);
          strcpy(combined_name, funcname);
          max_depth = 1;
        }
        else {
          // All other calls (the common case)
          const char* ancestors_names = complete_call_stack.back().first;
          size_t length_needed = strlen(funcname) + strlen(ancestors_names) + 2;
          if (len_combined_name < length_needed) {
            len_combined_name = length_needed*2;
            combined_name = (char*) realloc(combined_name, len_combined_name);
          }
          sprintf(combined_name, "%s %s", funcname, ancestors_names);
          current_stack_depth++;
          if (current_stack_depth > max_depth)
            max_depth = current_stack_depth;
        }
        unique_combined_name = bf_string_to_symbol(combined_name);
        complete_call_stack.push_back(std::make_pair(unique_combined_name, key));
        return unique_combined_name;
    }


          // Pop a function name from the call stack and return the new top of
          // the call stack (function + ancestors).
    CallStack::StackItem_t CallStack::pop_function (void) {
        complete_call_stack.pop_back();
        if (complete_call_stack.size() > 0)
        {
          //std::string & nm = complete_call_stack.back();
          return complete_call_stack.back();
        }
        else
          return std::make_pair("[EMPTY]", 0);
      }

} /* namespace bytesflops */

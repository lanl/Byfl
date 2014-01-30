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

    const char* CallStack::push_function (const char* funcname) {
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
          const char* ancestors_names = complete_call_stack.back();
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
        complete_call_stack.push_back(unique_combined_name);
        return unique_combined_name;
    }

          // Push a function name onto the stack.  Return a string containing
          // the name of the function followed by the names of all of its
          // ancestors.
//          const char* CallStack::push_function (const char* funcname) {
//            // Push both the current function name and the combined name of the
//            // function and its call stack.
//            static std::string combined_name;
//
//            const char* unique_combined_name;      // Interned version of combined_name
//            size_t current_stack_depth = complete_call_stack.size();
//            if (current_stack_depth == 0) {
//              // First function on the call stack
//              combined_name = funcname;
//              max_depth = 1;
//            }
//            else {
//              // All other calls (the common case)
//              std::string & ancestors_names = complete_call_stack.back();
//              combined_name = funcname;
//              combined_name += " ";
//              combined_name += ancestors_names;
//              current_stack_depth++;
//              if (current_stack_depth > max_depth)
//                max_depth = current_stack_depth;
//            }
//            unique_combined_name = bf_string_to_symbol(combined_name.c_str());
//            complete_call_stack.push_back(unique_combined_name);
//            std::cout << "Combined name = " << combined_name << ", unique name = " << unique_combined_name << std::endl;
//            return unique_combined_name;
//          }
//
//          // Pop a function name from the call stack and return the new top of
//          // the call stack (function + ancestors).
          const char* CallStack::pop_function (void) {
            complete_call_stack.pop_back();
            if (complete_call_stack.size() > 0)
            {
              //std::string & nm = complete_call_stack.back();
              return complete_call_stack.back();
            }
            else
              return "[EMPTY]";
          }

} /* namespace bytesflops */

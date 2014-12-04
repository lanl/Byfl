/*
 * Common definitions across both the LLVM
 * pass and the helper library
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#ifndef _BYFL_COMMON_H_
#define _BYFL_COMMON_H_

#include "config.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cxxabi.h>
#include <string.h>

using namespace std;

typedef uint64_t KeyType_t;

enum {
  BF_OP_LOAD,
  BF_OP_STORE,
  BF_OP_NUM
};

enum {
  BF_REF_VALUE,
  BF_REF_POINTER,
  BF_REF_NUM
};

enum {
  BF_AGG_SCALAR,
  BF_AGG_VECTOR,
  BF_AGG_NUM
};

enum {
  BF_TYPE_INT,
  BF_TYPE_FP,
  BF_TYPE_OTHER,
  BF_TYPE_NUM
};

enum {
  BF_WIDTH_8,
  BF_WIDTH_16,
  BF_WIDTH_32,
  BF_WIDTH_64,
  BF_WIDTH_128,
  BF_WIDTH_OTHER,
  BF_WIDTH_NUM
};

#define NUM_MEM_INSTS (BF_OP_NUM*BF_REF_NUM*BF_AGG_NUM*BF_TYPE_NUM*BF_WIDTH_NUM)

enum {
  BF_END_BB_ANY,          // Any terminator (i.e., total number of basic blocks)
  BF_END_BB_UNCOND_REAL,  // Unconditional, direct branch (probably required)
  BF_END_BB_UNCOND_FAKE,  // Unconditional, direct branch (needed only for SSA)
  BF_END_BB_COND,         // Conditional branch
  BF_END_BB_INDIRECT,     // Unconditional, indirect branch
  BF_END_BB_SWITCH,       // Switch instruction
  BF_END_BB_RETURN,       // Function-return instruction
  BF_END_BB_NUM
};

enum {
  BF_MEMSET_CALLS,     // Calls to llvm.memset.*
  BF_MEMSET_BYTES,     // Bytes stored by llvm.memset.*
  BF_MEMXFER_CALLS,    // Calls to either llvm.memcpy.* or llvm.memmove.*
  BF_MEMXFER_BYTES,    // Bytes loaded and stored by llvm.mem{cpy,move}.*
  BF_NUM_MEM_INTRIN
};

// Map a memory-access type to an index into bf_mem_insts_count[].
static inline uint64_t
mem_type_to_index(uint64_t memop,
                  uint64_t memref,
                  uint64_t memagg,
                  uint64_t memtype,
                  uint64_t memwidth)
{
  uint64_t idx = 0;
  idx = idx*BF_OP_NUM + memop;
  idx = idx*BF_REF_NUM + memref;
  idx = idx*BF_AGG_NUM + memagg;
  idx = idx*BF_TYPE_NUM + memtype;
  idx = idx*BF_WIDTH_NUM + memwidth;
  return idx;
}

// Attempt to demangle a space-separated list of function names so the
// masses can follow along.  Elements in the resulting string are
// separated by hash marks as these can't (?) appear in C++ function
// names or argument types.
#pragma GCC diagnostic ignored "-Wunused-function"
static std::string
demangle_func_name(std::string mangled_name_list)
{
  std::istringstream mangled_stream(mangled_name_list);  // Stream of mangled names
  std::string mangled_name;      // A single mangled name
  char idelim = ' ';             // Delimiter between input mangled names
  string odelim(" # ");          // Delimiter between output demangled names
  int status;                    // Status of demangling a name
  std::ostringstream demangled_stream;    // Stream of demangled names
  bool first_name = true;        // true=first name in list; false=subsequent name
  while (std::getline(mangled_stream, mangled_name, idelim)) {
    if (first_name)
      first_name = false;
    else
      demangled_stream << odelim;
    char* demangled_name = __cxxabiv1::__cxa_demangle(mangled_name.c_str(), NULL, 0, &status);
    if (status == 0 && demangled_name != 0)
      demangled_stream << demangled_name;
    else
      demangled_stream << mangled_name;
  }
  return demangled_stream.str();
}

// Read /proc/self/cmdline and parse it into a vector of strings.  If
// /proc/self/cmdline doesn't exist or can't be read, return a dummy
// string instead.
#pragma GCC diagnostic ignored "-Wunused-function"
static std::vector<std::string>
parse_command_line (void)
{
  // Open the command-line pseudo-file.
  std::vector<std::string> arglist;   // Vector of arguments to return
  ifstream cmdline("/proc/self/cmdline");   // Full command line passed to opt
  string failed_read("[failed to read /proc/self/cmdline]");  // Error return
  if (!cmdline.is_open()) {
    arglist.push_back(failed_read);
    return arglist;
  }

  // Read the entire command line into a buffer.
  const size_t maxcmdlinelen = 131072;     // Maximum buffer space we're willing to allocate
  char cmdline_chars[maxcmdlinelen] = {0};
  cmdline.read(cmdline_chars, maxcmdlinelen);
  cmdline.close();

  // Parse the command line.  Each argument is terminated by a null
  // character, and the command line as a whole is terminated by two
  // null characters.
  if (cmdline.bad()) {
    arglist.push_back(failed_read);
    return arglist;
  }
  char* arg = cmdline_chars;
  while (1) {
    size_t arglen = strlen(arg);
    if (arglen == 0)
      break;
    arglist.push_back(arg);
    arg += arglen + 1;
  }
  return arglist;
}

#endif

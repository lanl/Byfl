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
#include <unistd.h>
#ifdef HAVE_CRT_EXTERNS_H
# include <crt_externs.h>
#endif

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
  BF_END_BB_COND_NT,      // Conditional branch (not taken)
  BF_END_BB_COND_T,       // Conditional branch (taken)
  BF_END_BB_INDIRECT,     // Unconditional, indirect branch
  BF_END_BB_SWITCH,       // Switch instruction
  BF_END_BB_RETURN,       // Function-return instruction
  BF_END_BB_INVOKE,       // Function-invocation instruction (supporting exceptions)
  BF_END_BB_NUM
};

enum {
  BF_MEMSET_CALLS,     // Calls to llvm.memset.*
  BF_MEMSET_BYTES,     // Bytes stored by llvm.memset.*
  BF_MEMXFER_CALLS,    // Calls to either llvm.memcpy.* or llvm.memmove.*
  BF_MEMXFER_BYTES,    // Bytes loaded and stored by llvm.mem{cpy,move}.*
  BF_NUM_MEM_INTRIN
};

// Define constants for "constant operand" and "no operand" for
// instruction-dependency reporting.
enum {
  BF_CONST_ARG = NUM_LLVM_OPCODES,      // Constant operand
  BF_NO_ARG    = NUM_LLVM_OPCODES + 1   // No operand
};

// Define a type for communicating symbol information from the plugin
// to the run-time library.
typedef struct {
  uint64_t ID;           // Unique identifier for the symbol
  const char *origin;    // Who allocated the symbol
  const char *symbol;    // Symbol name
  const char *function;  // Name of function containing the symbol
  const char *file;      // Name of directory+file containing the symbol
  unsigned int line;     // Line number at which the symbol appears
} bf_symbol_info_t;

// Map a memory-access type to an index into bf_mem_insts_count[].
static inline uint64_t mem_type_to_index(uint64_t memop,
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

// Strip a "@@<version>" suffix from a symbol name.  It seems that
// __cxa_demangle() can handle names like "_ZSt4cout" but gets confused by
// names like "_ZSt4cout@@GLIBCXX_3.4".
#pragma GCC diagnostic ignored "-Wunused-function"
static std::string strip_atat(string name)
{
  size_t atat_pos = name.find("@@");
  if (atat_pos != string::npos)
    name.erase(atat_pos);
  return name;
}

// Replace a "_GLOBAL__sub_I_" (respectively "_GLOBAL__sub_D_") prefix in a
// symbol name with "_GLOBAL__sub_I_" (respectively "_GLOBAL__sub_D_").
// __cxa_demangle() has no idea what the former is -- and I haven't yet found
// any documentation to tell me -- but the latter is documented as global
// constructors (respectively destructors) keyed to the function.  I'm guessing
// that the "sub" form is some simple variant of that.
#pragma GCC diagnostic ignored "-Wunused-function"
static std::string strip_global_sub(string name)
{
  if (name.compare(0, 13, "_GLOBAL__sub_") == 0)
    name = string("_GLOBAL__") + name.substr(13);
  return name;
}

// Attempt to demangle a space-separated list of function names so the masses
// can follow along.  Elements in the resulting string are separated by hash
// marks as these can't (?) appear in C++ function names or argument types.
// Note that, despite its name, demangle_func_name() can also demangle other
// symbol types as well.
#pragma GCC diagnostic ignored "-Wunused-function"
static std::string
demangle_func_name(std::string mangled_name_list)
{
  // First, check if we were given a line of LLVM IR, which Byfl sometimes uses
  // to represent unnamed code points.
  if (mangled_name_list.find_first_of('%') != string::npos) {
    // mangled_name_list looks like LLVM IR -- extract the opcode name.
    size_t op_begin;     // Position of opcode name
    op_begin = mangled_name_list.find(" = ");
    if (op_begin == string::npos)
      // Not found -- opcode must begin the string
      op_begin = 0;
    else
      // Found -- skip past the " = " to find the opcode name.
      op_begin += 3;
    size_t op_end;      // Position of end of opcode name
    op_end = mangled_name_list.find_first_of(" \t\n\r", op_begin);
    string demangled_name = string("LLVM ") + mangled_name_list.substr(op_begin, op_end - op_begin) + string(" instruction");

    // If the instruction references a function (e.g., @_Znwm)) or a named
    // register (e.g., %"class foo" or %__foo), append that information to the
    // demangled name.
    const string llvm_var_chars("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.");  // Valid characters for an LLVM name
    size_t func_begin = mangled_name_list.find('@', op_begin);
    if (func_begin != string::npos) {
      // Function reference
      func_begin++;
      size_t func_end = mangled_name_list.find_first_not_of(llvm_var_chars, func_begin);
      if (func_end > func_begin) {
        string func_name = demangle_func_name(mangled_name_list.substr(func_begin, func_end - func_begin));
        return demangled_name + string(" referencing ") + func_name;
      }
    }
    size_t reg_begin = mangled_name_list.find("%\"", op_begin);
    if (reg_begin != string::npos) {
      // Quoted named register
      reg_begin += 2;
      size_t reg_end = mangled_name_list.find_first_of('"', reg_begin);
      if (reg_end > reg_begin) {
        string reg_name = mangled_name_list.substr(reg_begin, reg_end - reg_begin);
        return demangled_name + string(" referencing ") + reg_name;
      }
    }
    reg_begin = mangled_name_list.find("%", op_begin);
    if (reg_begin != string::npos &&
        mangled_name_list.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.", reg_begin + 1) == reg_begin + 1) {
      // Unquoted named register
      reg_begin++;
      size_t reg_end = mangled_name_list.find_first_not_of(llvm_var_chars, reg_begin);
      string reg_name = mangled_name_list.substr(reg_begin, reg_end - reg_begin);
      return demangled_name + string(" referencing ") + reg_name;
    }
    return demangled_name;
  }

  // We have an ordinary symbol name.  Try various approaches to demangle it.
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
    if (mangled_name.compare(0, 2, "_Z") == 0 ||
        mangled_name.compare(0, 13, "_GLOBAL__sub_") == 0) {
      // mangled_name looks like a g++ mangled name.
      char* demangled_name =
        __cxxabiv1::__cxa_demangle(strip_global_sub(strip_atat(mangled_name)).c_str(),
                                   NULL, 0, &status);
      if (status == 0 && demangled_name != 0)
        demangled_stream << demangled_name;
      else
        demangled_stream << mangled_name;
    }
    else {
      size_t mod_ofs = mangled_name.find("_MOD_");
      if (mod_ofs != string::npos && mangled_name.compare(0, 2, "__") == 0) {
        // mangled_name looks like a gfortran mangled name.
        demangled_stream << mangled_name.substr(2, mod_ofs - 2)  // Module name
                         << "::"                                 // Separator
                         << mangled_name.substr(mod_ofs + 5);    // Function name
      }
      else
        // mangled_name does not look like a g++ or gfortran mangled name.
        // Don't let __cxa_demangle() screw up mangled_name (e.g., by
        // demangling "f" to "float").  However, do strip "@@<version>" from
        // the end of the symbol name.
        demangled_stream << strip_atat(mangled_name);
    }
  }
  return demangled_stream.str();
}

// Parse the command line into a vector of strings.  We acquire the command
// line via the /proc/self/cmdline file on Linux and via the _NSGetArgv() call
// on OS X.  On failure, return a dummy string.
#pragma GCC diagnostic ignored "-Wunused-function"
static std::vector<std::string>
parse_command_line (void)
{
  std::vector<std::string> arglist;   // Vector of arguments to return

#ifdef HAVE__NSGETARGV
  // Get the command line from _NSGetArgv() and _NSGetArgc() (OS X).
  char ***argvp = _NSGetArgv();
  int *argcp = _NSGetArgc();
  if (argvp != nullptr && argcp != nullptr) {
    for (int i = 0; i < *argcp; i++)
      arglist.push_back((*argvp)[i]);
    return arglist;
  }
#endif

  // Open the command-line pseudo-file.
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

// Canonicalize a file name and convert it to an absolute path.  The original
// string will be returned on error.  The value returned should be considered
// ephemeral.
#pragma GCC diagnostic ignored "-Wunused-function"
static char *absolute_file_name(const char* filename)
{
  // Determine the maximum path length.
  static ssize_t path_max = 0;
  if (path_max == 0) {
#ifdef PATH_MAX
    path_max = PATH_MAX;
#else
    path_max = pathconf(filename, _PC_PATH_MAX);
    if (path_max <= 0)
      path_max = 4096;
#endif
  }

  // Allocate space in which to work.
  static char *old_path = nullptr;
  static char *new_path = nullptr;
  if (old_path == nullptr) {
    old_path = new char[path_max + 1];
    new_path = new char[path_max + 1];
  }

  // realpath() seems to get tripped up by "//".  Remove those.
  strcpy(old_path, filename);
  char *dslash;
  while ((dslash = strstr(old_path, "//")) != nullptr)
    memmove(old_path, dslash + 1, strlen(dslash));   // Include the '\0'.

  // Return the canonical absolute path.
  if (realpath(old_path, new_path) != nullptr)
    return new_path;
  else
    return old_path;
}

#endif

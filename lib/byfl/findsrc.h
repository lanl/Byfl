/*
 * Find the source-code location associated with an instruction address
 * (class declaration)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _FINDSRC_H_
#define _FINDSRC_H_

#include "byfl.h"

// The entire ProcessSymbolTable class requires libbfd.
#ifdef USE_BFD

#include <bfd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

// Represent a source-code location.
class SourceCodeLocation
{
public:
  uint64_t address;
  string file_name;
  string function_name;
  uint64_t line_number;
  uint64_t discriminator;   // Used when one line maps to multiple basic blocks

  SourceCodeLocation(string fname, string funcname,
                     uint64_t lineno, uint64_t discrim) {
    file_name = fname;
    function_name = funcname;
    line_number = lineno;
    discriminator = discrim;
  }
};

// Represent the symbol table of the current process.
class ProcessSymbolTable
{
private:
  bool valid;          // true=object is usable; false=all methods will fail
  bfd* bfd_self;       // Descriptor for the calling process
  asymbol **symtable;  // The calling process's symbol table
  ssize_t numsyms;     // Number of symbols in the above

  struct CacheEntry {
    uint64_t address;              // Instruction address
    SourceCodeLocation* location;  // Source-code location to return

    CacheEntry() : address(0), location(nullptr) {}
  };

  static const size_t cache_capacity = 100;     // Maximum number of cache entries
  size_t cache_length;                   // Currently valid cache entries
  CacheEntry cache[cache_capacity];      // Cache of prior lookups
  SourceCodeLocation* find_address_in_sym_table (uint64_t addr);

public:
  ProcessSymbolTable (void);
  ~ProcessSymbolTable (void);
  SourceCodeLocation* find_address (uint64_t addr);
  void get_raw_bfd_data (bfd** abfd, asymbol*** syms, ssize_t* nsyms);
};

#endif

#endif

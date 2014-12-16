/*
 * Find the source-code location associated with an instruction address
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#ifndef _FINDSRC_H_
#define _FINDSRC_H_

#include "byfl.h"

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
};

ProcessSymbolTable::ProcessSymbolTable (void) : valid(false), bfd_self(NULL), symtable(NULL), cache_length(0)
{
  // Initialize the BFD library.
  bfd_init();
  // Do I need to call bfd_set_default_target()?
  bfd_self = bfd_openr("/proc/self/exe", NULL);
  if (bfd_self == NULL)
    return;
  bfd_self->flags |= BFD_DECOMPRESS;
  char **matching = NULL;
  if (!bfd_check_format_matches (bfd_self, bfd_object, &matching)) {
    free(matching);
    return;
  }

  // Read the symbol table.
  if ((bfd_get_file_flags (bfd_self) & HAS_SYMS) == 0)
    return;
  ssize_t bytes_needed = bfd_get_symtab_upper_bound(bfd_self);
  if (bytes_needed <= 0)
    return;
  symtable = new asymbol*[bytes_needed/sizeof(asymbol*)];
  ssize_t numsyms = bfd_canonicalize_symtab(bfd_self, symtable);
  if (numsyms < 0)
    return;

  // If we made it this far, we ought to be able to translate addresses.
  valid = true;
}

ProcessSymbolTable::~ProcessSymbolTable (void)
{
  if (symtable != NULL)
    delete[] symtable;
  if (bfd_self == NULL)
    return;
  (void) bfd_close(bfd_self);
}

// Given an instruction address, return the corresponding source-code
// filename and line number.
SourceCodeLocation* ProcessSymbolTable::find_address_in_sym_table (uint64_t addr)
{
  // Do nothing if we're not in a good state.
  if (!valid)
    return NULL;

  // Search each section in turn for the given address.
  for (asection* sec = bfd_self->sections; sec != NULL ; sec = sec->next) {
    // See if the given instruction address lies within the current section.
    if ((bfd_get_section_flags(bfd_self, sec) & SEC_ALLOC) == 0)
      // Section is not allocated.
      continue;
    bfd_vma vma = bfd_get_section_vma(bfd_self, sec);
    if (addr < vma)
      // Instruction address precedes the beginning of the section.
      continue;
    bfd_size_type vma_size = bfd_get_section_size(sec);
    if (addr >= vma + vma_size)
      // Instruction address follows the end of the section.
      continue;

    // The address appears within the current section.  Look up the
    // associated source-code location.
    const char *file_name;
    const char *function_name;
    unsigned int line_number;
    unsigned int discriminator;
    if (!bfd_find_nearest_line_discriminator(bfd_self, sec, symtable, addr - vma,
                                             &file_name, &function_name,
                                             &line_number, &discriminator))
      return NULL;
    return new SourceCodeLocation(file_name == nullptr ? "??" : file_name,
                                  function_name == nullptr ? "??" : function_name,
                                  line_number,
                                  discriminator);
  }
  return NULL;
}

// Wrap find_address_in_sym_table() with a version that caches results.
SourceCodeLocation* ProcessSymbolTable::find_address (uint64_t addr)
{
  // See if the address is already cached.
  for (size_t i = 0; i < cache_length; i++)
    if (cache[i].address == addr) {
      // We found the address -- bubble up its cache entry by one slot.
      CacheEntry found = cache[i];
      if (i > 0) {
        cache[i] = cache[i - 1];
        cache[i - 1] = found;
      }
      return found.location;
    }

  // We didn't find the address.  Look it up in the symbol table and
  // shift the result into the first slot on the assumption that we'll
  // be seeing a lot more of that address in the near future.
  if (cache_length < cache_capacity)
    cache_length++;
  else
    // Free the memory we allocated for the last slot in the cache.
    if (cache[cache_capacity - 1].location != nullptr)
      delete cache[cache_capacity - 1].location;
  for (size_t i = cache_length - 1; i > 0; i--)
    cache[i] = cache[i - 1];
  cache[0].address = addr;
  cache[0].location = find_address_in_sym_table(addr);
  return cache[0].location;
}

#endif

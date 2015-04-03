/*
 * Find the source-code location associated with an instruction address
 * (class implementation)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"

// The entire ProcessSymbolTable class requires libbfd.
#ifdef USE_BFD

#include "findsrc.h"

ProcessSymbolTable::ProcessSymbolTable (void) : valid(false), bfd_self(nullptr), symtable(nullptr), numsyms(0), cache_length(0)
{
  // Initialize the BFD library.
  bfd_init();
  // Do I need to call bfd_set_default_target()?
  bfd_self = bfd_openr("/proc/self/exe", NULL);
  if (bfd_self == NULL)
    return;
#ifdef BFD_DECOMPRESS
  bfd_self->flags |= BFD_DECOMPRESS;
#endif
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
  numsyms = bfd_canonicalize_symtab(bfd_self, symtable);
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
#if HAVE_DECL_BFD_FIND_NEAREST_LINE_DISCRIMINATOR
    if (!bfd_find_nearest_line_discriminator(bfd_self, sec, symtable, addr - vma,
                                             &file_name, &function_name,
                                             &line_number, &discriminator))
      return NULL;
#else
    if (!bfd_find_nearest_line(bfd_self, sec, symtable, addr - vma,
                               &file_name, &function_name, &line_number))
      return NULL;
    discriminator = 0;
#endif
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

// Return the raw BFD information the symbol table represents.
void ProcessSymbolTable::get_raw_bfd_data (bfd** abfd, asymbol*** syms,
                                           ssize_t* nsyms)
{
  *abfd = bfd_self;
  *syms = symtable;
  *nsyms = numsyms;
}

#endif

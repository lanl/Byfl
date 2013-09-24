/*
 * Helper library for computing bytes:flops ratios
 * (symbol table manipulation)
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "byfl.h"

using namespace std;

// Say whether one string is strictly less than another.
struct str_less_than
{
  bool operator()(const char* s1, const char* s2) const {
    if (s1 == s2)
      return false;
    return strcmp(s1, s2) < 0;
  }
};

namespace bytesflops {

// Define a map from equal strings to equivalent strings.
typedef map<const char*, const char*, str_less_than> symbol_table_t;
static symbol_table_t* symbol_table = NULL;


// Initialize some of our variables at first use.
void initialize_symtable (void) {
  symbol_table = new symbol_table_t();
}


// Map a nonunique string to a unique string (in other words, intern a
// string to a symbol).
const char* bf_string_to_symbol (const char* nonunique)
{
  if (nonunique == NULL)
    return NULL;
  symbol_table_t::iterator sym_iter = symbol_table->find(nonunique);
  if (sym_iter == symbol_table->end()) {
    // New entry for the symbol table -- create a unique symbol and return it.
    const char* unique = strdup(nonunique);
    (*symbol_table)[unique] = unique;
    return unique;
  }
  else
    // Existing entry in the symbol table -- return it.
    return sym_iter->second;
}

} // namespace bytesflops

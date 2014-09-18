/*
 * Helper library for computing bytes:flops ratios
 * (tracking vector operations)
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "byfl.h"

namespace bytesflops {}
using namespace bytesflops;
using namespace std;

// Encapsulate all interesting information about a vector operation.
class VectorOperation {
public:
  uint64_t num_elements;   // Number of scalar elements in the vector operation
  uint64_t element_bits;   // Number of bits per scalar element
  bool is_flop;            // True=floating-point operation; false=integer operation

  // Initialize all of our values.
  VectorOperation (uint64_t initial_elts=0, uint64_t initial_bits=0, bool initial_fp=false) {
    num_elements = initial_elts;
    element_bits = initial_bits;
    is_flop = initial_fp;
  }

  // Assign us new values.
  void set (uint64_t new_elts=0, uint64_t new_bits=0, bool new_fp=false) {
    num_elements = new_elts;
    element_bits = new_bits;
    is_flop = new_fp;
  }
};

// Define a number of functors for use in constructing various hash tables.
struct eqvec
{
  bool operator()(const VectorOperation* v1, const VectorOperation* v2) const {
    if (v1 == v2)
      return true;
    if (v1 == NULL || v2 == NULL)
      return false;
    if (v1->num_elements != v2->num_elements)
      return false;
    if (v1->element_bits != v2->element_bits)
      return false;
    if (v1->is_flop != v2->is_flop)
      return false;
    return true;
  }
};
struct hashvec
{
  size_t operator()(const VectorOperation* v) const {
    size_t result = (size_t) v->num_elements;
    result *= 1099511628211ULL;
    result += v->element_bits;
    result *= 14695981039346656037ULL;
    result += v->is_flop;
    return result;
  }
};


// Define a mapping from a function name to a vector operation to a tally.
typedef CachedUnorderedMap<VectorOperation*, uint64_t, hashvec, eqvec> vector_to_tally_t;
typedef CachedUnorderedMap<const char*, vector_to_tally_t*> name_to_vector_t;

// Keep track of the number of vector operations performed by each
// function and by each chunk of code defined by the user.
static name_to_vector_t* function_vector_usage = NULL;
static name_to_vector_t* user_defined_vector_usage = NULL;


namespace bytesflops {

extern ostream* bfout;


// Initialize some of our variables at first use.
void initialize_vectors (void)
{
  function_vector_usage = new name_to_vector_t();
  user_defined_vector_usage = new name_to_vector_t();
}


// Associate a vector operation with a given name.
static void tally_vector_operation (name_to_vector_t* vector_usage,
                                    const char *tag, uint64_t num_elements,
                                    uint64_t element_bits, bool is_flop)
{
  // Find or create the associated vector-to-tally mapping.
  name_to_vector_t::iterator vectally_iter = vector_usage->find(tag);
  if (vectally_iter == vector_usage->end()) {
    // This is the first time we've seen this tag.  Give it a fresh
    // map and return.
    vector_to_tally_t* newvectally = new vector_to_tally_t();
    (*newvectally)[new VectorOperation(num_elements, element_bits, is_flop)] = 1;
    (*vector_usage)[tag] = newvectally;
    return;
  }
  vector_to_tally_t* vectally = vectally_iter->second;

  // Increment the tally.
  static VectorOperation* search_vector = new VectorOperation();
  search_vector->set(num_elements, element_bits, is_flop);
  vector_to_tally_t::iterator tally_iter = vectally->find(search_vector);
  if (tally_iter == vectally->end()) {
    // This is the first time we've seen this vector type in the
    // current tag.  Create an initial tally and return.
    (*vectally)[search_vector] = 1;
    search_vector = new VectorOperation();
    return;
  }
  tally_iter->second++;
}

extern "C"
void bf_tally_vector_operation (const char *funcname, uint64_t num_elements,
                                uint64_t element_bits, bool is_flop)
{
  // Find the given function's mapping from vector to tally and increment that.
  if (bf_per_func)
    if (bf_call_stack)
      funcname = bf_func_and_parents;
    else
      funcname = bf_string_to_symbol(funcname);
  else
    funcname = "";
  tally_vector_operation(function_vector_usage, funcname, num_elements, element_bits, is_flop);

  // Also tally according to the user's specified data-partitioning scheme.
  const char* partition = bf_string_to_symbol(bf_categorize_counters());
  if (partition != NULL)
    tally_vector_operation(user_defined_vector_usage, partition, num_elements, element_bits, is_flop);
}


// Acquire statistics on all vector operations encountered.
void bf_get_vector_statistics(uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits) {
  *num_ops = *total_elts = *total_bits = 0;
  for (name_to_vector_t::iterator vectally_iter = function_vector_usage->begin();
       vectally_iter != function_vector_usage->end();
       vectally_iter++) {
    vector_to_tally_t* vectally = vectally_iter->second;
    for (vector_to_tally_t::iterator tally_iter = vectally->begin();
         tally_iter != vectally->end();
         tally_iter++) {
      VectorOperation* vecop = tally_iter->first;
      uint64_t tally = tally_iter->second;
      *num_ops += tally;
      *total_elts += vecop->num_elements * tally;
      *total_bits += vecop->element_bits * tally;
    }
  }
}

// Acquire statistics on all vector operations encountered by a
// specific user-defined partition.
void bf_get_vector_statistics(const char* tag, uint64_t* num_ops, uint64_t* total_elts, uint64_t* total_bits) {
  *num_ops = *total_elts = *total_bits = 0;
  name_to_vector_t::iterator nv_iter = user_defined_vector_usage->find(tag);
  if (nv_iter == user_defined_vector_usage->end())
    return;
  vector_to_tally_t* vectally = nv_iter->second;
  for (vector_to_tally_t::iterator tally_iter = vectally->begin();
       tally_iter != vectally->end();
       tally_iter++) {
    VectorOperation* vecop = tally_iter->first;
    uint64_t tally = tally_iter->second;
    *num_ops += tally;
    *total_elts += vecop->num_elements * tally;
    *total_bits += vecop->element_bits * tally;
  }
}


// Output a histogram of all vector operations encountered.
void bf_report_vector_operations (size_t call_stack_depth)
{
  // Output a header line.
  *bfout << bf_output_prefix
         << "BYFL_VECTOR_HEADER: "
         << setw(20) << "Elements" << ' '
         << setw(20) << "Elt_bits" << ' '
         << setw(4)  << "Type" << ' '
         << setw(20) << "Tally";
  if (bf_per_func) {
    *bfout << " Function";
    if (bf_call_stack)
      for (size_t i=0; i<call_stack_depth-1; i++)
        *bfout << ' '
               << "Parent_func_" << i+1;
  }
  *bfout << '\n';

  // Output a histogram.  Each line represents one set of vector
  // characteristics from one function.
  for (name_to_vector_t::iterator vectally_iter = function_vector_usage->begin();
       vectally_iter != function_vector_usage->end();
       vectally_iter++) {
    const char* funcname = vectally_iter->first;
    vector_to_tally_t* vectally = vectally_iter->second;
    for (vector_to_tally_t::iterator tally_iter = vectally->begin();
         tally_iter != vectally->end();
         tally_iter++) {
      VectorOperation* vecop = tally_iter->first;
      uint64_t tally = tally_iter->second;
      *bfout << bf_output_prefix
             << "BYFL_VECTOR:        "
             << setw(20) << vecop->num_elements << ' '
             << setw(20) << vecop->element_bits << ' '
             << (vecop->is_flop ? "FP   " : "Int   ")
             << setw(20) << tally;
      if (bf_per_func)
        *bfout << ' ' << funcname;
      *bfout << '\n';
    }
  }
}

} // namespace bytesflops

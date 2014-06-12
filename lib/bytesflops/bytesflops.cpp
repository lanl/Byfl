/*
 * Instrument code to keep track of run-time behavior:
 * top-level declarations for the LLVM pass
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "bytesflops.h"

namespace bytesflops_pass {
  // Define a command-line option for outputting results at the end of
  // every basic block instead of only once at the end of the program.
  cl::opt<bool>
  InstrumentEveryBB("bf-every-bb", cl::init(false), cl::NotHidden,
                    cl::desc("Output byte and flop counts at the end of every basic block"));

  // Define a command-line option for aggregating measurements by
  // function name.
  cl::opt<bool>
  TallyByFunction("bf-by-func", cl::init(false), cl::NotHidden,
                  cl::desc("Associate byte and flop counts with each function"));

  // Define a command-line option for outputting not only function
  // names but also immediate parents.
  cl::opt<bool>
  TrackCallStack("bf-call-stack", cl::init(false), cl::NotHidden,
                 cl::desc("Additionally output the name of each function's parent"));

  // Define a command-line option for keeping track of unique bytes
  cl::opt<bool>
  TrackUniqueBytes("bf-unique-bytes", cl::init(false), cl::NotHidden,
                   cl::desc("Tally unique bytes accessed"));

  // Define a command-line option for keeping track of unique bytes
  cl::opt<bool>
  FindMemFootprint("bf-mem-footprint", cl::init(false), cl::NotHidden,
		   cl::desc("Tabulate the minimum amount of memory needed for various cache hit rates"));

  // Define a command-line option for tallying load/store operations
  // based on various data types (note this also implies --bf-all-ops).
  cl::opt<bool>
  TallyTypes("bf-types", cl::init(false), cl::NotHidden,
             cl::desc("Tally type information with loads and stores (note this flag enables --bf-all-ops)."));

  // Define a command-line option for tallying overall instruction mix histogram creation.
  cl::opt<bool>
  TallyInstMix("bf-inst-mix", cl::init(false), cl::NotHidden,
               cl::desc("Tally total count for each individual instruction (instruction mix)."));

  // Define a command-line option for merging basic-block measurements
  // to reduce the output volume.
  cl::opt<unsigned long long>
  BBMergeCount("bf-merge-bb", cl::init(1), cl::NotHidden,
               cl::desc("Merge this many basic blocks into a single line of output"),
               cl::value_desc("count"));

  // Define a command-line option to accept a list of functions to
  // instrument, ignoring all others.
  cl::list<string>
  IncludedFunctions("bf-include", cl::NotHidden, cl::ZeroOrMore, cl::CommaSeparated,
               cl::desc("Instrument only the functions in the given list"),
               cl::value_desc("function,..."));

  // Define a command-line option to accept a list of functions not to
  // instrument, including all others.
  cl::list<string>
  ExcludedFunctions("bf-exclude", cl::NotHidden, cl::ZeroOrMore, cl::CommaSeparated,
               cl::desc("Do not instrument the functions in the given list"),
               cl::value_desc("function,..."));

  // Define a command-line option for enabling thread safety (at the
  // cost of increasing execution time).
  cl::opt<bool>
  ThreadSafety("bf-thread-safe", cl::init(false), cl::NotHidden,
               cl::desc("Generate slower but thread-safe instrumentation"));

  // Define a command-line option for tallying vector operations.
  cl::opt<bool>
  TallyVectors("bf-vectors", cl::init(false), cl::NotHidden,
               cl::desc("Tally vector lengths and counts"));

  // Define a command-line option for tracking reuse distance.
  cl::bits<ReuseDistType>
  ReuseDist("bf-reuse-dist", cl::NotHidden, cl::CommaSeparated, cl::ValueOptional,
            cl::desc("Keep track of data reuse distance"),
            cl::values(clEnumValN(RD_LOADS,  "loads",  "Keep track of loads"),
                       clEnumValN(RD_STORES, "stores", "Keep track of stores"),
                       clEnumValN(RD_BOTH,   "",       "Keep track of both loads and stores"),
                       clEnumValEnd));
  unsigned int rd_bits = 0;    // Same as ReuseDist.getBits() but with RD_BOTH expanded

  // Define a command-line option for pruning reuse distance.
  cl::opt<unsigned long long>
  MaxReuseDist("bf-max-rdist", cl::init(~(unsigned long long)0 - 1),
               cl::NotHidden,
               cl::desc("Treat addresses not touched after this many accesses as untouched"),
               cl::value_desc("accesses"));

  // Define a command-line option for turning on the cache model.
  cl::opt<bool>
  CacheModel("bf-cache-model", cl::init(false), cl::NotHidden,
             cl::desc("Model memory accesses to a simple cache."));

  // Define a command-line option to specify the number of lines in the simple 
  // cache model.
  cl::opt<unsigned long long>
  NumCacheLines("bf-cache-lines", cl::init(4), cl::NotHidden,
                cl::desc("Number of lines used by the simple cache model when turned on."),
                cl::value_desc("lines"));

  // Define a command-line option to specify the size in bytes of the cache
  // lines the simple cache model uses.
  cl::opt<unsigned long long>
  CacheLineBytes("bf-line-size", cl::init(64), cl::NotHidden,
                 cl::desc("Size, in bytes, of the cache lines used by the simple cache model."),
                 cl::value_desc("size"));

  static RegisterPass<BytesFlops> H("bytesflops", "Bytes:flops instrumentation");

}  // namespace bytesflops_pass

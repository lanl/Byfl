/*
 * Instrument code to keep track of run-time behavior:
 * LLVM pass class definition
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include "byfl-common.h"

using namespace std;
using namespace llvm;

namespace bytesflops_pass {

  // Define a command-line option for outputting results at the end of
  // every basic block instead of only once at the end of the program.
  extern cl::opt<bool> InstrumentEveryBB;

  // Define a command-line option for aggregating measurements by
  // function name.
  extern cl::opt<bool> TallyByFunction;

  // Define a command-line option for outputting not only function
  // names but also immediate parents.
  extern cl::opt<bool> TrackCallStack;

  // Define a command-line option for keeping track of unique bytes.
  extern cl::opt<bool> TrackUniqueBytes;

  // Define a command-line option for helping find a program's
  // working-set size.
  extern cl::opt<bool> FindMemFootprint;

  // Define a command-line option for tallying load/store operations
  // based on various data types.
  extern cl::opt<bool> TallyTypes;

  // Define a command-line option for tallying a histogram of the
  // occurrence of individual instructions within the code; aka the
  // instruction mix.
  extern cl::opt<bool> TallyInstMix;

  // Define a command-line option for merging basic-block measurements
  // to reduce the output volume.
  extern cl::opt<unsigned long long> BBMergeCount;

  // Define a command-line option to accept a list of functions to
  // instrument, ignoring all others.
  extern cl::list<string> IncludedFunctions;

  // Define a command-line option to accept a list of functions not to
  // instrument, including all others.
  extern cl::list<string> ExcludedFunctions;

  // Define a command-line option for enabling thread safety (at the
  // cost of increasing execution time).
  extern cl::opt<bool> ThreadSafety;

  // Define a command-line option for tallying vector operations.
  extern cl::opt<bool> TallyVectors;

  // Define a command-line option for tracking reuse distance.
  typedef enum {RD_LOADS, RD_STORES, RD_BOTH} ReuseDistType;
  extern cl::bits<ReuseDistType> ReuseDist;
  extern unsigned int rd_bits;    // Same as ReuseDist.getBits() but with RD_BOTH expanded

  // Define a command-line option for pruning reuse distance.
  extern cl::opt<unsigned long long> MaxReuseDist;

  // Destructively remove all instances of a given character from a string.
  extern void remove_all_instances(string& some_string, char some_char);

  // Parse a list of function names into a set.  The trick is that
  // demangled C++ function names are split (at commas) across list
  // elements and need to be recombined.
  extern set<string>* parse_function_names(vector<string>& funclist);


  // Define a pass over each basic block in the module.
  struct BytesFlops : public FunctionPass {

  private:
    static const int CLEAR_LOADS;
    static const int CLEAR_FLOAT_LOADS;
    static const int CLEAR_DOUBLE_LOADS;
    static const int CLEAR_INT_LOADS;
    static const int CLEAR_PTR_LOADS;
    static const int CLEAR_OTHER_TYPE_LOADS;

    static const int CLEAR_STORES;
    static const int CLEAR_FLOAT_STORES;
    static const int CLEAR_DOUBLE_STORES;
    static const int CLEAR_INT_STORES;
    static const int CLEAR_PTR_STORES;
    static const int CLEAR_OTHER_TYPE_STORES;

    static const int CLEAR_FLOPS;
    static const int CLEAR_FP_BITS;
    static const int CLEAR_OPS;
    static const int CLEAR_OP_BITS;

    static const int CLEAR_MEM_TYPES;

    GlobalVariable* load_var;  // Global reference to bf_load_count, a 64-bit load counter
    GlobalVariable* store_var; // Global reference to bf_store_count, a 64-bit store counter
    GlobalVariable* load_inst_var;              // Global reference to bf_load_ins_count, a 64-bit load-instruction counter
    GlobalVariable* store_inst_var;             // Global reference to bf_store_ins_count, a 64-bit store-instruction counter
    GlobalVariable* mem_insts_var;              // Global reference to bf_mem_insts, a set of 64-bit memory instruction counters
    GlobalVariable* inst_mix_histo_var;         // Global reference to bf_inst_mix_histo, an array representing histogram of specific instruction counts.
    GlobalVariable* terminator_var;             // Global reference to bf_terminator_count, an array of terminator tallies
    GlobalVariable* mem_intrinsics_var;         // Global reference to bf_mem_intrin_count, tallies of memory intrinsics
    GlobalVariable* flop_var;  // Global reference to bf_flop_count, a 64-bit flop counter
    GlobalVariable* fp_bits_var;  // Global reference to bf_fp_bits_count, a 64-bit FP-bit counter
    GlobalVariable* op_var;    // Global reference to bf_op_count, a 64-bit operation counter
    GlobalVariable* op_bits_var;   // Global reference to bf_op_bits_count, a 64-bit operation-bit counter
    uint64_t static_loads;   // Number of static load instructions
    uint64_t static_stores;  // Number of static store instructions
    uint64_t static_flops;   // Number of static floating-point instructions
    uint64_t static_ops;     // Number of static instructions of any type (except no-ops)
    uint64_t static_cond_brs;  // Number of static conditional or indirect branch instructions
    uint64_t static_bblocks;   // Number of static basic blocks
    Function* init_if_necessary;  // Pointer to bf_initialize_if_necessary()
    Function* accum_bb_tallies;   // Pointer to bf_accumulate_bb_tallies()
    Function* report_bb_tallies;  // Pointer to bf_report_bb_tallies()
    Function* reset_bb_tallies;   // Pointer to bf_reset_bb_tallies()
    Function* assoc_counts_with_func;    // Pointer to bf_assoc_counters_with_func()
    Function* assoc_addrs_with_func;    // Pointer to bf_assoc_addresses_with_func()
    Function* assoc_addrs_with_prog;    // Pointer to bf_assoc_addresses_with_prog()
    Function* push_function;     // Pointer to bf_push_function()
    Function* pop_function;      // Pointer to bf_pop_function()
    Function* tally_function;    // Pointer to bf_incr_func_tally()
    Function* push_bb;           // Pointer to bf_push_basic_block()
    Function* pop_bb;            // Pointer to bf_pop_basic_block()
    Function* take_mega_lock;    // Pointer to bf_acquire_mega_lock()
    Function* release_mega_lock; // Pointer to bf_release_mega_lock()
    Function* tally_vector;      // Pointer to bf_tally_vector_operation()
    Function* reuse_dist_prog;   // Pointer to bf_reuse_dist_addrs_prog()
    Function* memset_intrinsic;  // Pointer to LLVM's memset() intrinsic
    StringMap<Constant*> func_name_to_arg;   // Map from a function name to an IR function argument
    set<string>* instrument_only;   // Set of functions to instrument; NULL=all
    set<string>* dont_instrument;   // Set of functions not to instrument; NULL=none
    ConstantInt* not_end_of_bb;     // 0, not at the end of a basic block
    ConstantInt* uncond_end_bb;     // 1, basic block ended with an unconditional branch
    ConstantInt* cond_end_bb;       // 2, basic block ended with a conditional branch
    ConstantInt* zero;        // A 64-bit constant "0"
    ConstantInt* one;         // A 64-bit constant "1"

    // Insert after a given instruction some code to increment a
    // global variable.
    void increment_global_variable(BasicBlock::iterator& iter,
                                   Constant* global_var,
                                   Value* increment);

    // Insert after a given instruction some code to increment an
    // element of a global array.
    void increment_global_array(BasicBlock::iterator& insert_before,
                                Constant* global_var,
                                Value* idx,
                                Value* increment);

    // Mark a variable as "used" (not eligible for dead-code elimination).
    void mark_as_used(Module& module, Constant* protected_var);

    // Create and initialize a global uint64_t constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           uint64_t value);

    // Create and initialize a global bool constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           bool value);

    // Create and initialize a global char* constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char* name,
                                           const char* value);

    // Return the number of elements in a given vector.
    ConstantInt* get_vector_length(LLVMContext& bbctx, const Type* dataType,
                                   ConstantInt* scalarValue);


    // Return true if and only if the given instruction should be
    // treated as a do-nothing operation.
    bool is_no_op(const Instruction& inst, const unsigned int opcode, const Type* instType);

    // Return true if and only if the given instruction should be
    // tallied as a floating-point operation.
    bool is_fp_operation(const Instruction& inst, const unsigned int opcode,
                         const Type* instType);

    // Return the total number of bits consumed and produced by a
    // given instruction.  The result is are a bit unintuitive for
    // certain types of instructions so use with caution.
    uint64_t instruction_operand_bits(const Instruction& inst);


    // Declare a function with external linkage and C calling conventions.
    Function* declare_extern_c(FunctionType *return_type,
			       StringRef func_name, Module *module);

    // Declare a function that takes no arguments and returns no value.
    Function* declare_thunk(Module* module, const char* thunk_name);

    // Map a function name (string) to an argument to an IR function call.
    Constant* map_func_name_to_arg (Module* module, StringRef funcname);

    // Declare an external variable.
    GlobalVariable* declare_global_var(Module& module, Type* var_type,
                                       StringRef var_name, bool is_const=false);

    // Insert code to set every element of a given array to zero.
    void insert_zero_array_code(Module* module,
                                GlobalVariable* array_to_zero,
                                uint64_t num_elts,
                                BasicBlock::iterator& insert_before);

    // Insert code at the end of a basic block.
    void insert_end_bb_code (Module* module, StringRef function_name,
                             int& must_clear, BasicBlock::iterator& insert_before);

    // Wrap CallInst::Create() with code to acquire and release the
    // mega-lock when instrumenting in thread-safe mode.
    void callinst_create(Value* function, ArrayRef<Value*> args,
                         Instruction* insert_before);

    // Ditto the above but for parameterless functions.
    void callinst_create(Value* function, Instruction* insert_before);

    // Ditto the above but with a different parameter list.
    void callinst_create(Value* function, BasicBlock* insert_before);

    // Ditto the above but for functions with arguments.
    void callinst_create(Value* function, ArrayRef<Value*> args,
                         BasicBlock* insert_before);

    // Given a Call instruction, return true if we can safely ignore it.
    bool ignorable_call (const Instruction* inst);

    // Instrument Load and Store instructions.
    void instrument_load_store(Module* module,
                               StringRef function_name,
                               BasicBlock::iterator& iter,
                               LLVMContext& bbctx,
                               DataLayout& target_data,
                               BasicBlock::iterator& terminator_inst,
                               int& must_clear);

    // Instrument Call instructions.
    void instrument_call(Module* module,
                         StringRef function_name,
                         Instruction* inst,
                         BasicBlock::iterator& insert_before,
                         int& must_clear);

    // Instrument all instructions.
    void instrument_all(Module* module,
                        StringRef function_name,
                        Instruction& iter,
                        LLVMContext& bbctx,
                        BasicBlock::iterator& insert_before,
                        int& must_clear);

    // Instrument miscellaneous instructions.
    void instrument_other(Module* module,
                          StringRef function_name,
                          Instruction& iter,
                          LLVMContext& bbctx,
                          BasicBlock::iterator& insert_before,
                          int& must_clear);

    // Do most of the instrumentation work: Walk each instruction in
    // each basic block and add instrumentation code around loads,
    // stores, flops, etc.
    void instrument_entire_function(Module* module, Function& function,
                                    StringRef function_name);

    // Instrument the current basic block iterator (representing a
    // load) for type-specific memory operations.
    void instrument_mem_type(Module* module,
                             bool is_store,
                             BasicBlock::iterator &iter,
                             Type *data_type);

    // Instrument the current basic block iterator (representing a
    // load) for type-specific characteristics.
    void instrument_load_types(BasicBlock::iterator &iter,
                               Type *data_type,
                               int &must_clear);

    // Instrument the current basic block iterator (representing a
    // store) for type-specific characteristics.
    void instrument_store_types(BasicBlock::iterator &iter,
                                Type *data_type,
                                int &must_clear);

    // Indicate that we need access to DataLayout.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<DataLayout>();
    }

  public:
    static char ID;
    BytesFlops() : FunctionPass(ID) { }

    // Initialize the BytesFlops pass.
    virtual bool doInitialization(Module& module);

    // Insert code for incrementing our byte, flop, etc. counters.
    virtual bool runOnFunction(Function& function);

    // Output what we instrumented.
    virtual void print(raw_ostream &outfile, const Module *module) const;
  };

}  // namespace bytesflops_pass

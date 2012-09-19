/*
 * Instrument code to keep track of the number of bytes accessed
 * and the number of floating-point operations performed.
 *
 * By Scott Pakin <pakin@lanl.gov>
      Pat McCormick <pat@lanl.gov>
 */

#include "llvm/ADT/StringMap.h"
#include "llvm/BasicBlock.h"
#include "llvm/Constants.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/GlobalValue.h"
#include "llvm/Instructions.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetData.h"
#include <vector>
#include <set>

using namespace std;
using namespace llvm;

namespace {

  // Define a command-line option for outputting results at the end of
  // every basic block instead of only once at the end of the program.
  static cl::opt<bool>
  InstrumentEveryBB("bf-every-bb", cl::init(false), cl::NotHidden,
                    cl::desc("Output byte and flop counts at the end of every basic block"));

  // Define a command-line option for aggregating measurements by
  // function name.
  static cl::opt<bool>
  TallyByFunction("bf-by-func", cl::init(false), cl::NotHidden,
                  cl::desc("Associate byte and flop counts with each function"));

  // Define a command-line option for outputting not only function
  // names but also immediate parents.
  static cl::opt<bool>
  TrackCallStack("bf-call-stack", cl::init(false), cl::NotHidden,
                 cl::desc("Additionally output the name of each function's parent"));

  // Define a command-line option for keeping track of unique bytes
  static cl::opt<bool>
  TrackUniqueBytes("bf-unique-bytes", cl::init(false), cl::NotHidden,
                   cl::desc("Additionally tally unique bytes accessed"));

  // Define a command-line option for tallying all binary operations,
  // not just floating-point operations.
  static cl::opt<bool>
  TallyAllOps("bf-all-ops", cl::init(false), cl::NotHidden,
              cl::desc("Tally all operations, not just floating-point operations"));

  // Define a command-line option for tallying load/store operations
  // based on various data types (note this also implies --bf-all-ops).
  static cl::opt<bool>
  TallyTypes("bf-types", cl::init(false), cl::NotHidden,
	     cl::desc("Tally type information with loads and stores (note this flag enables --bf-all-ops)."));

  // Define a command-line option for merging basic-block measurements
  // to reduce the output volume.
  static cl::opt<int>
  BBMergeCount("bf-merge-bb", cl::init(1), cl::NotHidden,
               cl::desc("Merge this many basic blocks into a single line of output"),
               cl::value_desc("count"));

  // Define a command-line option to accept a list of functions to
  // instrument, ignoring all others.
  static cl::list<string>
  IncludedFunctions("bf-include", cl::NotHidden, cl::ZeroOrMore, cl::CommaSeparated,
               cl::desc("Instrument only the functions in the given list"),
               cl::value_desc("function,..."));

  // Define a command-line option to accept a list of functions not to
  // instrument, including all others.
  static cl::list<string>
  ExcludedFunctions("bf-exclude", cl::NotHidden, cl::ZeroOrMore, cl::CommaSeparated,
               cl::desc("Do not instrument the functions in the given list"),
               cl::value_desc("function,..."));

  // Define a command-line option for enabling thread safety (at the
  // cost of increasing execution time).
  static cl::opt<bool>
  ThreadSafety("bf-thread-safe", cl::init(false), cl::NotHidden,
               cl::desc("Generate slower but thread-safe instrumentation"));

  // Define a command-line option for tallying vector operations.
  static cl::opt<bool>
  TallyVectors("bf-vectors", cl::init(false), cl::NotHidden,
               cl::desc("Tally vector lengths and counts"));

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

    GlobalVariable* load_var;  // Global reference to bf_load_count, a 64-bit load counter
    GlobalVariable* store_var; // Global reference to bf_store_count, a 64-bit store counter

    // TODO: We might want to collapse types into a single array-based set of counters to 
    // make the code a bit cleaner... 
    GlobalVariable* load_inst_var;              // Global reference to bf_load_ins_count, a 64-bit load-instruction counter
    GlobalVariable* load_float_inst_var;        // Global reference to bf_float_load_ins_count, a 64-bit load-instruction counter for single-precision floats
    GlobalVariable* load_double_inst_var;       // Global reference to bf_double_load_ins_count, a 64-bit load-instruction counter for double-precision floats
    GlobalVariable* load_int8_inst_var;         // Global reference to bf_int8_load_ins_count, a 64-bit load-instruction counter for 8-bit integers
    GlobalVariable* load_int16_inst_var;        // Global reference to bf_int16_load_ins_count, a 64-bit load-instruction counter for 16-bit integers
    GlobalVariable* load_int32_inst_var;        // Global reference to bf_int32_load_ins_count, a 64-bit load-instruction counter for 32-bit integers
    GlobalVariable* load_int64_inst_var;        // Global reference to bf_int64_load_ins_count, a 64-bit load-instruction counter for 64-bit integers
    GlobalVariable* load_ptr_inst_var;          // Global reference to bf_ptr_load_ins_count, a 64-bit load-instruction counter for pointers
    GlobalVariable* load_other_type_inst_var;   // Global reference to bf_other_type_load_ins_count, a 64-bit load-instruction counter for other types

    GlobalVariable* store_inst_var;             // Global reference to bf_store_ins_count, a 64-bit store-instruction counter
    GlobalVariable* store_float_inst_var;       // Global reference to bf_float_store_ins_count, a 64-bit store-instruction counter for single-precision floats
    GlobalVariable* store_double_inst_var;      // Global reference to bf_double_store_ins_count, a 64-bit store-instruction counter for double-precision floats
    GlobalVariable* store_int8_inst_var;        // Global reference to bf_int8_store_ins_count, a 64-bit store-instruction counter for 8-bit integers
    GlobalVariable* store_int16_inst_var;       // Global reference to bf_int16_store_ins_count, a 64-bit store-instruction counter for 16-bit integers
    GlobalVariable* store_int32_inst_var;       // Global reference to bf_int32_store_ins_count, a 64-bit store-instruction counter for 32-bit integers
    GlobalVariable* store_int64_inst_var;       // Global reference to bf_int64_store_ins_count, a 64-bit store-instruction counter for 64-bit integers
    GlobalVariable* store_ptr_inst_var;         // Global reference to bf_ptr_store_ins_count, a 64-bit store-instruction counter for pointers
    GlobalVariable* store_other_type_inst_var; // Global reference to bf_other_type_store_ins_count, a 64-bit store-instruction counter for other types     

    GlobalVariable* flop_var;  // Global reference to bf_flop_count, a 64-bit flop counter
    GlobalVariable* fp_bits_var;  // Global reference to bf_fp_bits_count, a 64-bit FP-bit counter
    GlobalVariable* op_var;    // Global reference to bf_op_count, a 64-bit operation counter
    GlobalVariable* op_bits_var;   // Global reference to bf_op_bits_count, a 64-bit operation-bit counter
    uint64_t static_loads;   // Number of static load instructions
    uint64_t static_stores;  // Number of static store instructions
    uint64_t static_flops;   // Number of static floating-point instructions
    uint64_t static_ops;     // Number of static binary-operation instructions
    uint64_t static_cond_brs;  // Number of static conditional or indirect branch instructions
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
                                   Value* increment) {

      // Point to the next instruction, because it's easy to insert before that.
      BasicBlock::iterator next_iter = iter;
      next_iter++;

      // %0 = load i64* @<global_var>, align 8
      LoadInst* load_var = new LoadInst(global_var, "gvar", false, next_iter);

        // %1 = add i64 %0, <increment>
      BinaryOperator* inc_var =
        BinaryOperator::Create(Instruction::Add, load_var, increment,
                               "new_gvar", next_iter);

      // store i64 %1, i64* @<global_var>, align 8
      new StoreInst(inc_var, global_var, false, next_iter);

      // Point to the last instruction we inserted.  The loop over
      // instructions will then increment this to point one past the
      // last instruction we inserted.
      iter = next_iter;
      iter--;
    }

    // Mark a variable as "used" (not eligible for dead-code elimination).
    void mark_as_used(Module& module, GlobalVariable* protected_var) {
      LLVMContext& globctx = module.getContext();
      PointerType* ptr8 = PointerType::get(IntegerType::get(globctx, 8), 0);
      ArrayType* ptr8_array = ArrayType::get(ptr8, 1);

      GlobalVariable* llvm_used =
        new GlobalVariable(module, ptr8_array, false,
                           GlobalValue::AppendingLinkage, 0, "llvm.used");
      llvm_used->setSection("llvm.metadata");
      std::vector<Constant*> llvm_used_elts;
      llvm_used_elts.push_back(ConstantExpr::getCast(Instruction::BitCast,
                                                     protected_var, ptr8));
      llvm_used->setInitializer(ConstantArray::get(ptr8_array, llvm_used_elts));
    }

    // Create and initialize a global uint64_t constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char *name, uint64_t value) {
      LLVMContext& globctx = module.getContext();
      IntegerType* i64type = Type::getInt64Ty(globctx);
      ConstantInt* const_value = ConstantInt::get(globctx, APInt(64, value));
      GlobalVariable* new_constant =
        new GlobalVariable(module, i64type, true, GlobalValue::LinkOnceODRLinkage,
                           const_value, name);
      mark_as_used(module, new_constant);
      return new_constant;
    }

    // Create and initialize a global bool constant in the
    // instrumented code.
    GlobalVariable* create_global_constant(Module& module, const char *name, bool value) {
      LLVMContext& globctx = module.getContext();
      IntegerType* booltype = Type::getInt1Ty(globctx);
      ConstantInt* const_value = ConstantInt::get(globctx, APInt(1, value));
      GlobalVariable* new_constant =
        new GlobalVariable(module, booltype, true, GlobalValue::LinkOnceODRLinkage,
                           const_value, name);
      mark_as_used(module, new_constant);
      return new_constant;
    }

    // Return the number of elements in a given vector.
    ConstantInt* get_vector_length(LLVMContext& bbctx, const Type* dataType, ConstantInt* scalarValue) {
      if (dataType->isVectorTy()) {
        unsigned int num_elts = dyn_cast<VectorType>(dataType)->getNumElements();
        return ConstantInt::get(bbctx, APInt(64, num_elts));
      }
      else
        return scalarValue;
    }

    // Return true if and only if the given instruction should be
    // tallied as an operation.
    bool is_any_operation(const Instruction& inst, const unsigned int opcode,
                          const Type* instType) {
      // Treat a variety of instructions as "operations".
      if (inst.isBinaryOp(opcode))
        // Binary operator (e.g., add)
        return true;
      if (isa<CastInst>(inst) && !isa<BitCastInst>(inst))
        // Cast (e.g., sitofp) but not a bit cast (e.g., bitcast)
        return true;
      if (isa<CmpInst>(inst))
        // Comparison (e.g., icmp)
        return true;

      // None of the above
      return false;
    }

    // Return true if and only if the given instruction should be
    // tallied as a floating-point operation.
    bool is_fp_operation(const Instruction& inst, const unsigned int opcode,
                         const Type* instType) {
      return inst.isBinaryOp(opcode) && instType->isFPOrFPVectorTy();
    }

    // Return the total number of bits consumed and produced by a
    // given instruction.  Results are are a bit unintuitive for
    // certain types of instructions so use with caution.
    uint64_t instruction_operand_bits(const Instruction& inst) {
      uint64_t total_bits = inst.getType()->getPrimitiveSizeInBits();
      for (User::const_op_iterator iter = inst.op_begin(); iter != inst.op_end(); iter++) {
          Value* val = dyn_cast<Value>(*iter);
          total_bits += val->getType()->getPrimitiveSizeInBits();
        }
      return total_bits;
    }

    // Declare a function that takes no arguments and returns no value.
    Function* declare_thunk(Module* module, const char* thunk_name) {
        vector<Type*> no_args;
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(module->getContext()), no_args, false);
        Function* thunk_function =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           thunk_name, module);
        thunk_function->setCallingConv(CallingConv::C);
        return thunk_function;
    }

    // Map a function name (string) to an argument to an IR function call.
    Constant* map_func_name_to_arg (Module* module, StringRef funcname) {
      // If we already mapped this function name we don't need to do
      // so again.
      Constant* string_argument = func_name_to_arg[funcname];
      if (string_argument != NULL)
        return string_argument;

      // This is the first time we've seen this function name.
      LLVMContext& globctx = module->getContext();
      ArrayType* char_array =
        ArrayType::get(IntegerType::get(globctx, 8), funcname.size()+1);
      GlobalVariable* const_char_ptr =
        new GlobalVariable(*module, char_array, true,
                           GlobalValue::PrivateLinkage,
                           ConstantDataArray::getString(globctx, funcname, true),
                           ".fname");
      vector<Constant*> getelementptr_indices;
      ConstantInt* zero_index = ConstantInt::get(globctx, APInt(64, 0));
      getelementptr_indices.push_back(zero_index);
      getelementptr_indices.push_back(zero_index);
      string_argument =
        ConstantExpr::getGetElementPtr(const_char_ptr, getelementptr_indices);
      func_name_to_arg[funcname] = string_argument;
      return string_argument;
    }

    // Declare an external thread-local variable.
    GlobalVariable* declare_TLS_global(Module& module, Type* var_type, StringRef var_name) {
      return new GlobalVariable(module, var_type, false, GlobalVariable::ExternalLinkage, 0, var_name, 0, GlobalVariable::GeneralDynamicTLSModel);
    }

    // Insert code at the end of a basic block.
    void insert_end_bb_code (Module* module, StringRef function_name,
                             int& must_clear, BasicBlock::iterator& insert_before) {
      // Determine if we're really at the end of a basic block or if
      // we're simply at a call instruction.
      Instruction& inst = *insert_before;
      bool is_end_of_bb = insert_before->isTerminator();
      ConstantInt* end_of_bb_type;
      if (is_end_of_bb) {
        unsigned int opcode = inst.getOpcode();   // Terminator instruction's opcode
        switch (opcode) {
          case Instruction::IndirectBr:
          case Instruction::Switch:
            end_of_bb_type = cond_end_bb;
            break;

          case Instruction::Br:
            end_of_bb_type = dyn_cast<BranchInst>(&inst)->isConditional() ? cond_end_bb : uncond_end_bb;
            break;

          default:
            end_of_bb_type = uncond_end_bb;
            break;
        }
      }
      else
        end_of_bb_type = not_end_of_bb;
      if (end_of_bb_type == cond_end_bb)
        static_cond_brs++;

      // If requested by the user, insert a call to
      // bf_accumulate_bb_tallies() and, at the true end of the basic
      // block, bf_report_bb_tallies().
      if (InstrumentEveryBB) {
        callinst_create(accum_bb_tallies, end_of_bb_type, insert_before);
        if (is_end_of_bb)
          callinst_create(report_bb_tallies, insert_before);
      }

      // If requested by the user, insert a call to
      // bf_assoc_counters_with_func() at the end of the basic block.
      if (TallyByFunction) {
        vector<Value*> arg_list;
        arg_list.push_back(map_func_name_to_arg(module, function_name));
        arg_list.push_back(end_of_bb_type);
        callinst_create(assoc_counts_with_func, arg_list, insert_before);
      }

      // Reset all of our counter variables.  Technically, even when
      // instrumenting in thread-safe mode we don't need to protect
      // these with a lock because all counter variables are
      // thread-local.  However, we do so anyway to enable
      // reduce_mega_lock_activity() to merge the preceding call to
      // bf_assoc_counters_with_func(), the counter-resetting code,
      // and the following call to either bf_reset_bb_tallies() or
      // bf_pop_function() into a single critical section.
      if (InstrumentEveryBB || TallyByFunction) {
        if (ThreadSafety)
          CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);

        if (must_clear & CLEAR_LOADS) {
          new StoreInst(zero, load_var, false, insert_before);
	  if (TallyAllOps) {
	    new StoreInst(zero, load_inst_var, false, insert_before);

	    if (TallyTypes) {
	      if (must_clear & CLEAR_FLOAT_LOADS) 
		new StoreInst(zero, load_float_inst_var, false, insert_before);

	      if (must_clear & CLEAR_DOUBLE_LOADS) 
		new StoreInst(zero, load_double_inst_var, false, insert_before);

	      // We currently treat all int-based loads as being in
	      // the same category -- this means we will likely get
	      // extra code here when only one type was counted.
	      // This was a trade off between more code here or some 
	      // overhead for function calls.  No clue on how best to 
	      // strike a blance here...
	      if (must_clear & CLEAR_INT_LOADS) { 
		new StoreInst(zero, load_int8_inst_var, false,  insert_before);
		new StoreInst(zero, load_int16_inst_var, false, insert_before);
		new StoreInst(zero, load_int32_inst_var, false, insert_before);
		new StoreInst(zero, load_int64_inst_var, false, insert_before);
	      }

	      if (must_clear & CLEAR_PTR_LOADS) 
		new StoreInst(zero, load_ptr_inst_var, false,  insert_before);
	      
	      if (must_clear & CLEAR_OTHER_TYPE_LOADS) 
		new StoreInst(zero, load_other_type_inst_var, false,  insert_before);
	    }
	  }
	}

        if (must_clear & CLEAR_STORES) {
          new StoreInst(zero, store_var, false, insert_before);
	  if (TallyAllOps) {
	    new StoreInst(zero, store_inst_var, false, insert_before);
	    
	    if (TallyTypes) {

	      if (must_clear & CLEAR_FLOAT_STORES)
                new StoreInst(zero, store_float_inst_var, false, insert_before);

              if (must_clear & CLEAR_DOUBLE_STORES)
                new StoreInst(zero, store_double_inst_var, false, insert_before);

	      // We currently treat all int-based loads as being in
	      // the same category -- this means we will likely get
	      // extra code here when only one type was counted.
	      // This was a trade off between more code here or some 
	      // overhead for function calls.  No clue on how best to 
	      // strike a blance here...
              if (must_clear & CLEAR_INT_STORES) {
                new StoreInst(zero, store_int8_inst_var, false,  insert_before);
                new StoreInst(zero, store_int16_inst_var, false, insert_before);
                new StoreInst(zero, store_int32_inst_var, false, insert_before);
                new StoreInst(zero, store_int64_inst_var, false, insert_before);
              }

              if (must_clear & CLEAR_PTR_STORES)
                new StoreInst(zero, store_ptr_inst_var, false,  insert_before);
	      
              if (must_clear & CLEAR_OTHER_TYPE_STORES)
                new StoreInst(zero, store_other_type_inst_var, false,  insert_before);
	    }
	  }
	}

        if (must_clear & CLEAR_FLOPS)
          new StoreInst(zero, flop_var, false, insert_before);
        if (must_clear & CLEAR_FP_BITS)
          new StoreInst(zero, fp_bits_var, false, insert_before);
        if (must_clear & CLEAR_OPS)
          new StoreInst(zero, op_var, false, insert_before);
        if (must_clear & CLEAR_OP_BITS)
          new StoreInst(zero, op_bits_var, false, insert_before);
        must_clear = 0;
        if (ThreadSafety)
          CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
        if (is_end_of_bb && InstrumentEveryBB)
          callinst_create(reset_bb_tallies, insert_before);
      }

      // If requested by the user, insert a call to bf_pop_function()
      // at every return from the function.
      if (TrackCallStack && insert_before->getOpcode() == Instruction::Ret)
        callinst_create(pop_function, insert_before);
    }

    // Wrap CallInst::Create() with code to acquire and release the
    // mega-lock when instrumenting in thread-safe mode.
    void callinst_create(Value* function, ArrayRef<Value*> args,
                         Instruction* insert_before) {
      if (ThreadSafety)
        CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
      CallInst::Create(function, args, "", insert_before)->setCallingConv(CallingConv::C);
      if (ThreadSafety)
        CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    }

    // Ditto the above but for parameterless functions.
    void callinst_create(Value* function, Instruction* insert_before) {
      if (ThreadSafety)
        CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
      CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
      if (ThreadSafety)
        CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    }

    // Ditto the above but with a different parameter list.
    void callinst_create(Value* function, BasicBlock* insert_before) {
      if (ThreadSafety)
        CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
      CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
      if (ThreadSafety)
        CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    }

    // Ditto the above but for functions with arguments.
    void callinst_create(Value* function, ArrayRef<Value*> args,
                         BasicBlock* insert_before) {
      if (ThreadSafety)
        CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
      CallInst::Create(function, args, "", insert_before)->setCallingConv(CallingConv::C);
      if (ThreadSafety)
        CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    }

    // Do most of the instrumentation work: Walk each instruction in
    // each basic block and add instrumentation code around loads,
    // stores, flops, etc.
    void instrument_entire_function(Module* module, Function& function, StringRef function_name) {
      // Iterate over each basic block in turn.
      for (Function::iterator func_iter = function.begin();
           func_iter != function.end();
           func_iter++) {
        // Perform per-basic-block variable initialization.
        BasicBlock& bb = *func_iter;
        LLVMContext& bbctx = bb.getContext();
        TargetData& target_data = getAnalysis<TargetData>();
        BasicBlock::iterator terminator_inst = bb.end();
        terminator_inst--;
        int must_clear = 0;   // Keep track of which counters we need to clear.

        // Iterate over the basic block's instructions one-by-one.
        for (BasicBlock::iterator iter = bb.begin();
             iter != terminator_inst;
             iter++) {
          Instruction& inst = *iter;                // Current instruction
          unsigned int opcode = inst.getOpcode();   // Current instruction's opcode

          if (opcode == Instruction::Load || opcode == Instruction::Store) {
            // Increment the byte counter for load and store
            // instructions (any datatype).
            Value* mem_value = opcode == Instruction::Load ? &inst : cast<StoreInst>(inst).getValueOperand();
            uint64_t byte_count = target_data.getTypeStoreSize(mem_value->getType());
            ConstantInt* num_bytes =
              ConstantInt::get(bbctx, APInt(64, byte_count));
            if (opcode == Instruction::Load) {
              increment_global_variable(iter, load_var, num_bytes);
	      if (TallyAllOps)
		increment_global_variable(iter, load_inst_var, one);
              must_clear |= CLEAR_LOADS;
              static_loads++;

	      if (TallyTypes) {
		Type *data_type = mem_value->getType();
		if (data_type->isSingleValueType()) {
		  // We only instrument types that are register
		  // (single value) friendly...
		  instrument_load_types(iter, data_type, must_clear);
		}
	      }
            }
            else
              if (opcode == Instruction::Store) {
                increment_global_variable(iter, store_var, num_bytes);
		if (TallyAllOps)
		  increment_global_variable(iter, store_inst_var, one);
                must_clear |= CLEAR_STORES;
                static_stores++;

		if (TallyTypes) {
		  Type *data_type = mem_value->getType();
		  if (data_type->isSingleValueType()) {
		    // We only instrument stores that are register
		    // (single value) friendly...
		    instrument_store_types(iter, data_type, must_clear);
		  }
		}
              }

            // If requested by the user, also insert a call to
            // bf_assoc_addresses_with_prog() and perhaps
            // bf_assoc_addresses_with_func().
            if (TrackUniqueBytes) {
              // Determine the starting address that was loaded or stored.
              BasicBlock::iterator next_iter = iter;
              next_iter++;
              Value* mem_ptr =
                opcode == Instruction::Load
                ? cast<LoadInst>(inst).getPointerOperand()
                : cast<StoreInst>(inst).getPointerOperand();
              CastInst* mem_addr = new PtrToIntInst(mem_ptr,
                                                    IntegerType::get(bbctx, 64),
                                                    "", next_iter);

              // Conditionally insert a call to bf_assoc_addresses_with_func().
              if (TallyByFunction) {
                vector<Value*> arg_list;
                arg_list.push_back(map_func_name_to_arg(module, function_name));
                arg_list.push_back(mem_addr);
                arg_list.push_back(num_bytes);
                callinst_create(assoc_addrs_with_func, arg_list, terminator_inst);
              }

              // Unconditionally insert a call to
              // bf_assoc_addresses_with_prog().
              vector<Value*> arg_list;
              arg_list.push_back(mem_addr);
              arg_list.push_back(num_bytes);
              callinst_create(assoc_addrs_with_prog, arg_list, terminator_inst);
            }
          }
          else
            // The instruction isn't a load or a store.  See if it's a
            // function call.
            if (opcode == Instruction::Call) {
              // Ignore LLVM pseudo-functions and functions that *we* inserted.
              Function* func = dyn_cast<CallInst>(&inst)->getCalledFunction();
              if (func) {
                StringRef callee_name = func->getName();
                if (!callee_name.startswith("_ZN10bytesflops")
                    && !callee_name.startswith("llvm.dbg")) {
                  // Tally the caller (with a distinguishing "+" in
                  // front of its name) in order to keep track of
                  // calls to uninstrumented functions.
		  if (TallyByFunction) {
		    string augmented_callee_name(string("+") + callee_name.str());
		    Constant* argument = map_func_name_to_arg(module, StringRef(augmented_callee_name));
		    callinst_create(tally_function, argument, iter);
		  }

                  // Push our counter state before the call and pop it
                  // afterwards.
                  insert_end_bb_code(module, function_name, must_clear, iter);
                  callinst_create(push_bb, iter);
                  BasicBlock::iterator next_iter = iter;
                  next_iter++;
                  callinst_create(pop_bb, next_iter);
                }
              }
            }
            else {
              // The instruction isn't a load, a store, or a function
              // call.  See if it's an operation that we need to
              // watch.
              const Type* instType = inst.getType();     // Type of this instruction
              if (is_any_operation(inst, opcode, instType)) {
                ConstantInt* num_elts;    // Number of operations that this instruction performs
                ConstantInt* num_bits;    // Number of bits that this instruction produces
                bool tally_fp = is_fp_operation(inst, opcode, instType);

                if (TallyAllOps || tally_fp) {
                  // Initialize variables needed by at least one of the FP
                  // counter and the all-operation counter.
                  num_elts = get_vector_length(bbctx, instType, one);
                  num_bits = ConstantInt::get(bbctx, APInt(64, instruction_operand_bits(inst)));
                }

                // If the user requested a count of *all* operations, not
                // just floating-point operations, increment the operation
                // counter and the operation bit counter.
                if (TallyAllOps) {
                  increment_global_variable(iter, op_var, num_elts);
                  must_clear |= CLEAR_OPS;
                  increment_global_variable(iter, op_bits_var, num_bits);
                  must_clear |= CLEAR_OP_BITS;
                  static_ops++;
                }

                // Increment the flop counter and floating-point bit
                // counter for any binary instruction with a
                // floating-point type.
                if (tally_fp) {
                  increment_global_variable(iter, flop_var, num_elts);
                  must_clear |= CLEAR_FLOPS;
                  increment_global_variable(iter, fp_bits_var, num_bits);
                  must_clear |= CLEAR_FP_BITS;
                  static_flops++;
                }

                // If the user requested a characterization of vector
                // operations, see if we have a vector operation and
                // if so, bin it.
                if (TallyVectors)
                  do {
                    // Determine if this is a vector operation and one
                    // that we're interested in.
                    const VectorType *vt = dyn_cast<VectorType>(instType);
                    if (vt == NULL)
                      // This isn't a vector operation.
                      break;
                    if (opcode == Instruction::ExtractElement
                        || opcode == Instruction::InsertElement
                        || opcode == Instruction::ExtractValue
                        || opcode == Instruction::InsertValue)
                      // Ignore mixed scalar/vector operations.
                      break;

                    // Tally this vector operation.
                    vector<Value*> arg_list;
                    uint64_t elt_count = vt->getNumElements();
                    uint64_t total_bits = instType->getPrimitiveSizeInBits();
                    arg_list.push_back(map_func_name_to_arg(module, function_name));
                    arg_list.push_back(get_vector_length(bbctx, vt, one));
                    arg_list.push_back(ConstantInt::get(bbctx, APInt(64, total_bits/elt_count)));
                    arg_list.push_back(ConstantInt::get(bbctx, APInt(8, 1)));
                    callinst_create(tally_vector, arg_list, terminator_inst);
                  }
                  while (0);
              }
              else
                if (TallyAllOps && isa<GetElementPtrInst>(inst)) {
                  // LLVM's getelementptr instruction requires special
                  // handling.  Given the C declaration "int *a", the
                  // getelementptr representation of a[3] is likely to
                  // turn into a+12 (a single addition), while the
                  // getelementptr representation of a[i] is likely to
                  // turn into a+4*i (an addition plus a multiplication).
                  // We therefore count variable arguments as two ops and
                  // constants as one op.
                  uint64_t arg_ops = 0;      // Expected number of operations
                  uint64_t arg_op_bits = 0;  // Expected number of bits used
                  User::const_op_iterator arg_iter = inst.op_begin();
                  for (arg_iter++; arg_iter != inst.op_end(); arg_iter++) {
                    Value* arg = dyn_cast<Value>(*arg_iter);
                    switch (arg->getValueID()) {
                      // All of the following constant cases were copied
                      // and pasted from LLVM's Value.h.
                      case Value::ConstantExprVal:
                      case Value::ConstantAggregateZeroVal:
                      case Value::ConstantIntVal:
                      case Value::ConstantFPVal:
                      case Value::ConstantArrayVal:
                      case Value::ConstantStructVal:
                      case Value::ConstantVectorVal:
                      case Value::ConstantPointerNullVal:
                        arg_ops++;
                        arg_op_bits += sizeof(int)*3;  // a = b + c
                        break;

                      // Non-constant cases count as a multiply and an add.
                      default:
                        arg_ops += 2;
                        arg_op_bits += sizeof(int)*6;  // a = b * c; d = a + f
                        break;
                    }
                  }
                  increment_global_variable(iter, op_var,
                                            ConstantInt::get(bbctx, APInt(64, arg_ops)));
                  must_clear |= CLEAR_OPS;
                  increment_global_variable(iter, op_bits_var,
                                            ConstantInt::get(bbctx, APInt(64, arg_op_bits)));
                  must_clear |= CLEAR_OP_BITS;
                  static_ops++;
                }
            }
        }

        // Insert various bits of code at the end of the basic block.
        insert_end_bb_code(module, function_name, must_clear, terminator_inst);
      }  // Ends the loop over basic blocks within the function

      // Insert a call to bf_initialize_if_necessary() at the
      // beginning of the function.  Also insert a call to
      // bf_push_function() if -bf-call-stack was specified or to
      // bf_incr_func_tally() if -bf-by-func was specified without
      // -bf-call-stack.
      LLVMContext& func_ctx = function.getContext();
      BasicBlock& old_entry = function.front();
      BasicBlock* new_entry =
        BasicBlock::Create(func_ctx, "bf_entry", &function, &old_entry);
      callinst_create(init_if_necessary, new_entry);
      if (TallyByFunction) {
        Function* entry_func = TrackCallStack ? push_function : tally_function;
        Constant* argument = map_func_name_to_arg(module, function_name);
        callinst_create(entry_func, argument, new_entry);
      }
      BranchInst::Create(&old_entry, new_entry);
    }

    // Instrument the current basic block iterator (representing a
    // load) for type-specific characteristics.
    void instrument_load_types(BasicBlock::iterator &iter, 
                               Type *data_type, 
                               int &must_clear) {

      if (data_type->isFloatTy()) {
        increment_global_variable(iter, load_float_inst_var, one);
        must_clear |= CLEAR_FLOAT_LOADS;
      } else if (data_type->isDoubleTy()) {
        increment_global_variable(iter, load_double_inst_var, one);
        must_clear |= CLEAR_DOUBLE_LOADS;
      } else if (data_type->isIntegerTy(8)) {
        increment_global_variable(iter, load_int8_inst_var, one);
        must_clear |= CLEAR_INT_LOADS;
      } else if (data_type->isIntegerTy(16)) {
        increment_global_variable(iter, load_int16_inst_var, one);
        must_clear |= CLEAR_INT_LOADS;
      } else if (data_type->isIntegerTy(32)) {
        increment_global_variable(iter, load_int32_inst_var, one);
        must_clear |= CLEAR_INT_LOADS;
      } else if (data_type->isIntegerTy(64)) {
        increment_global_variable(iter, load_int64_inst_var, one);
        must_clear |= CLEAR_INT_LOADS;
      } else if (data_type->isPointerTy()) {
        increment_global_variable(iter, load_ptr_inst_var, one);
        must_clear |= CLEAR_PTR_LOADS;
      } else {
        increment_global_variable(iter, load_other_type_inst_var, one);
        must_clear |= CLEAR_OTHER_TYPE_LOADS;            
      }
    }

    
    // Instrument the current basic block iterator (representing a
    // store) for type-specific characteristics.
    void instrument_store_types(BasicBlock::iterator &iter, 
                                Type *data_type,
                                int &must_clear) {
      
      if (data_type->isFloatTy()) {
        increment_global_variable(iter, store_float_inst_var, one);
        must_clear |= CLEAR_FLOAT_STORES;
      } else if (data_type->isDoubleTy()) {
        increment_global_variable(iter, store_double_inst_var, one);
        must_clear |= CLEAR_DOUBLE_STORES;
      } else if (data_type->isIntegerTy(8)) {
        increment_global_variable(iter, store_int8_inst_var, one);
        must_clear |= CLEAR_INT_STORES;
      } else if (data_type->isIntegerTy(16)) {
        increment_global_variable(iter, store_int16_inst_var, one);
        must_clear |= CLEAR_INT_STORES;
      } else if (data_type->isIntegerTy(32)) {
        increment_global_variable(iter, store_int32_inst_var, one);
        must_clear |= CLEAR_INT_STORES;
      } else if (data_type->isIntegerTy(64)) {
        increment_global_variable(iter, store_int64_inst_var, one);
        must_clear |= CLEAR_INT_STORES;
      } else if (data_type->isPointerTy()) {
        increment_global_variable(iter, store_ptr_inst_var, one);
        must_clear |= CLEAR_PTR_STORES;
      } else {
        increment_global_variable(iter, store_other_type_inst_var, one);
        must_clear |= CLEAR_OTHER_TYPE_STORES;                    
      }
    }


    // Optimize the instrumented code by deleting back-to-back
    // mega-lock releases and acquisitions.
    void reduce_mega_lock_activity(Function& function) {
      // Store a few constant strings.
      StringRef take_mega_lock_name = take_mega_lock->getName();
      StringRef release_mega_lock_name = release_mega_lock->getName();

      // Iterate over each basic block in turn.
      for (Function::iterator func_iter = function.begin();
           func_iter != function.end();
           func_iter++) {
        // Perform per-basic-block variable initialization.
        BasicBlock& bb = *func_iter;
        BasicBlock::iterator terminator_inst = bb.end();
        terminator_inst--;

        // Iterate over the basic block's instructions one-by-one,
        // accumulating a list of function calls to delete.
        vector<Instruction*> deletable_insts;   // List of function calls to delete
        Instruction* prev_inst = NULL;  // Immediately preceding function call or NULL if the previous instruction wasn't a function call
        for (BasicBlock::iterator iter = bb.begin();
             iter != terminator_inst;
             iter++) {
          // Find a pair of back-to-back functions.
          Instruction& inst = *iter;                // Current instruction
          if (inst.getOpcode() != Instruction::Call) {
            // We care only about function calls.
            prev_inst = NULL;
            continue;
          }
          Function* func = dyn_cast<CallInst>(&inst)->getCalledFunction();
          if (!prev_inst) {
            // We care only about back-to-back functions.
            prev_inst = &inst;
            continue;
          }

          // Delete release-acquire pairs.  (None of the other three
          // combinations of release and acquire are likely to occur
          // in practice.)
          Function* prev_func = dyn_cast<CallInst>(prev_inst)->getCalledFunction();
          if (prev_func
              && func
              && prev_func->getName() == release_mega_lock_name
              && func->getName() == take_mega_lock_name) {
            deletable_insts.push_back(prev_inst);
            deletable_insts.push_back(&inst);
            prev_inst = NULL;
          }
          else
            prev_inst = &inst;
        }

        // Delete all function calls we marked as deletable.
        while (!deletable_insts.empty()) {
          deletable_insts.back()->eraseFromParent();
          deletable_insts.pop_back();
        }
      }
    }

    // Indicate that we need access to TargetData.
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.addRequired<TargetData>();
    }

  public:
    static char ID;
    BytesFlops() : FunctionPass(ID) {
      static_loads = 0;
      static_stores = 0;
      static_flops = 0;
      static_ops = 0;
      static_cond_brs = 0;
    }

    virtual bool doInitialization(Module& module) {
      // Inject external declarations to various variables defined in byfl.c.
      LLVMContext& globctx = module.getContext();
      IntegerType* i64type = Type::getInt64Ty(globctx);

      load_var                  = declare_TLS_global(module, i64type, "bf_load_count");
      store_var                 = declare_TLS_global(module, i64type, "bf_store_count");

      load_inst_var             = declare_TLS_global(module, i64type, "bf_load_ins_count");
      load_float_inst_var       = declare_TLS_global(module, i64type, "bf_load_float_ins_count");
      load_double_inst_var      = declare_TLS_global(module, i64type, "bf_load_double_ins_count");
      load_int8_inst_var        = declare_TLS_global(module, i64type, "bf_load_int8_ins_count");
      load_int16_inst_var       = declare_TLS_global(module, i64type, "bf_load_int16_ins_count");
      load_int32_inst_var       = declare_TLS_global(module, i64type, "bf_load_int32_ins_count");
      load_int64_inst_var       = declare_TLS_global(module, i64type, "bf_load_int64_ins_count");
      load_ptr_inst_var         = declare_TLS_global(module, i64type, "bf_load_ptr_ins_count");
      load_other_type_inst_var  = declare_TLS_global(module, i64type, "bf_load_other_type_ins_count");

      store_inst_var            = declare_TLS_global(module, i64type, "bf_store_ins_count");
      store_float_inst_var      = declare_TLS_global(module, i64type, "bf_store_float_ins_count");
      store_double_inst_var     = declare_TLS_global(module, i64type, "bf_store_double_ins_count");
      store_int8_inst_var       = declare_TLS_global(module, i64type, "bf_store_int8_ins_count");
      store_int16_inst_var      = declare_TLS_global(module, i64type, "bf_store_int16_ins_count");
      store_int32_inst_var      = declare_TLS_global(module, i64type, "bf_store_int32_ins_count");
      store_int64_inst_var      = declare_TLS_global(module, i64type, "bf_store_int64_ins_count");
      store_ptr_inst_var        = declare_TLS_global(module, i64type, "bf_store_ptr_ins_count");
      store_other_type_inst_var = declare_TLS_global(module, i64type, "bf_store_other_type_ins_count");

      flop_var                  = declare_TLS_global(module, i64type, "bf_flop_count");
      fp_bits_var               = declare_TLS_global(module, i64type, "bf_fp_bits_count");
      op_var                    = declare_TLS_global(module, i64type, "bf_op_count");
      op_bits_var               = declare_TLS_global(module, i64type, "bf_op_bits_count");

      // Assign a few constant values.
      not_end_of_bb = ConstantInt::get(globctx, APInt(32, 0));
      uncond_end_bb = ConstantInt::get(globctx, APInt(32, 1));
      cond_end_bb = ConstantInt::get(globctx, APInt(32, 2));
      zero = ConstantInt::get(globctx, APInt(64, 0));
      one = ConstantInt::get(globctx, APInt(64, 1));

      // Construct a set of functions to instrument.
      vector<string> funclist = IncludedFunctions;
      if (funclist.size() == 0)
        instrument_only = NULL;
      else {
        instrument_only = new(set<string>);
        for (vector<string>::iterator fniter = funclist.begin();
             fniter != funclist.end();
             fniter++)
          instrument_only->insert(*fniter);
      }

      // Construct a set of functions not to instrument.
      funclist = ExcludedFunctions;
      if (funclist.size() == 0)
        dont_instrument = NULL;
      else {
        dont_instrument = new(set<string>);
        for (vector<string>::iterator fniter = funclist.begin();
             fniter != funclist.end();
             fniter++)
          dont_instrument->insert(*fniter);
      }

      // Complain if we were given lists to instrument and not to instrument.
      if (instrument_only && dont_instrument)
        report_fatal_error("-bf-include and -bf-exclude are mutually exclusive");

      // Assign a value to bf_bb_merge.
      int merge_count = BBMergeCount;
      if (merge_count <= 0)
        report_fatal_error("-bf-merge requires a positive integer argument");
      create_global_constant(module, "bf_bb_merge", uint64_t(merge_count));

      // Tallying of types requires us to enable TallAllOps...
      if (TallyTypes && !TallyAllOps) {
	TallyAllOps = true;
      }

      // Assign a value to bf_all_ops.
      create_global_constant(module, "bf_all_ops", bool(TallyAllOps));

      // Assign a value to bf_types.
      create_global_constant(module, "bf_types", bool(TallyTypes));

      // Assign a value to bf_per_func.
      create_global_constant(module, "bf_per_func", bool(TallyByFunction));

      // Assign a value to bf_call_stack.
      if (TrackCallStack && !TallyByFunction)
        report_fatal_error("-bf-call-stack is allowed only in conjuction with -bf-by-func");
      create_global_constant(module, "bf_call_stack", bool(TrackCallStack));

      // Assign a value to bf_unique_bytes.
      create_global_constant(module, "bf_unique_bytes", bool(TrackUniqueBytes));

      // Assign a value to bf_vectors.
      create_global_constant(module, "bf_vectors", bool(TallyVectors));

      // Inject external declarations for
      // bf_initialize_if_necessary(), bf_push_basic_block(), and
      // bf_pop_basic_block().
      init_if_necessary = declare_thunk(&module, "_ZN10bytesflops26bf_initialize_if_necessaryEv");
      push_bb = declare_thunk(&module, "_ZN10bytesflops19bf_push_basic_blockEv");
      pop_bb = declare_thunk(&module, "_ZN10bytesflops18bf_pop_basic_blockEv");

      // Inject external declarations for bf_accumulate_bb_tallies(),
      // bf_reset_bb_tallies(), and bf_report_bb_tallies().
      if (InstrumentEveryBB) {
        vector<Type*> int_arg;
        int_arg.push_back(IntegerType::get(globctx, 32));
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(module.getContext()), int_arg, false);
        accum_bb_tallies =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops24bf_accumulate_bb_talliesE8bb_end_t",
                           &module);
        accum_bb_tallies->setCallingConv(CallingConv::C);
        reset_bb_tallies = declare_thunk(&module, "_ZN10bytesflops19bf_reset_bb_talliesEv");
        report_bb_tallies = declare_thunk(&module, "_ZN10bytesflops20bf_report_bb_talliesEv");
      }

      // Inject an external declaration for bf_assoc_counters_with_func().
      if (TallyByFunction) {
        vector<Type*> string_and_enum_arg;
        string_and_enum_arg.push_back(PointerType::get(IntegerType::get(globctx, 8), 0));
        string_and_enum_arg.push_back(IntegerType::get(globctx, 32));
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(globctx), string_and_enum_arg, false);
        assoc_counts_with_func =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops27bf_assoc_counters_with_funcEPKc8bb_end_t",
                           &module);
        assoc_counts_with_func->setCallingConv(CallingConv::C);
      }

      // Inject an external declarations for bf_increment_func_tally().
      if (TallyByFunction) {
        vector<Type*> single_string_arg;
        single_string_arg.push_back(PointerType::get(IntegerType::get(globctx, 8), 0));
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(globctx), single_string_arg, false);
        tally_function =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops18bf_incr_func_tallyEPKc", &module);
        tally_function->setCallingConv(CallingConv::C);
      }

      // Inject external declarations for bf_push_function() and
      // bf_pop_function().
      if (TallyByFunction && TrackCallStack) {
        // bf_push_function()
        vector<Type*> single_string_arg;
        single_string_arg.push_back(PointerType::get(IntegerType::get(globctx, 8), 0));
        FunctionType* void_str_func_result =
          FunctionType::get(Type::getVoidTy(globctx), single_string_arg, false);
        push_function =
          Function::Create(void_str_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops16bf_push_functionEPKc", &module);
        push_function->setCallingConv(CallingConv::C);

        // bf_pop_function()
        pop_function = declare_thunk(&module, "_ZN10bytesflops15bf_pop_functionEv");
      }

      // Declare bf_tally_vector_operation() only if we were asked
      // to track vector operations.
      if (TallyVectors) {
        vector<Type*> all_function_args;
        all_function_args.push_back(PointerType::get(IntegerType::get(globctx, 8), 0));
        all_function_args.push_back(IntegerType::get(globctx, 64));
        all_function_args.push_back(IntegerType::get(globctx, 64));
        all_function_args.push_back(IntegerType::get(globctx, 8));
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
        tally_vector =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops25bf_tally_vector_operationEPKcmmb",
                           &module);
        tally_vector->setCallingConv(CallingConv::C);
      }

      // Inject external declarations for bf_assoc_addresses_with_prog()
      // and bf_assoc_addresses_with_func().
      if (TrackUniqueBytes) {
        // Declare bf_assoc_addresses_with_prog() any time we need to
        // track unique bytes.
        vector<Type*> all_function_args;
        all_function_args.push_back(IntegerType::get(globctx, 64));
        all_function_args.push_back(IntegerType::get(globctx, 64));
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
        assoc_addrs_with_prog =
          Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                           "_ZN10bytesflops28bf_assoc_addresses_with_progEmm",
                           &module);
        assoc_addrs_with_prog->setCallingConv(CallingConv::C);

        // Declare bf_assoc_addresses_with_func() only if we were
        // asked to track unique addresses by function.
        if (TallyByFunction) {
          vector<Type*> all_function_args;
          all_function_args.push_back(PointerType::get(IntegerType::get(globctx, 8), 0));
          all_function_args.push_back(IntegerType::get(globctx, 64));
          all_function_args.push_back(IntegerType::get(globctx, 64));
          FunctionType* void_func_result =
            FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
          assoc_addrs_with_func =
            Function::Create(void_func_result, GlobalValue::ExternalLinkage,
                             "_ZN10bytesflops28bf_assoc_addresses_with_funcEPKcmm",
                             &module);
          assoc_addrs_with_func->setCallingConv(CallingConv::C);
        }
      }

      // Inject external declarations for bf_acquire_mega_lock() and
      // bf_release_mega_lock().
      if (ThreadSafety) {
        take_mega_lock = declare_thunk(&module, "_ZN10bytesflops20bf_acquire_mega_lockEv");
        release_mega_lock = declare_thunk(&module, "_ZN10bytesflops20bf_release_mega_lockEv");
      }

      // Depend on our helper library and on LLVMSupport (for
      // StringMap).  If we're trying to preserve thread safety, also
      // depend on the pthread library.
      module.addLibrary("byfl");
      module.addLibrary("LLVMSupport");
      if (ThreadSafety)
        module.addLibrary("pthread");
      return true;
    }

    // Insert code for incrementing our byte, flop, etc. counters.
    virtual bool runOnFunction(Function& function) {
      // Do nothing if we're supposed to ignore this function.
      StringRef function_name = function.getName();
      if (instrument_only != NULL
          && instrument_only->find(function_name) == instrument_only->end())
        return false;
      if (dont_instrument != NULL
          && dont_instrument->find(function_name) != dont_instrument->end())
        return false;

      // Instrument "interesting" instructions in every basic block.
      Module* module = function.getParent();
      instrument_entire_function(module, function, function_name);

      // Clean up our mess.
      if (ThreadSafety)
        reduce_mega_lock_activity(function);

      // Return, indicating that we modified this function.
      return true;
    }

    // Output what we instrumented.
    virtual void print(raw_ostream &outfile, const Module *module) const {
      outfile << module->getModuleIdentifier() << ": "
              << static_loads << " loads, "
              << static_stores << " stores, "
              << static_flops << " flins";
      if (TallyAllOps)
        outfile << ", " << static_ops << " binops";
      if (InstrumentEveryBB)
        outfile << ", " << static_cond_brs << " cond_brs";
      outfile << '\n';
    }
  };

  const int BytesFlops::CLEAR_LOADS             =  1;
  const int BytesFlops::CLEAR_STORES            =  2;
  const int BytesFlops::CLEAR_FLOPS             =  4;
  const int BytesFlops::CLEAR_FP_BITS           =  8;
  const int BytesFlops::CLEAR_OPS               = 16;
  const int BytesFlops::CLEAR_OP_BITS           = 32;

  const int BytesFlops::CLEAR_FLOAT_LOADS       = 64;
  const int BytesFlops::CLEAR_DOUBLE_LOADS      = 128;
  const int BytesFlops::CLEAR_INT_LOADS         = 256;
  const int BytesFlops::CLEAR_PTR_LOADS         = 512;
  const int BytesFlops::CLEAR_OTHER_TYPE_LOADS  = 1024;

  const int BytesFlops::CLEAR_FLOAT_STORES      = 2048;
  const int BytesFlops::CLEAR_DOUBLE_STORES     = 4096;
  const int BytesFlops::CLEAR_INT_STORES        = 8192;
  const int BytesFlops::CLEAR_PTR_STORES        = 16384;
  const int BytesFlops::CLEAR_OTHER_TYPE_STORES = 32768;

  char BytesFlops::ID = 0;
  static RegisterPass<BytesFlops> H("bytesflops", "Bytes:flops instrumentation");
}

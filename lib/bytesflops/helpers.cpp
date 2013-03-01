/*
 * Instrument code to keep track of run-time behavior:
 * helper methods
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "bytesflops.h"

namespace bytesflops_pass {

  // Tally the number of instances of a given character in a string.
  static size_t tally_all_instances(string& some_string, char some_char) {
    size_t tally = 0;
    for (string::const_iterator iter = some_string.cbegin(); iter != some_string.cend(); iter++)
      if (*iter == some_char)
	tally++;
    return tally;
  }

  // Read a list of function names, one per line, from a file into a
  // set.  C++ function names can be either mangled or unmangled.
  static void functions_from_file(string filename, set<string>* funcset) {
    ifstream infile(filename.c_str(), ifstream::in);
    if (!infile.good())
      report_fatal_error(StringRef("Failed to open file ") + filename);
    string oneline;
    while (infile.good() && getline(infile, oneline)) {
      // Normalize unmangled names by removing spaces then insert the
      // result into the set.
      remove_all_instances(oneline, ' ');
      funcset->insert(oneline);
    }
    infile.close();
  }

  // Destructively remove all instances of a given character from a string.
  void remove_all_instances(string& some_string, char some_char) {
    some_string.erase(remove(some_string.begin(), some_string.end(), some_char),
		      some_string.end());
  }

  // Parse a list of function names into a set.  The trick is that (1)
  // demangled C++ function names are split (at commas) across list
  // elements and need to be recombined, and (2) the form "@filename"
  // causes function names to be read from a file.
  set<string>* parse_function_names(vector<string>& funclist) {
    if (funclist.size() == 0)
      return NULL;
    string funcname;
    size_t lparens = 0;
    size_t rparens = 0;
    set<string>* resulting_set = new(set<string>);
    for (vector<string>::iterator fniter = funclist.begin();
	 fniter != funclist.end();
	 fniter++) {
      // Combine pieces of mangled names.
      string partial_name(*fniter);
      if (lparens > 0 || rparens > 0)
	funcname += ',';
      funcname += partial_name;
      lparens += tally_all_instances(partial_name, '(');
      rparens += tally_all_instances(partial_name, ')');
      if (lparens != rparens)
	continue;
	  
      // We have a complete function name.  Add it to the set.
      if (funcname[0] == '@')
	// Read function names from a file into the set.
	functions_from_file(funcname.substr(1), resulting_set);
      else {
	// Function name was specified literally.  Normalize
	// unmangled names by removing spaces then insert the result
	// into the set.
	remove_all_instances(funcname, ' ');
	resulting_set->insert(funcname);
      }
      funcname = "";
      lparens = 0;
      rparens = 0;
    }
    return resulting_set;
  }

  // Insert after a given instruction some code to increment a global
  // variable.
  void BytesFlops::increment_global_variable(BasicBlock::iterator& iter,
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

  // Insert after a given instruction some code to increment an
  // element of a global array.
  void BytesFlops::increment_global_array(BasicBlock::iterator& iter,
					  Constant* global_var,
					  Value* idx,
					  Value* increment,
                                          bool ins_after) {
    // Point to the next instruction, because it's easy to insert before that.
    BasicBlock::iterator next_iter = iter;
    if (ins_after)
      next_iter++;

    // %1 = load i64** @myarray, align 8
    LoadInst* load_array = new LoadInst(global_var, "bfmic", false, next_iter);
    load_array->setAlignment(8);

    // %2 = getelementptr inbounds i64* %1, i64 %idx
    GetElementPtrInst* idx_ptr = GetElementPtrInst::Create(load_array, idx, "idx_ptr", next_iter);

    // %3 = load i64* %2, align 8
    LoadInst* idx_val = new LoadInst(idx_ptr, "idx_val", false, next_iter);
    idx_val->setAlignment(8);

    // %4 = add i64 %3, %increment
    BinaryOperator* inc_elt =
      BinaryOperator::Create(Instruction::Add, idx_val, increment, "new_val", next_iter);

    // store i64 %4, i64* %2, align 8
    StoreInst* store_inst = new StoreInst(inc_elt, idx_ptr, false, next_iter);
    store_inst->setAlignment(8);

    // Point to the last instruction we inserted.  The loop over
    // instructions will then increment this to point one past the
    // last instruction we inserted.
    iter = next_iter;
    iter--;
  }

  // Mark a variable as "used" (not eligible for dead-code elimination).
  void BytesFlops::mark_as_used(Module& module, GlobalVariable* protected_var) {
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
  GlobalVariable* BytesFlops::create_global_constant(Module& module,
						     const char *name,
						     uint64_t value) {
    LLVMContext& globctx = module.getContext();
    IntegerType* i64type = Type::getInt64Ty(globctx);
    ConstantInt* const_value = ConstantInt::get(globctx, APInt(64, value));
    GlobalVariable* new_constant =
      new GlobalVariable(module, i64type, true, GlobalValue::LinkOnceODRLinkage,
			 const_value, name);
    mark_as_used(module, new_constant);
    return new_constant;
  }

  // Create and initialize a global bool constant in the instrumented
  // code.
  GlobalVariable* BytesFlops::create_global_constant(Module& module,
						     const char *name,
						     bool value) {
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
  ConstantInt* BytesFlops::get_vector_length(LLVMContext& bbctx, const Type* dataType, ConstantInt* scalarValue) {
    if (dataType->isVectorTy()) {
      unsigned int num_elts = dyn_cast<VectorType>(dataType)->getNumElements();
      return ConstantInt::get(bbctx, APInt(64, num_elts));
    }
    else
      return scalarValue;
  }

  // Return true if and only if the given instruction should be
  // tallied as an operation.
  bool BytesFlops::is_any_operation(const Instruction& inst,
				    const unsigned int opcode,
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
  bool BytesFlops::is_fp_operation(const Instruction& inst,
				   const unsigned int opcode,
				   const Type* instType) {
    return inst.isBinaryOp(opcode) && instType->isFPOrFPVectorTy();
  }

  // Return the total number of bits consumed and produced by a
  // given instruction.  The result is are a bit unintuitive for
  // certain types of instructions so use with caution.
  uint64_t BytesFlops::instruction_operand_bits(const Instruction& inst) {
    uint64_t total_bits = inst.getType()->getPrimitiveSizeInBits();
    for (User::const_op_iterator iter = inst.op_begin(); iter != inst.op_end(); iter++) {
	Value* val = dyn_cast<Value>(*iter);
	total_bits += val->getType()->getPrimitiveSizeInBits();
      }
    return total_bits;
  }

  // Declare a function that takes no arguments and returns no value.
  Function* BytesFlops::declare_thunk(Module* module, const char* thunk_name) {
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
  Constant* BytesFlops::map_func_name_to_arg (Module* module, StringRef funcname) {
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
  GlobalVariable* BytesFlops::declare_TLS_global(Module& module, Type* var_type, StringRef var_name) {
    return new GlobalVariable(module, var_type, false,
			      GlobalVariable::ExternalLinkage, 0, var_name, 0,
			      GlobalVariable::GeneralDynamicTLSModel);
  }

  // Insert code at the end of a basic block.
  void BytesFlops::insert_end_bb_code (Module* module, StringRef function_name,
				       int& must_clear,
				       BasicBlock::iterator& insert_before) {
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
	if (TallyAllOps)
	  new StoreInst(zero, load_inst_var, false, insert_before);
      }
      if (must_clear & CLEAR_STORES) {
	new StoreInst(zero, store_var, false, insert_before);
	if (TallyAllOps)
	  new StoreInst(zero, store_inst_var, false, insert_before);
      }
      if (must_clear & CLEAR_FLOPS)
	new StoreInst(zero, flop_var, false, insert_before);
      if (must_clear & CLEAR_FP_BITS)
	new StoreInst(zero, fp_bits_var, false, insert_before);
      if (must_clear & CLEAR_OPS)
	new StoreInst(zero, op_var, false, insert_before);
      if (must_clear & CLEAR_OP_BITS)
	new StoreInst(zero, op_bits_var, false, insert_before);
      if (must_clear & CLEAR_MEM_TYPES) {
	// Zero out the entire array.
	LoadInst* mem_insts_addr = new LoadInst(mem_insts_var, "mi", false, insert_before);
	mem_insts_addr->setAlignment(8);
	LLVMContext& globctx = module->getContext();
	CastInst* mem_insts_cast =
	  new BitCastInst(mem_insts_addr,
			  PointerType::get(IntegerType::get(globctx, 8), 0),
			  "miv", insert_before);
	static ConstantInt* zero_8bit =
	  ConstantInt::get(globctx, APInt(8, 0));
	static ConstantInt* mem_insts_size =
	  ConstantInt::get(globctx, APInt(64, NUM_MEM_INSTS*sizeof(uint64_t)));
	static ConstantInt* mem_insts_align =
	  ConstantInt::get(globctx, APInt(32, sizeof(uint64_t)));
	static ConstantInt* zero_1bit =
	  ConstantInt::get(globctx, APInt(1, 0));
	std::vector<Value*> func_args;
	func_args.push_back(mem_insts_cast);
	func_args.push_back(zero_8bit);
	func_args.push_back(mem_insts_size);
	func_args.push_back(mem_insts_align);
	func_args.push_back(zero_1bit);
	callinst_create(memset_intrinsic, func_args, insert_before);
      }      
      if (TallyInstMix) {
       	// If we're tallying instructions we don't need a must_clear
       	// bit to tell us that an instruction was executed.  We always
       	// need to zero out the entire array.
	LoadInst* tally_insts_addr = new LoadInst(inst_mix_histo_var, "ti", false, insert_before);
	tally_insts_addr->setAlignment(8);
	LLVMContext& globctx = module->getContext();
	CastInst* tally_insts_cast =
	  new BitCastInst(tally_insts_addr,
			  PointerType::get(IntegerType::get(globctx, 8), 0),
			  "miv", insert_before);
	static ConstantInt* zero_8bit =
	  ConstantInt::get(globctx, APInt(8, 0));
        static uint64_t totalInstCount = uint64_t(Instruction::OtherOpsEnd);
	static ConstantInt* tally_insts_size =
	  ConstantInt::get(globctx, APInt(64, totalInstCount*sizeof(uint64_t)));
	static ConstantInt* tally_insts_align =
	  ConstantInt::get(globctx, APInt(32, sizeof(uint64_t)));
	static ConstantInt* zero_1bit =
	  ConstantInt::get(globctx, APInt(1, 0));
	std::vector<Value*> func_args;
	func_args.push_back(tally_insts_cast);
	func_args.push_back(zero_8bit);
	func_args.push_back(tally_insts_size);
	func_args.push_back(tally_insts_align);
	func_args.push_back(zero_1bit);
	callinst_create(memset_intrinsic, func_args, insert_before);
      }
      
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
  void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
				   Instruction* insert_before) {
    if (ThreadSafety)
      CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    CallInst::Create(function, args, "", insert_before)->setCallingConv(CallingConv::C);
    if (ThreadSafety)
      CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but for parameterless functions.
  void BytesFlops::callinst_create(Value* function, Instruction* insert_before) {
    if (ThreadSafety)
      CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
    if (ThreadSafety)
      CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but with a different parameter list.
  void BytesFlops::callinst_create(Value* function, BasicBlock* insert_before) {
    if (ThreadSafety)
      CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
    if (ThreadSafety)
      CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but for functions with arguments.
  void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
				   BasicBlock* insert_before) {
    if (ThreadSafety)
      CallInst::Create(take_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
    CallInst::Create(function, args, "", insert_before)->setCallingConv(CallingConv::C);
    if (ThreadSafety)
      CallInst::Create(release_mega_lock, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Optimize the instrumented code by deleting back-to-back
  // mega-lock releases and acquisitions.
  void BytesFlops::reduce_mega_lock_activity(Function& function) {
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
} // namespace bytesflops_pass

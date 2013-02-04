/*
 * Instrument code to keep track of run-time behavior:
 * helper methods
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "bytesflops.h"

namespace bytesflops_pass {

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
    return new GlobalVariable(module, var_type, false, GlobalVariable::ExternalLinkage, 0, var_name, 0, GlobalVariable::GeneralDynamicTLSModel);
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

} // namespace bytesflops_pass

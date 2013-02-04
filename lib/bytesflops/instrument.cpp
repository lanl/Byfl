/*
 * Instrument code to keep track of run-time behavior:
 * intrumentation functions proper (modifying LLVM IR)
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "bytesflops.h"

namespace bytesflops_pass {

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

  // Instrument Load and Store instructions.
  void BytesFlops::instrument_load_store(Module* module,
					 StringRef function_name,
					 BasicBlock::iterator& iter,
					 LLVMContext& bbctx,
					 DataLayout& target_data,
					 BasicBlock::iterator& terminator_inst,
					 int& must_clear) {
    // Increment the byte counter for load and store
    // instructions (any datatype).
    Instruction& inst = *iter;                // Current instruction
    unsigned int opcode = inst.getOpcode();   // Current instruction's opcode
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

    // Determine the memory address that was loaded or stored.
    CastInst* mem_addr = NULL;
    if (TrackUniqueBytes || rd_bits > 0) {
      BasicBlock::iterator next_iter = iter;
      next_iter++;
      Value* mem_ptr =
	opcode == Instruction::Load
	? cast<LoadInst>(inst).getPointerOperand()
	: cast<StoreInst>(inst).getPointerOperand();
      mem_addr = new PtrToIntInst(mem_ptr, IntegerType::get(bbctx, 64),
				  "", next_iter);
    }

    // If requested by the user, also insert a call to
    // bf_assoc_addresses_with_prog() and perhaps
    // bf_assoc_addresses_with_func().
    if (TrackUniqueBytes) {
      // Conditionally insert a call to bf_assoc_addresses_with_func().
      if (TallyByFunction) {
	vector<Value*> arg_list;
	arg_list.push_back(map_func_name_to_arg(module, function_name));
	arg_list.push_back(mem_addr);
	arg_list.push_back(num_bytes);
	callinst_create(assoc_addrs_with_func, arg_list, terminator_inst);
      }

      // Unconditionally insert a call to bf_assoc_addresses_with_prog().
      vector<Value*> arg_list;
      arg_list.push_back(mem_addr);
      arg_list.push_back(num_bytes);
      callinst_create(assoc_addrs_with_prog, arg_list, terminator_inst);
    }

    // If requested by the user, also insert a call to
    // bf_reuse_dist_addrs_prog().
    if ((opcode == Instruction::Load && (rd_bits&(1<<RD_LOADS)) != 0)
	|| (opcode == Instruction::Store && (rd_bits&(1<<RD_STORES)) != 0)) {
      vector<Value*> arg_list;
      arg_list.push_back(mem_addr);
      arg_list.push_back(num_bytes);
      callinst_create(reuse_dist_prog, arg_list, terminator_inst);
    }
  }

  // Instrument Call instructions.
  void BytesFlops::instrument_call(Module* module,
				   StringRef function_name,
				   BasicBlock::iterator& iter,
				   int& must_clear) {
    // Ignore LLVM pseudo-functions and functions that *we* inserted.
    Instruction& inst = *iter;                // Current instruction
    Function* func = dyn_cast<CallInst>(&inst)->getCalledFunction();
    if (func) {
      StringRef callee_name = func->getName();
      if (!callee_name.startswith("_ZN10bytesflops")
	  && !callee_name.startswith("llvm.dbg")
	  && callee_name != "bf_categorize_counters") {
	// Tally the caller (with a distinguishing "+" in
	// front of its name) in order to keep track of
	// calls to uninstrumented functions.
	if (TallyByFunction) {
	  string augmented_callee_name;

	  // Attempt to demangle function names so the masses can follow along...
	  int status;
	  char *demangled_name = __cxxabiv1::__cxa_demangle(callee_name.str().c_str(), NULL, 0, &status);
	  if (status == 0 && demangled_name != 0) {
	    augmented_callee_name = string("+") + string(demangled_name);
	    free(demangled_name);
	  } else {
	    augmented_callee_name = string("+") + callee_name.str();
	  }
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

  // Instrument miscellaneous instructions.
  void BytesFlops::instrument_other(Module* module,
				    StringRef function_name,
				    BasicBlock::iterator& iter,
				    LLVMContext& bbctx,
				    BasicBlock::iterator& terminator_inst,
				    int& must_clear) {
    Instruction& inst = *iter;                // Current instruction
    const Type* instType = inst.getType();    // Type of this instruction
    unsigned int opcode = inst.getOpcode();   // Current instruction's opcode
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

      if (tally_fp) {
	// Increment the flop counter and floating-point bit
	// counter for any binary instruction with a
	// floating-point type.
	increment_global_variable(iter, flop_var, num_elts);
	must_clear |= CLEAR_FLOPS;
	increment_global_variable(iter, fp_bits_var, num_bits);
	must_clear |= CLEAR_FP_BITS;
	static_flops++;
      } else {
	// If the user requested a count of *all* operations, not
	// just floating-point operations, increment the operation
	// counter and the operation bit counter for non-floating point
	// operations.
	if (TallyAllOps) {
	  increment_global_variable(iter, op_var, num_elts);
	  must_clear |= CLEAR_OPS;
	  increment_global_variable(iter, op_bits_var, num_bits);
	  must_clear |= CLEAR_OP_BITS;
	  static_ops++;
	}
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

  // Do most of the instrumentation work: Walk each instruction in
  // each basic block and add instrumentation code around loads,
  // stores, flops, etc.
  void BytesFlops::instrument_entire_function(Module* module,
					      Function& function,
					      StringRef function_name) {
    // Iterate over each basic block in turn.
    for (Function::iterator func_iter = function.begin();
	 func_iter != function.end();
	 func_iter++) {
      // Perform per-basic-block variable initialization.
      BasicBlock& bb = *func_iter;
      LLVMContext& bbctx = bb.getContext();
      DataLayout& target_data = getAnalysis<DataLayout>();
      BasicBlock::iterator terminator_inst = bb.end();
      terminator_inst--;
      int must_clear = 0;   // Keep track of which counters we need to clear.

      // Iterate over the basic block's instructions one-by-one.
      for (BasicBlock::iterator iter = bb.begin();
	   iter != terminator_inst;
	   iter++) {
	Instruction& inst = *iter;                // Current instruction
	unsigned int opcode = inst.getOpcode();   // Current instruction's opcode

	if (opcode == Instruction::Load || opcode == Instruction::Store)
	  instrument_load_store(module, function_name, iter, bbctx,
				target_data, terminator_inst, must_clear);
	else
	  // The instruction isn't a load or a store.  See if it's a
	  // function call.
	  if (opcode == Instruction::Call)
	    instrument_call(module, function_name, iter, must_clear);
	  else
	    // The instruction isn't a load, a store, or a function
	    // call.  See if it's an operation that we need to
	    // watch.
	    instrument_other(module, function_name, iter, bbctx,
			     terminator_inst, must_clear);
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
  void BytesFlops::instrument_load_types(BasicBlock::iterator &iter,
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
  void BytesFlops::instrument_store_types(BasicBlock::iterator &iter,
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
}

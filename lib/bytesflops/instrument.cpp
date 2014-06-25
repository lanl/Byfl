/*
 * Instrument code to keep track of run-time behavior:
 * intrumentation functions proper (modifying LLVM IR)
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */
#include <iostream>
#include "bytesflops.h"
#include "BytesMP.h"

namespace bytesflops_pass {

  const int BytesFlops::CLEAR_LOADS          =   1;
  const int BytesFlops::CLEAR_STORES         =   2;
  const int BytesFlops::CLEAR_FLOPS          =   4;
  const int BytesFlops::CLEAR_FP_BITS        =   8;
  const int BytesFlops::CLEAR_OPS            =  16;
  const int BytesFlops::CLEAR_OP_BITS        =  32;
  const int BytesFlops::CLEAR_MEM_TYPES      =  64;

  /**
   * Create a constructor for the module in which we make a call to
   * record the map of function names to their unique keys.  The constructor
   * acts like declaring a file function like:
   *   __attribute__((constructor))
   *   void file_foo_init() {...}
   * This helps ensure that all of the function names are recorded
   * exactly once.
   */
  void BytesFlops::create_func_map_ctor(Module & module, uint32_t nkeys,
          Constant * keys, Constant * fnames)
  {
    initializeKeyMap(module);

    LLVMContext& ctx = module.getContext();
      
    // Function Definitions
    // create a basic block in the ctor.
    BasicBlock* ctor_bb = BasicBlock::Create(ctx, "", func_map_ctor, 0);
    if ( !ctor_bb )
    {
        printf("Error: unable to create basic block\n");
    }
    // insert call to record the keys
    std::vector<Value *> args;

    ConstantInt*
    const_nkeys = ConstantInt::get(ctx, APInt(32, nkeys));

    /**
     * args are:
     * 1) the number of keys
     * 2) the function keys
     * 3) the function names
     * that is, the key of function fnames[i] is keys[i].
     */
    args.push_back(const_nkeys);
    args.push_back(keys);
    args.push_back(fnames);
    CallInst* void_12 = CallInst::Create(record_funcs2keys, args, "", ctor_bb);
    void_12->setCallingConv(CallingConv::C);
    void_12->setTailCall(false);
    AttributeSet void_12_PAL;
    void_12->setAttributes(void_12_PAL);
      
    ReturnInst::Create(ctx, ctor_bb);
  }

  // Instrument the current basic block iterator (representing a
  // load) for type-specific memory operations.
  void BytesFlops::instrument_mem_type(Module* module,
                                       bool is_store,
                                       BasicBlock::iterator &iter,
                                       Type *data_type) {
    const Type* current_type = data_type;   // "Pointer of...", "vector of...", etc.

    // Load or store
    uint64_t memop = is_store ? BF_OP_STORE : BF_OP_LOAD;

    // Pointer or value
    uint64_t memref;
    if (current_type->isPointerTy()) {
      memref = BF_REF_POINTER;
      current_type = current_type->getPointerElementType();
    }
    else
      memref = BF_REF_VALUE;

    // Vector or scalar
    uint64_t memagg;
    if (current_type->isVectorTy()) {
      memagg = BF_AGG_VECTOR;
      current_type = current_type->getVectorElementType();
    }
    else
      memagg = BF_AGG_SCALAR;

    // Integer, floating-point, or other (e.g., pointer, array, or struct)
    uint64_t memtype;
    if (current_type->isIntegerTy())
      memtype = BF_TYPE_INT;
    else if (current_type->isFloatingPointTy())
      memtype = BF_TYPE_FP;
    else
      memtype = BF_TYPE_OTHER;

    // Width of the operation in bits
    uint64_t memwidth;
    switch (current_type->getPrimitiveSizeInBits()) {
      case 8:
        memwidth = BF_WIDTH_8;
        break;

      case 16:
        memwidth = BF_WIDTH_16;
        break;

      case 32:
        memwidth = BF_WIDTH_32;
        break;

      case 64:
        memwidth = BF_WIDTH_64;
        break;

      case 128:
        memwidth = BF_WIDTH_128;
        break;

      default:
        memwidth = BF_WIDTH_OTHER;
        break;
    }

    // Compute an index into the bf_mem_insts_count array.
    uint64_t idx = mem_type_to_index(memop, memref, memagg, memtype, memwidth);

    // Increment the counter indexed by idx.
    LLVMContext& globctx = module->getContext();
    ConstantInt* idxVal = ConstantInt::get(globctx, APInt(64, idx));
    increment_global_array(iter, mem_insts_var, idxVal, one);
  }

  // Instrument Load and Store instructions.
  void BytesFlops::instrument_load_store(Module* module,
                                         StringRef function_name,
                                         BasicBlock::iterator& iter,
                                         LLVMContext& bbctx,
                                         DataLayout& target_data,
                                         BasicBlock::iterator& insert_before,
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
      increment_global_variable(insert_before, load_var, num_bytes);
      increment_global_variable(insert_before, load_inst_var, one);
      if (TallyTypes) {
        Type *data_type = mem_value->getType();
        instrument_mem_type(module, false, insert_before, data_type);
        must_clear |= CLEAR_MEM_TYPES;
      }
      must_clear |= CLEAR_LOADS;
      static_loads++;
    }
    else
      if (opcode == Instruction::Store) {
        increment_global_variable(insert_before, store_var, num_bytes);
        increment_global_variable(insert_before, store_inst_var, one);
        if (TallyTypes) {
          Type *data_type = mem_value->getType();
          instrument_mem_type(module, true, insert_before, data_type);
          must_clear |= CLEAR_MEM_TYPES;
        }
        must_clear |= CLEAR_STORES;
        static_stores++;
      }

    // Determine the memory address that was loaded or stored.
    CastInst* mem_addr = NULL;
    if (TrackUniqueBytes || rd_bits > 0) {
      Value* mem_ptr =
        opcode == Instruction::Load
        ? cast<LoadInst>(inst).getPointerOperand()
        : cast<StoreInst>(inst).getPointerOperand();
      mem_addr = new PtrToIntInst(mem_ptr, IntegerType::get(bbctx, 64),
                                  "", insert_before);
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
        callinst_create(assoc_addrs_with_func, arg_list, insert_before);
      }

      // Unconditionally insert a call to bf_assoc_addresses_with_prog().
      vector<Value*> arg_list;
      arg_list.push_back(mem_addr);
      arg_list.push_back(num_bytes);
      callinst_create(assoc_addrs_with_prog, arg_list, insert_before);
    }

    // If requested by the user, also insert a call to
    // bf_reuse_dist_addrs_prog().
    if ((opcode == Instruction::Load && (rd_bits&(1<<RD_LOADS)) != 0)
        || (opcode == Instruction::Store && (rd_bits&(1<<RD_STORES)) != 0)) {
      vector<Value*> arg_list;
      arg_list.push_back(mem_addr);
      arg_list.push_back(num_bytes);
      callinst_create(reuse_dist_prog, arg_list, insert_before);
    }
  }

  FunctionKeyGen::KeyID BytesFlops::record_func(const string & fname)
  {
      /**
       * if we haven't recorded this function yet, then
       * generate a unique key to associate with the function and
       * record the (key, fname) pair.
       * For now, since we cannot preserve state (that is, the next available
       * key) across modules, we use large random integers as keys.
       * The RNG should have low chance of duplicates.
       */
      FunctionKeyGen::KeyID keyval;

      auto cit = func_key_map.find(fname);
      if ( cit == func_key_map.end() )
      {
          keyval = m_keygen->nextRandomKey();
          func_key_map[fname] = keyval;
      }
      else
      {
          keyval = cit->second;
      }

      return keyval;
  }


  // Instrument Call instructions.  Note that we've already skipped
  // over calls to llvm.dbg.*.
  void BytesFlops::instrument_call(Module* module,
                                   StringRef function_name,
                                   Instruction* inst,
                                   BasicBlock::iterator& insert_before,
                                   int& must_clear) {
    Function* func = dyn_cast<CallInst>(inst)->getCalledFunction();
    if (!func)
      return;
    StringRef callee_name = func->getName();

    // Tally calls to the LLVM memory intrinsics (llvm.mem{set,cpy,move}.*).
    if (isa<MemIntrinsic>(inst)) {
      LLVMContext& globctx = module->getContext();
      if (MemSetInst* memsetfunc = dyn_cast<MemSetInst>(inst)) {
        // Handle llvm.memset.* by incrementing the memset tally and
        // byte count.
        ConstantInt* callVal = ConstantInt::get(globctx, APInt(64, BF_MEMSET_CALLS));
        increment_global_array(insert_before, mem_intrinsics_var, callVal, one);
        ConstantInt* byteVal = ConstantInt::get(globctx, APInt(64, BF_MEMSET_BYTES));
        increment_global_array(insert_before, mem_intrinsics_var, byteVal, memsetfunc->getLength());
      }
      else if (MemTransferInst* memxferfunc = dyn_cast<MemTransferInst>(inst)) {
        // Handle llvm.memcpy.* and llvm.memmove.* by incrementing the
        // memxfer tally and byte count.
        ConstantInt* callVal = ConstantInt::get(globctx, APInt(64, BF_MEMXFER_CALLS));
        increment_global_array(insert_before, mem_intrinsics_var, callVal, one);
        ConstantInt* byteVal = ConstantInt::get(globctx, APInt(64, BF_MEMXFER_BYTES));
        increment_global_array(insert_before, mem_intrinsics_var, byteVal, memxferfunc->getLength());
      }
    }

    // Tally the callee (with a distinguishing "+" in front of its
    // name) in order to keep track of calls to uninstrumented
    // functions.
    if (TallyByFunction) {

      // generate key if needed
      FunctionKeyGen::KeyID keyval;
      string augmented_callee_name(string("+") + callee_name.str());
      keyval = record_func(augmented_callee_name);

      ConstantInt * key = ConstantInt::get(IntegerType::get(module->getContext(),
                                       8*sizeof(FunctionKeyGen::KeyID)),
                                       keyval);

      callinst_create(tally_function, key, insert_before);
    } // end if (TallyByFunction)
  }

  // Instrument all instructions except no-ops.
  void BytesFlops::instrument_all(Module* module,
                                  StringRef function_name,
                                  Instruction& inst,
                                  LLVMContext& bbctx,
                                  BasicBlock::iterator& insert_before,
                                  int& must_clear) {
    // Ignore operations that are likely to be discarded during code generation.
    const Type* instType = inst.getType();    // Type of this instruction
    unsigned int opcode = inst.getOpcode();   // Current instruction's opcode
    if (is_no_op(inst, opcode, instType))
      return;

    // Process all other instructions.
    if (isa<GetElementPtrInst>(inst)) {
      // LLVM's getelementptr instruction requires special handling.
      // Given the C declaration "int *a", the getelementptr
      // representation of a[3] is likely to turn into a+12 (a single
      // addition), while the getelementptr representation of a[i] is
      // likely to turn into a+4*i (an addition plus a
      // multiplication).  We therefore count variable arguments as
      // two ops and constants as one op.
      uint64_t arg_ops = 0;      // Expected number of operations
      uint64_t arg_op_bits = 0;  // Expected number of bits used
      User::const_op_iterator arg_iter = inst.op_begin();
      for (arg_iter++; arg_iter != inst.op_end(); arg_iter++) {
        Value* arg = dyn_cast<Value>(*arg_iter);
        unsigned int this_arg_bits = arg->getType()->getPrimitiveSizeInBits();
        switch (arg->getValueID()) {
          // All of the following constant cases were copied
          // and pasted from LLVM's Value.h.
          case Value::ConstantExprVal:
          case Value::ConstantAggregateZeroVal:
          case Value::ConstantDataArrayVal:
          case Value::ConstantDataVectorVal:
          case Value::ConstantIntVal:
          case Value::ConstantFPVal:
          case Value::ConstantArrayVal:
          case Value::ConstantStructVal:
          case Value::ConstantVectorVal:
          case Value::ConstantPointerNullVal:
            arg_ops++;
            arg_op_bits += this_arg_bits*3;  // a = b + c
            break;

            // Non-constant cases count as a multiply and an add.
          default:
            arg_ops += 2;
            arg_op_bits += this_arg_bits*6;  // a = b * c; d = a + f
            break;
        }
      }
      increment_global_variable(insert_before, op_var,
                                ConstantInt::get(bbctx, APInt(64, arg_ops)));
      must_clear |= CLEAR_OPS;
      increment_global_variable(insert_before, op_bits_var,
                                ConstantInt::get(bbctx, APInt(64, arg_op_bits)));
      must_clear |= CLEAR_OP_BITS;
      static_ops += arg_ops;
    }
    else {
      // We're not a getelementptr instruction.  Determine the number
      // of elements and number of bits on which this instruction
      // operates.
      ConstantInt* num_elts;    // Number of operations that this instruction performs
      ConstantInt* num_bits;    // Number of bits that this instruction produces
      num_elts = get_vector_length(bbctx, instType, one);
      num_bits = ConstantInt::get(bbctx, APInt(64, instruction_operand_bits(inst)));

      // Increment the operation counter and the operation bit counter.
      increment_global_variable(insert_before, op_var, num_elts);
      must_clear |= CLEAR_OPS;
      if (!isa<LoadInst>(inst) && !isa<StoreInst>(inst)) {
        // Count loads and stores as zero op bits to clarify reports
        // of bits per op bit: we don't want memory bits contributing
        // to both the numerator and the denominator.
        increment_global_variable(insert_before, op_bits_var, num_bits);
        must_clear |= CLEAR_OP_BITS;
      }
      static_ops += instType->isVectorTy() ? dyn_cast<VectorType>(instType)->getNumElements() : 1;
    }
  }

  // Instrument miscellaneous instructions.
  void BytesFlops::instrument_other(Module* module,
                                    StringRef function_name,
                                    Instruction& inst,
                                    LLVMContext& bbctx,
                                    BasicBlock::iterator& insert_before,
                                    int& must_clear) {
    const Type* instType = inst.getType();    // Type of this instruction
    unsigned int opcode = inst.getOpcode();   // Current instruction's opcode
    bool tally_fp = is_fp_operation(inst, opcode, instType);  // true=floating-point; false=integer

    // Increment the flop counter and floating-point bit counter for
    // any binary instruction with a floating-point type.
    if (tally_fp) {
      ConstantInt* num_elts;    // Number of operations that this instruction performs
      ConstantInt* num_bits;    // Number of bits that this instruction produces

      num_elts = get_vector_length(bbctx, instType, one);
      num_bits = ConstantInt::get(bbctx, APInt(64, instruction_operand_bits(inst)));
      increment_global_variable(insert_before, flop_var, num_elts);
      must_clear |= CLEAR_FLOPS;
      increment_global_variable(insert_before, fp_bits_var, num_bits);
      must_clear |= CLEAR_FP_BITS;
      static_flops++;
    }

    // If the user requested a characterization of vector operations,
    // see if we have a vector operation and if so, bin it.
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
        callinst_create(tally_vector, arg_list, insert_before);
      }
      while (0);
  }

  // Do most of the instrumentation work: Walk each instruction in
  // each basic block and add instrumentation code around loads,
  // stores, flops, etc.
  void BytesFlops::instrument_entire_function(Module* module,
                                              Function& function,
                                              StringRef function_name) {
    // Tally the number of basic blocks that the function contains.
    static_bblocks += function.size();

    // generate a unique key for the function and insert call to record it
    FunctionKeyGen::KeyID keyval = record_func(function_name.str());

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

      // Insert an "unreachable" instruction as a sentinel before the
      // real terminator instruction.  New code is inserted before the
      // real terminator, and instrumentation stops at the sentinel.
      Instruction* unreachable = new UnreachableInst(bbctx, terminator_inst);

      // Acquire the mega-lock before inserting any instrumentation code.
      if (ThreadSafety)
        callinst_create(take_mega_lock, terminator_inst);

      // Iterate over the basic block's instructions one-by-one until
      // we reach the sentinal.
      for (BasicBlock::iterator iter = bb.begin();
           iter != bb.end();
           iter++) {

        // If we reach the sentinel, jump to the terminator.
        if (iter->isIdenticalTo(unreachable))
          iter = terminator_inst;

        // Ignore various function calls non grata.
        Instruction& inst = *iter;
        if (ignorable_call(&inst))
          continue;

        // Snag the current opcode for further interrogation.
        unsigned int opcode = iter->getOpcode();

        // Maintain a histogram of instructions executed.
        if (TallyInstMix) {
          ConstantInt* opCodeIdx = ConstantInt::get(bbctx,  APInt(64, int64_t(opcode)));
          increment_global_array(terminator_inst, inst_mix_histo_var, opCodeIdx, one);
        }

        // Process the current instruction.
        switch (opcode) {
          case Instruction::Load:
          case Instruction::Store:
            instrument_load_store(module, function_name, iter, bbctx,
                                  target_data, terminator_inst, must_clear);
            break;

          case Instruction::Call:
            instrument_call(module, function_name, &inst, terminator_inst,
                            must_clear);
            break;

          default:
            instrument_other(module, function_name, inst, bbctx, terminator_inst, must_clear);
            break;
        }
        instrument_all(module, function_name, inst, bbctx, terminator_inst, must_clear);
      }

      // Add one last bit of code then release the mega-lock and elide
      // the sentinel terminator.
      insert_end_bb_code(module, keyval, must_clear, terminator_inst);
      if (ThreadSafety)
        callinst_create(release_mega_lock, terminator_inst);
      unreachable->eraseFromParent();
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
      std::vector<Value*> key_args;

      ConstantInt * key = ConstantInt::get(IntegerType::get(func_ctx, 8*sizeof(FunctionKeyGen::KeyID)),
                                       keyval);
      Constant* argument = map_func_name_to_arg(module, function_name);

      key_args.push_back(argument);
      key_args.push_back(key);
        
      if ( TrackCallStack )
      {
          callinst_create(push_function, key_args, new_entry);
      }
      else
      {
          callinst_create(tally_function, key, new_entry);
      }

    }

    BranchInst::Create(&old_entry, new_entry);
  }

    

  bool BytesFlops::doFinalization(Module& module)
  {
      /**
       * The purpose is to create a static C array of function names along
       * with an array of the associated keys.  These arrays are then
       * used to create a module constructor to populate the
       * (function name -> function key) map during code execution.
       */
      LLVMContext& ctx = module.getContext();
      
      // construct C arrays of keys and strings.

      // declare an array of integers of size func_key_map.size()
      // (# of items in array).
      ArrayType *
      ArrayTy_0 = ArrayType::get(IntegerType::get(ctx, 64), func_key_map.size());

      // set up array of pointers to char (array of strings)
      PointerType *
      PointerTy = PointerType::get(IntegerType::get(ctx, 8), 0);
      ArrayType * ArrayPtrTy = ArrayType::get(PointerTy, func_key_map.size());

      ConstantInt *
      const_int32 = ConstantInt::get(ctx, APInt(32, StringRef("0"), 10));

      std::vector<Constant*> const_key_elems;
      std::vector<Constant*> const_fname_elems;

      int i = 0;
      for ( auto it = func_key_map.begin(); it != func_key_map.end(); it++ )
      {
          const std::string & name = it->first;
          auto key = it->second;

          // name.size() + 1 for NULL char.
          ArrayType *
          ArrayStrTy = ArrayType::get(IntegerType::get(ctx, 8), name.size() + 1);

          // record the key
          ConstantInt* const_int64 = ConstantInt::get(ctx, APInt(64, key));
          const_key_elems.push_back(const_int64);

          // record the name
          std::string gv_name = ".str" + std::to_string(i++);
          GlobalVariable *
          gvar_array_str = new GlobalVariable(/*Module=*/module,
                                  /*Type=*/       ArrayStrTy,
                                  /*isConstant=*/ true,
                                  /*Linkage=*/    GlobalValue::PrivateLinkage,
                                  /*Initializer=*/0, // has initializer, specified below
                                  /*Name=*/       gv_name.c_str());
          gvar_array_str->setAlignment(1);
          mark_as_used(module, gvar_array_str);

          std::vector<Constant*> const_ptr_indices;
          Constant *
          const_fname = ConstantDataArray::getString(ctx, name.c_str(), true);
          const_ptr_indices.push_back(const_int32);
          const_ptr_indices.push_back(const_int32);
          Constant *
          const_ptr = ConstantExpr::getGetElementPtr(gvar_array_str, const_ptr_indices);

          const_fname_elems.push_back(const_ptr);

          gvar_array_str->setInitializer(const_fname);
      }

      Constant* const_array_keys = ConstantArray::get(ArrayTy_0, const_key_elems);
      Constant* const_array_fnames = ConstantArray::get(ArrayPtrTy, const_fname_elems);

      //
      // set up global key array
      //
      GlobalVariable*
      gvar_key_data = new GlobalVariable(/*Module=*/module,
                         /*Type=*/        ArrayTy_0,
                         /*isConstant=*/  true,
                         /*Linkage=*/     GlobalValue::InternalLinkage,
                         /*Initializer=*/ const_array_keys, // has initializer, specified below
                         /*Name=*/        string("bf_keys") + string(".data"));
      gvar_key_data->setAlignment(16);

      PointerType* pointer_type = PointerType::get(Type::getInt64Ty(ctx), 0);
      std::vector<Constant*> getelementptr_indexes;
      getelementptr_indexes.push_back(zero);
      getelementptr_indexes.push_back(zero);
      Constant* array_key_pointer =
      ConstantExpr::getGetElementPtr(gvar_key_data, getelementptr_indexes);

      GlobalVariable *
      gvar_array_key = new GlobalVariable(/*Module=*/module,
                          /*Type=*/       pointer_type,
                          /*isConstant=*/ true,
                          /*Linkage=*/    GlobalValue::ExternalLinkage,
                          /*Initializer=*/array_key_pointer, // has initializer, specified below
                          /*Name=*/       "bf_keys");
      mark_as_used(module, gvar_array_key);


      // declare global array of function names matching keys
      // since the first item of the key array is the # of keys, then
      // the key of function bf_fname[i] is bf_key[i+1].
      GlobalVariable*
      gvar_fnames_data = new GlobalVariable(/*Module=*/module,
                  /*Type=*/       ArrayPtrTy,
                  /*isConstant=*/ true,
                  /*Linkage=*/    GlobalValue::InternalLinkage,
                  /*Initializer=*/const_array_fnames, // has initializer, specified below
                  /*Name=*/       string("bf_fnames") + string(".data"));
      gvar_fnames_data->setAlignment(16);

      // create (char **) type
      PointerType* char_ptr = PointerType::get(IntegerType::get(ctx, 8), 0);
      PointerType* char_ptr_ptr = PointerType::get(char_ptr, 0);
      std::vector<Constant*> fnames_indexes;
      fnames_indexes.push_back(zero);
      fnames_indexes.push_back(zero);
      Constant* array_fnames_pointer =
      ConstantExpr::getGetElementPtr(gvar_fnames_data, fnames_indexes);

      GlobalVariable*
      gvar_fnames = new GlobalVariable(/*Module=*/module,
                      /*Type=*/       char_ptr_ptr,
                      /*isConstant=*/ true,
                      /*Linkage=*/    GlobalValue::ExternalLinkage,
                      /*Initializer=*/array_fnames_pointer, // has initializer, specified below
                      /*Name=*/       "bf_fnames");
      mark_as_used(module, gvar_fnames);



      // now insert callto create the function map into the module constructor.
      create_func_map_ctor(module, (uint32_t)func_key_map.size(),
              array_key_pointer, array_fnames_pointer);

      return true;
  }

} // namespace bytesflops_pass

/*
 * Instrument code to keep track of run-time behavior:
 * helper methods
 *
 * By Scott Pakin <pakin@lanl.gov>
 * and Pat McCormick <pat@lanl.gov>
 */

#include "bytesflops.h"
#include <iostream>

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
  void BytesFlops::increment_global_variable(BasicBlock::iterator& insert_before,
                                             Constant* global_var,
                                             Value* increment) {
    // %0 = load i64* @<global_var>, align 8
    LoadInst* load_var = new LoadInst(global_var, "gvar", false, insert_before);

    // %1 = add i64 %0, <increment>
    BinaryOperator* inc_var =
      BinaryOperator::Create(Instruction::Add, load_var, increment,
                             "new_gvar", insert_before);

    // store i64 %1, i64* @<global_var>, align 8
    new StoreInst(inc_var, global_var, false, insert_before);
  }

  // Insert before a given instruction some code to increment an
  // element of a global array.
  void BytesFlops::increment_global_array(BasicBlock::iterator& insert_before,
                                          Constant* global_var,
                                          Value* idx,
                                          Value* increment) {
    // %1 = load i64** @<global_var>, align 8
    LoadInst* load_array = new LoadInst(global_var, "garray", false, insert_before);
    load_array->setAlignment(8);

    // %2 = getelementptr inbounds i64* %1, i64 %idx
    GetElementPtrInst* idx_ptr = GetElementPtrInst::Create(load_array, idx, "idx_ptr", insert_before);

    // %3 = load i64* %2, align 8
    LoadInst* idx_val = new LoadInst(idx_ptr, "idx_val", false, insert_before);
    idx_val->setAlignment(8);

    // %4 = add i64 %3, %increment
    BinaryOperator* inc_elt =
      BinaryOperator::Create(Instruction::Add, idx_val, increment, "new_val", insert_before);

    // store i64 %4, i64* %2, align 8
    StoreInst* store_inst = new StoreInst(inc_elt, idx_ptr, false, insert_before);
    store_inst->setAlignment(8);
  }

  // Mark a variable as "used" (not eligible for dead-code elimination).
  void mark_as_used(Module& module, Constant* protected_var) {
    LLVMContext& globctx = module.getContext();
    PointerType* ptr8 = Type::getInt8PtrTy(globctx);
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

  void BytesFlops::mark_as_used(Module& module, Constant* protected_var) {
    LLVMContext& globctx = module.getContext();
    PointerType* ptr8 = Type::getInt8PtrTy(globctx);
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

  // Create and initialize a global variable in the
  // instrumented code.
  GlobalVariable* BytesFlops::create_global_variable(Module& module,
                                                     Type* var_type,
                                                     Constant * init_value,
                                                     const char* name) {
    GlobalVariable* new_var =
      new GlobalVariable(module, var_type, false, GlobalValue::ExternalLinkage,
              init_value, name);
    mark_as_used(module, new_var);
    return new_var;
  }


  // Create and initialize a global uint64_t constant in the
  // instrumented code.
  GlobalVariable* BytesFlops::create_global_constant(Module& module,
                                                     const char* name,
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
  GlobalVariable* create_global_constant(Module& module,
                                                     const char* name,
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

  // Create and initialize a global char* constant in the instrumented
  // code.
  GlobalVariable* create_global_constant(Module& module,
                                                     const char* name,
                                                     const char* value) {
    // First, create a _local_ array of characters.
    LLVMContext& globctx = module.getContext();
    ConstantInt *
    zero = ConstantInt::get(globctx, APInt(64, 0));

    size_t num_bytes = strlen(value) + 1;   // Number of characters including the trailing '\0'
    ArrayType* array_type = ArrayType::get(Type::getInt8Ty(globctx), num_bytes);
    Constant *local_string = ConstantDataArray::getString(globctx, value, true);
    GlobalVariable* string_contents =
      new GlobalVariable(module, array_type, true, GlobalValue::PrivateLinkage,
                         local_string, string(name)+string(".data"));
    string_contents->setAlignment(8);

    // Next, create a global pointer to the local array of characters.
    std::vector<Constant*> getelementptr_indexes;
    getelementptr_indexes.push_back(zero);
    getelementptr_indexes.push_back(zero);
    Constant* array_pointer =
      ConstantExpr::getGetElementPtr(string_contents, getelementptr_indexes);
    PointerType* pointer_type = PointerType::get(Type::getInt8Ty(globctx), 0);
    GlobalVariable* new_constant =
      new GlobalVariable(module, pointer_type, true,
                         GlobalValue::LinkOnceODRLinkage, array_pointer, name);
    mark_as_used(module, new_constant);
    return new_constant;
  }

    // Create and initialize a global char* constant in the instrumented
    // code.
    GlobalVariable* create_global_constant(Module& module,
                                           const char* name,
                                           std::vector<uint64_t> & value) {
        // First, create a _local_ array of characters.
        LLVMContext& globctx = module.getContext();
        ConstantInt *
        zero = ConstantInt::get(globctx, APInt(64, 0));
        
        ArrayType* array_type = ArrayType::get(IntegerType::get(globctx, 64), value.size());

        Constant *local_array = ConstantDataArray::get(globctx, value);
        GlobalVariable* array_contents =
        new GlobalVariable(module, array_type, true, GlobalValue::PrivateLinkage,
                           local_array, string(name)+string(".data"));
        array_contents->setAlignment(16);
        
        // Next, create a global pointer to the local array of characters.
        std::vector<Constant*> getelementptr_indexes;
        getelementptr_indexes.push_back(zero);
        getelementptr_indexes.push_back(zero);
        Constant* array_pointer =
        ConstantExpr::getGetElementPtr(array_contents, getelementptr_indexes);
        PointerType* pointer_type = PointerType::get(Type::getInt64Ty(globctx), 0);
        GlobalVariable* new_constant =
        new GlobalVariable(module, pointer_type, true,
                           GlobalValue::LinkOnceODRLinkage, array_pointer, name);
        mark_as_used(module, new_constant);
        return new_constant;
    }
    

  // Create and initialize a global uint64_t constant in the
  // instrumented code.
  GlobalVariable* create_global_constant(Module& module,
                                                     const char* name,
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
                                                     const char* name,
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

    // Create and initialize a global char* constant in the instrumented
    // code.
    GlobalVariable* BytesFlops::create_global_constant(Module& module,
                                                       const char* name,
                                                       const char* value) {
        // First, create a _local_ array of characters.
        LLVMContext& globctx = module.getContext();
        size_t num_bytes = strlen(value) + 1;   // Number of characters including the trailing '\0'
        ArrayType* array_type = ArrayType::get(Type::getInt8Ty(globctx), num_bytes);
        Constant *local_string = ConstantDataArray::getString(globctx, value, true);
        GlobalVariable* string_contents =
        new GlobalVariable(module, array_type, true, GlobalValue::PrivateLinkage,
                           local_string, string(name)+string(".data"));
        string_contents->setAlignment(8);
        
        // Next, create a global pointer to the local array of characters.
        std::vector<Constant*> getelementptr_indexes;
        getelementptr_indexes.push_back(zero);
        getelementptr_indexes.push_back(zero);
        Constant* array_pointer =
        ConstantExpr::getGetElementPtr(string_contents, getelementptr_indexes);
        PointerType* pointer_type = PointerType::get(Type::getInt8Ty(globctx), 0);
        GlobalVariable* new_constant =
        new GlobalVariable(module, pointer_type, true,
                           GlobalValue::LinkOnceODRLinkage, array_pointer, name);
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
  // treated as a do-nothing operation.
  bool BytesFlops::is_no_op(const Instruction& inst,
                            const unsigned int opcode,
                            const Type* instType) {
    // Reject certain instructions that we expect to turn into no-ops
    // during code generation.
    if (ignorable_call(&inst))
      return true;
    if (isa<PHINode>(inst))
      return true;
    if (isa<BitCastInst>(inst))
      return true;
    if (isa<LandingPadInst>(inst))
      // I *think* this becomes a no-op, right?
      return true;

    // Everything else is considered an operation.
    return false;
  }

  // Return true if and only if the given instruction should be
  // tallied as a floating-point operation.
  bool BytesFlops::is_fp_operation(const Instruction& inst,
                                   const unsigned int opcode,
                                   const Type* instType) {
    // Handle a few opcodes up front, without even looking at the
    // instruction type.
    switch (opcode) {
      // We don't consider these to be floating-point operations, even
      // if LLVM does.
      case Instruction::BitCast:
      case Instruction::ExtractElement:
      case Instruction::ExtractValue:
      case Instruction::InsertElement:
      case Instruction::InsertValue:
      case Instruction::Invoke:
      case Instruction::PHI:
      case Instruction::Select:
      case Instruction::ShuffleVector:
        return false;
        break;

      // We consider these to be floating-point operations.
      case Instruction::FAdd:
      case Instruction::FCmp:
      case Instruction::FDiv:
      case Instruction::FMul:
      case Instruction::FPExt:
      case Instruction::FPToSI:
      case Instruction::FPToUI:
      case Instruction::FPTrunc:
      case Instruction::FRem:
      case Instruction::FSub:
      case Instruction::SIToFP:
      case Instruction::UIToFP:
        return true;
        break;

      default:
        break;
    }

    // Find the elemental type of instType and test that for being a
    // floating-point type.
    const Type* eltType = instType;
    while (eltType->isVectorTy())
      eltType = eltType->getVectorElementType();
    if (!eltType->isFloatingPointTy())
      return false;

    // We don't expect ever to get here.
    errs() << "bytesflops: Encountered " << inst << '\n';
    report_fatal_error("Internal error: Found unexpected FP instruction");
    return true;
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

  // Declare a function with external linkage and C calling conventions.
  Function* BytesFlops::declare_extern_c(FunctionType *return_type,
                                         StringRef func_name, Module *module)
  {
    // Don't declare the same function twice.
    Function* func = module->getFunction(func_name);
    if (!func) {
      // Create a new function.
      func = Function::Create(return_type, GlobalValue::ExternalLinkage,
                              func_name, module);
      func->setCallingConv(CallingConv::C);
    }
    return func;
  }

  // Declare a function that takes no arguments and returns no value.
  Function* BytesFlops::declare_thunk(Module* module, const char* thunk_name) {
    // Don't declare the same function twice.
    Function* oldfunc = module->getFunction(thunk_name);
    if (oldfunc)
      return oldfunc;

    // Declare a new function.
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

  // Declare an external variable.
  GlobalVariable* BytesFlops::declare_global_var(Module& module,
                                                 Type* var_type,
                                                 StringRef var_name,
                                                 bool is_const) {
    // Don't declare the same variable twice in a single module.
    GlobalVariable* oldvar = module.getGlobalVariable(var_name);
    if (oldvar)
      return oldvar;
    else
      return new GlobalVariable(module, var_type, is_const,
                                GlobalVariable::ExternalLinkage, 0,
                                var_name, 0, GlobalVariable::NotThreadLocal);
  }

  // Create an external variable.
//  GlobalVariable* BytesFlops::create_global_var(Module& module,
//                                                 Type* var_type,
//                                                 StringRef var_name,
//                                                 size_t nelts) {
//    // Don't declare the same variable twice in a single module.
//    GlobalVariable* oldvar = module.getGlobalVariable(var_name);
//    if (oldvar)
//    {
//      return oldvar;
//    }
//    else
//    {
//        oldvar = new GlobalVariable(module, var_type, is_const,
//                                    GlobalVariable::ExternalLinkage, 0,
//                                    var_name, 0, GlobalVariable::NotThreadLocal);
//
//    }
//
//  }

  // Insert code to set every element of a given array to zero.
  void BytesFlops::insert_zero_array_code(Module* module,
                                          GlobalVariable* array_to_zero,
                                          uint64_t num_elts,
                                          BasicBlock::iterator& insert_before) {
    LoadInst* array_addr = new LoadInst(array_to_zero, "ar", false, insert_before);
    array_addr->setAlignment(8);
    LLVMContext& globctx = module->getContext();
    CastInst* array_addr_cast =
      new BitCastInst(array_addr,
                      PointerType::get(IntegerType::get(globctx, 8), 0),
                      "arv", insert_before);
    static ConstantInt* zero_8bit =
      ConstantInt::get(globctx, APInt(8, 0));
    static ConstantInt* array_align =
      ConstantInt::get(globctx, APInt(32, sizeof(uint64_t)));
    static ConstantInt* zero_1bit =
      ConstantInt::get(globctx, APInt(1, 0));
    ConstantInt* array_bytes =
      ConstantInt::get(globctx, APInt(64, num_elts*sizeof(uint64_t)));
    std::vector<Value*> func_args;
    func_args.push_back(array_addr_cast);
    func_args.push_back(zero_8bit);
    func_args.push_back(array_bytes);
    func_args.push_back(array_align);
    func_args.push_back(zero_1bit);
    callinst_create(memset_intrinsic, func_args, insert_before);
  }

  // Insert code at the end of a basic block.
  void BytesFlops::insert_end_bb_code (Module* module, KeyType_t funcKey,
                                       int& must_clear,
                                       BasicBlock::iterator& insert_before) {
    // Keep track of how the basic block terminated.
    Instruction& inst = *insert_before;
    unsigned int opcode = inst.getOpcode();   // Terminator instruction's opcode
    LLVMContext& globctx = module->getContext();
    int bb_end_type;
    switch (opcode) {
      case Instruction::IndirectBr:
      case Instruction::Switch:
        bb_end_type = BF_END_BB_DYNAMIC;
        increment_global_array(insert_before, terminator_var,
                               ConstantInt::get(globctx, APInt(64, bb_end_type)),
                               one);
        break;

      case Instruction::Br:
        if (dyn_cast<BranchInst>(&inst)->isConditional()) {
          bb_end_type = BF_END_BB_DYNAMIC;
          static_cond_brs++;
        }
        else
          bb_end_type = BF_END_BB_STATIC;
        increment_global_array(insert_before, terminator_var,
                               ConstantInt::get(globctx, APInt(64, bb_end_type)),
                               one);
        break;

      default:
        break;
    }
    increment_global_array(insert_before, terminator_var,
                           ConstantInt::get(globctx, APInt(64, BF_END_BB_ANY)),
                           one);

    // If we're instrumenting every basic block, insert calls to
    // bf_accumulate_bb_tallies() and bf_report_bb_tallies().
    if (InstrumentEveryBB) {
      callinst_create(accum_bb_tallies, insert_before);
      callinst_create(report_bb_tallies, insert_before);
    }

    // If we're instrumenting by function, insert a call to
    // bf_assoc_counters_with_func() at the end of the basic block.
    if (TallyByFunction) {
      vector<Value*> arg_list;
      ConstantInt * key = ConstantInt::get(IntegerType::get(globctx, 8*sizeof(FunctionKeyGen::KeyID)),
                                           funcKey);
      arg_list.push_back(key);
      callinst_create(assoc_counts_with_func, arg_list, insert_before);
    }

    // Reset all of our counter variables.
    if (InstrumentEveryBB || TallyByFunction) {
      if (must_clear & CLEAR_LOADS) {
        new StoreInst(zero, load_var, false, insert_before);
        new StoreInst(zero, load_inst_var, false, insert_before);
      }
      if (must_clear & CLEAR_STORES) {
        new StoreInst(zero, store_var, false, insert_before);
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
      insert_zero_array_code(module, terminator_var, BF_END_BB_NUM, insert_before);
      insert_zero_array_code(module, mem_intrinsics_var, BF_NUM_MEM_INTRIN, insert_before);
      must_clear = 0;
    }

    // If we're instrumenting every basic block, insert a call to
    // bf_reset_bb_tallies().
    if (InstrumentEveryBB)
      callinst_create(reset_bb_tallies, insert_before);

    // If we're instrumenting by call stack, insert a call to
    // bf_pop_function() at every return from the function.
    if (TrackCallStack && insert_before->getOpcode() == Instruction::Ret)
      callinst_create(pop_function, insert_before);
  }

  // Wrap CallInst::Create() with a more convenient interface.
  void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
                                   Instruction* insert_before) {
    CallInst::Create(function, args, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but for parameterless functions.
  void BytesFlops::callinst_create(Value* function, Instruction* insert_before) {
    CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but with a different parameter list.
  void BytesFlops::callinst_create(Value* function, BasicBlock* insert_before) {
    CallInst::Create(function, "", insert_before)->setCallingConv(CallingConv::C);
  }

  // Ditto the above but for functions with arguments.
  void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
                                   BasicBlock* insert_before) {
      CallInst * inst = CallInst::Create(function, args, "", insert_before);
      if ( !inst )
      {
          std::cerr << "Error: unable to create call instruction to "
                  << function->getName().str() << std::endl;
      }
      else
      {
          inst->setCallingConv(CallingConv::C);
      }
  }

  // Given a Call instruction, return true if we can safely ignore it.
  bool BytesFlops::ignorable_call (const Instruction* inst) {
    // Ignore debug intrinsics (llvm.dbg.*).
    if (isa<DbgInfoIntrinsic>(*inst))
      return true;

    // Ignore lifetime intrinsics (llvm.lifetime.*).
    if (isa<CallInst>(inst)) {
      Function* func = dyn_cast<CallInst>(inst)->getCalledFunction();
      if (func) {
        StringRef callee_name = func->getName();
        if (callee_name.startswith("llvm.lifetime"))
          return true;
      }
    }

    // Pay attention to everything else.
    return false;
  }

  // Count the number of "real" instructions in a basic block.
  size_t BytesFlops::bb_size(const BasicBlock& bb) {
    size_t tally = 0;
    BasicBlock::const_iterator inst;
    for (inst = bb.getFirstInsertionPt(); inst != bb.end(); inst++)
      tally++;
    return tally;
  }

} // namespace bytesflops_pass

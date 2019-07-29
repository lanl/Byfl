/*
 * Instrument code to keep track of run-time behavior:
 * LLVM pass initialization
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "bytesflops.h"

namespace bytesflops_pass {

  // Prepend a function to the list of constructors.
  static void prepend_to_ctor_list (Module* module, Function* func) {
    // Determine the number of elements in the final array.
    LLVMContext& globctx = module->getContext();
    GlobalVariable* existing_ctors =
      module->getGlobalVariable("llvm.global_ctors");
    unsigned int numelts = 1;
    if (existing_ctors != nullptr)
      if (PointerType* ctor_type = dyn_cast<PointerType>(existing_ctors->getType()))
        if (ArrayType* array = dyn_cast<ArrayType>(ctor_type->getPointerElementType()))
          numelts += array->getNumElements();

    // Define a new constructors array.
    std::vector<Type*> no_args;
    FunctionType* thunk =
      FunctionType::get(Type::getVoidTy(globctx), no_args, false);
    PointerType* ptr_to_thunk = PointerType::get(thunk, 0);
    std::vector<Type*>ctor_record_fields;
    ctor_record_fields.push_back(IntegerType::get(globctx, 32));
    ctor_record_fields.push_back(ptr_to_thunk);
    Constant* null_pointer =
      ConstantPointerNull::get(PointerType::get(IntegerType::get(globctx, 8), 0));
    ctor_record_fields.push_back(null_pointer->getType());
    StructType *ctor_record =
      StructType::get(globctx, ctor_record_fields, false);
    ArrayType* ctor_array = ArrayType::get(ctor_record, numelts);

    // Wrap the given function in a ctor_record and insert that into the
    // constructor array.
    std::vector<Constant*> ctor_elems;
    std::vector<Constant*> ctor_elem_fields;
    ctor_elem_fields.push_back(ConstantInt::get(globctx, APInt(32, StringRef("65535"), 10)));
    ctor_elem_fields.push_back(func);
    ctor_elem_fields.push_back(null_pointer);
    ctor_elems.push_back(ConstantStruct::get(ctor_record, ctor_elem_fields));

    // Prepend the existing constructors to the new array.
    if (existing_ctors != nullptr) {
      Constant* initializer = existing_ctors->getInitializer();
      if (isa<ConstantArray>(initializer) || isa<ConstantAggregateZero>(initializer)) {
        for (unsigned int e = 0; e < numelts - 1; e++) {
          Constant* elt = initializer->getAggregateElement(e);
          assert(isa<ConstantStruct>(elt));
          ctor_elems.push_back(elt);
        }
      }
      else
        report_fatal_error("Encountered unexpected type of llvm.global_ctors");
    }

    // Remove the old constructor array (if any) and create a new constructor
    // array.
    if (existing_ctors != nullptr)
      existing_ctors->eraseFromParent();
    Constant* new_ctors = ConstantArray::get(ctor_array, ctor_elems);
    new GlobalVariable(*module, ctor_array, false, GlobalValue::AppendingLinkage,
                       new_ctors, "llvm.global_ctors");
  }

  /* Create a module constructor function to create the map from function keys
   * to function names.  This function will be automatically called when the
   * module is loaded. */
  void BytesFlops::initializeKeyMap(Module* module) {
    // Declare the bf_func_key_map_ctor() function.
    const char* funcname = "bf_func_key_map_ctor";
    func_map_ctor = module->getFunction(funcname);
    if (func_map_ctor != nullptr)
      return;
    func_map_ctor = declare_thunk(module, funcname);
    func_map_ctor->setLinkage(GlobalValue::InternalLinkage);

    // Prepend bf_func_key_map_ctor() to the list of constructors.
    prepend_to_ctor_list(module, func_map_ctor);
  }

  /*
   * Define a constructor called bf_track_global_vars_ctor() with the following
   * form, invoking bf_assoc_addresses_with_sstruct() for each global variable
   * declared in the module:
   *
   * __attribute__((constructor))
   * static void bf_track_global_vars_ctor (void)
   * {
   *   bf_initialize_if_necessary();
   *   bf_assoc_addresses_with_sstruct(...);
   *   bf_assoc_addresses_with_sstruct(...);
   *   bf_assoc_addresses_with_sstruct(...);
   *   bf_assoc_addresses_with_sstruct(...);
   *                  ...
   * }
   */
  void BytesFlops::track_global_variables (Module* module) {
    // Declare the bf_track_global_vars_ctor() function.
    const char* funcname = "bf_track_global_vars_ctor";
    Function* func = module->getFunction(funcname);
    if (func != nullptr)
      return;
    func = declare_thunk(module, funcname);
    func->setLinkage(GlobalValue::InternalLinkage);

    // Prepend bf_track_global_vars_ctor() to the list of constructors.
    prepend_to_ctor_list(module, func);

    // Add a single basic block to bf_track_global_vars_ctor().
    LLVMContext& globctx = module->getContext();
    BasicBlock* bblock = BasicBlock::Create(globctx, "entry", func);
    ReturnInst* ret_inst = ReturnInst::Create(globctx, bblock);

    // Inject a call to bf_initialize_if_necessary().
    callinst_create(init_if_necessary, ret_inst);

    // Each instruction in the basic block is a call to
    // bf_assoc_addresses_with_sstruct().
    vector<Value*> arg_list;
    NamedMDNode* nmd = module->getNamedMetadata("llvm.dbg.cu");
    if (nmd == nullptr)
      return;   // No named metadata
    const DataLayout& target_data = module->getDataLayout();
    AllocaInst* syminfo_struct = nullptr;   // bf_symbol_info_t structure
    PointerType* void_ptr = PointerType::get(IntegerType::get(globctx, 8), 0);

    Module::GlobalListType& gv_list = module->getGlobalList();
    for (auto gv_iter = gv_list.begin(); gv_iter != gv_list.end(); gv_iter++) {
      // Acquire debug information for one global variable.
      GlobalVariable& gv_var = *gv_iter;
      SmallVector<DIGlobalVariableExpression*,1> gvs;
      gv_var.getDebugInfo(gvs);
      if (gvs.size() == 0)
        continue;
      DIGlobalVariableExpression* gv = gvs[0];
      Type* gv_type  = gv_var.getType();   // Data type
      if (gv_type->isPointerTy())
        gv_type = gv_type->getPointerElementType();
      uint64_t byte_count = target_data.getTypeStoreSize(gv_type);
      if (byte_count == 0)
        continue;    // We can never access zero-sized data.
      StringRef segment = gv_var.isZeroValue() ? ".bss" : ".data";
      InternalSymbolInfo gv_info(*gv, segment.str());

      // Inject a call to bf_assoc_addresses_with_sstruct().
      vector<Value*> arg_list;
      CastInst* base_addr = new BitCastInst(&gv_var, void_ptr, "const_ptr", ret_inst);
      syminfo_struct = find_value_provenance(*module, gv_info, ret_inst, syminfo_struct);
      arg_list.push_back(syminfo_struct);
      arg_list.push_back(base_addr);
      arg_list.push_back(ConstantInt::get(globctx, APInt(64, byte_count)));
      callinst_create(assoc_addrs_with_sstruct, arg_list, ret_inst);
    }
  }

  // Initialize the BytesFlops pass.
  bool BytesFlops::doInitialization(Module& module) {
    // Inject external declarations to various variables defined in byfl.c.
    LLVMContext& globctx = module.getContext();
    IntegerType* i32type = Type::getInt32Ty(globctx);
    IntegerType* i64type = Type::getInt64Ty(globctx);
    PointerType* i64ptrtype = Type::getInt64PtrTy(globctx);
    mem_insts_var       = declare_global_var(module, i64ptrtype, "bf_mem_insts_count", true);
    inst_mix_histo_var  = declare_global_var(module, i64ptrtype, "bf_inst_mix_histo", true);
    terminator_var      = declare_global_var(module, i64ptrtype, "bf_terminator_count", true);
    mem_intrinsics_var  = declare_global_var(module, i64ptrtype, "bf_mem_intrin_count", true);
    load_var        = declare_global_var(module, i64type, "bf_load_count");
    store_var       = declare_global_var(module, i64type, "bf_store_count");
    load_inst_var   = declare_global_var(module, i64type, "bf_load_ins_count");
    store_inst_var  = declare_global_var(module, i64type, "bf_store_ins_count");
    flop_var        = declare_global_var(module, i64type, "bf_flop_count");
    fp_bits_var     = declare_global_var(module, i64type, "bf_fp_bits_count");

    op_var          = declare_global_var(module, i64type, "bf_op_count");
    op_bits_var     = declare_global_var(module, i64type, "bf_op_bits_count");
    call_inst_var   = declare_global_var(module, i64type, "bf_call_ins_count");

    // bf_inst_deps_histo is a bit tricky because it's a 3D array.
    ArrayType* i64array1Dtype = ArrayType::get(i64type, 2);
    ArrayType* i64array2Dtype = ArrayType::get(i64array1Dtype, NUM_LLVM_OPCODES_POW2);
    ArrayType* i64array3Dtype = ArrayType::get(i64array2Dtype, NUM_LLVM_OPCODES_POW2);
    ArrayType* i64array4Dtype = ArrayType::get(i64array3Dtype, NUM_LLVM_OPCODES_POW2);
    inst_deps_histo_var = declare_global_var(module, i64array4Dtype, "bf_inst_deps_histo", false);

    // Declare a few argument types we intend to use in multiple declarations.
    IntegerType* uint8_arg = IntegerType::get(globctx, 8);
    IntegerType* uint64_arg = IntegerType::get(globctx, 64);
    PointerType* ptr_to_char_arg = PointerType::get(uint8_arg, 0);

    // Declare a bf_symbol_info_t struct type and a pointer to it.
    syminfo_type = module.getTypeByName("struct.bf_symbol_info_t");
    if (syminfo_type == nullptr) {
      syminfo_type = StructType::create(globctx, "struct.bf_symbol_info_t");
      std::vector<Type*> syminfo_fields;
      syminfo_fields.push_back(i64type);
      syminfo_fields.push_back(ptr_to_char_arg);
      syminfo_fields.push_back(ptr_to_char_arg);
      syminfo_fields.push_back(ptr_to_char_arg);
      syminfo_fields.push_back(ptr_to_char_arg);
      syminfo_fields.push_back(i32type);
      syminfo_type->setBody(syminfo_fields, false);
    }
    PointerType* ptr_to_syminfo_arg = PointerType::get(syminfo_type, 0);

    // Assign a few constant values.
    not_end_of_bb = ConstantInt::get(globctx, APInt(32, 0));
    uncond_end_bb = ConstantInt::get(globctx, APInt(32, 1));
    cond_end_bb = ConstantInt::get(globctx, APInt(32, 2));
    zero = ConstantInt::get(globctx, APInt(64, 0));
    one = ConstantInt::get(globctx, APInt(64, 1));
    null_pointer = ConstantPointerNull::get(PointerType::get(IntegerType::get(globctx, 8), 0));
    null_syminfo_pointer = ConstantPointerNull::get(ptr_to_syminfo_arg);

    // Construct a set of functions to instrument and a set of
    // functions not to instrument.
    instrument_only = parse_function_names(IncludedFunctions);
    dont_instrument = parse_function_names(ExcludedFunctions);
    if (instrument_only && dont_instrument)
      report_fatal_error("-bf-include and -bf-exclude are mutually exclusive");

    // Assign a value to bf_bb_merge.
    create_global_constant(module, "bf_bb_merge", uint64_t(BBMergeCount));

    // Assign a value to bf_every_bb.
    create_global_constant(module, "bf_every_bb", bool(InstrumentEveryBB));

    // Assign a value to bf_types.
    create_global_constant(module, "bf_types", bool(TallyTypes));

    // Assign a value to bf_tally_inst_mix (instruction mix).
    create_global_constant(module, "bf_tally_inst_mix", bool(TallyInstMix));

    // Assign a value to bf_tally_inst_deps (instruction dependencies).
    create_global_constant(module, "bf_tally_inst_deps", bool(TallyInstDeps));

    // Assign a value to bf_per_func.
    create_global_constant(module, "bf_per_func", bool(TallyByFunction));

    // Assign a value to bf_call_stack.
    if (TrackCallStack && !TallyByFunction)
      report_fatal_error("-bf-call-stack is allowed only in conjuction with -bf-by-func");
    create_global_constant(module, "bf_call_stack", bool(TrackCallStack));

    // Assign a value to bf_mem_footprint.
    create_global_constant(module, "bf_mem_footprint", bool(FindMemFootprint));

    // Assign a value to bf_unique_bytes.
    create_global_constant(module, "bf_unique_bytes", bool(TrackUniqueBytes) || bool(FindMemFootprint));

    // Assign a value to bf_vectors.
    create_global_constant(module, "bf_vectors", bool(TallyVectors));

    // Assign a value to bf_data_structs.
    create_global_constant(module, "bf_data_structs", bool(TallyByDataStruct));

    // Assign a value to bf_strides.
    create_global_constant(module, "bf_strides", bool(TrackStrides));

    // Assign a value to bf_max_reuse_dist.
    create_global_constant(module, "bf_max_reuse_distance", uint64_t(MaxReuseDist));

    // Assign a value to bf_cache_model.
    create_global_constant(module, "bf_cache_model", bool(CacheModel));

    // Assign a value to bf_line_size.
    create_global_constant(module, "bf_line_size", uint64_t(CacheLineBytes));

    // Assign a value to bf_max_sets.
    create_global_constant(module, "bf_max_set_bits", uint64_t(CacheMaxSetBits));

    // Create a global string that stores all of our command-line options.
    vector<string> command_line = parse_command_line();   // All command-line arguments
    string bf_cmdline;   // Reconstructed command line with -bf-* options only
    for (auto iter = command_line.cbegin(); iter != command_line.end(); iter++)
      if (iter->compare(0, 3, "-bf") == 0) {
        bf_cmdline += ' ';
        bf_cmdline += *iter;
      }
    if (bf_cmdline.length() == 0 && command_line[0].compare(0, 7, "[failed") == 0)
      bf_cmdline = command_line[0];
    else
      bf_cmdline.erase(0, 1);  // Remove the leading space character.
    create_global_constant(module, "bf_option_string", strdup(bf_cmdline.c_str()));

    /* Instead of using a map with the function names as keys, we associate a
     * unique integer with each function and use that as the key.  We maintain
     * the association of the function names to their integer keys at compile
     * time, then create a constructor to record the map via a call to
     * bf_record_funcs2keys. */
    vector<Type*> func_arg;
    func_arg.push_back(IntegerType::get(globctx, 8*sizeof(uint32_t)));
    func_arg.push_back(PointerType::get(IntegerType::get(globctx, 8*sizeof(uint64_t)),0));
    PointerType* char_ptr_ptr = PointerType::get(ptr_to_char_arg, 0);
    func_arg.push_back(char_ptr_ptr);
    FunctionType* void_int_func_result =
      FunctionType::get(Type::getVoidTy(globctx), func_arg, false);
    record_funcs2keys = declare_extern_c(void_int_func_result,
                                         "bf_record_funcs2keys",
                                         &module);

    // Inject an external declarations for bf_initialize_if_necessary().
    init_if_necessary = declare_thunk(&module, "bf_initialize_if_necessary");

    // Inject external declarations for bf_accumulate_bb_tallies(),
    // bf_reset_bb_tallies(), bf_report_bb_tallies(), and
    // bf_tally_bb_execution().
    if (InstrumentEveryBB) {
      // Declare the zero-argument functions first.
      accum_bb_tallies = declare_thunk(&module, "bf_accumulate_bb_tallies");
      reset_bb_tallies = declare_thunk(&module, "bf_reset_bb_tallies");

      // Declare bf_report_bb_tallies().
      vector<Type*> func_args;
      func_args.push_back(ptr_to_syminfo_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), func_args, false);
      report_bb_tallies =
        declare_extern_c(void_func_result, "bf_report_bb_tallies", &module);

      // Declare bf_tally_bb_execution().
      func_args.clear();
      func_args.push_back(ptr_to_syminfo_arg);
      func_args.push_back(uint64_arg);
      func_args.push_back(uint64_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), func_args, false);
      tally_bb_exec =
        declare_extern_c(void_func_result, "bf_tally_bb_execution", &module);
    }

    // Inject an external declarations for bf_increment_func_tally().
    assoc_counts_with_func = 0;
    tally_function = 0;
    push_function = 0;
    pop_function = 0;

    if (TallyByFunction) {
      // bf_assoc_counters_with_func
      vector<Type*> func_arg;
      IntegerType* keyid_arg = IntegerType::get(globctx, 8*sizeof(FunctionKeyGen::KeyID));
      func_arg.push_back(keyid_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), func_arg, false);
      assoc_counts_with_func =
        declare_extern_c(void_func_result,
                         "bf_assoc_counters_with_func",
                         &module);

      // bf_incr_func_tally
      func_arg.clear();
      func_arg.push_back(keyid_arg);
      func_arg.push_back(ptr_to_syminfo_arg);
      FunctionType* void_int_ptr_func_result =
        FunctionType::get(Type::getVoidTy(globctx), func_arg, false);
      tally_function =
        declare_extern_c(void_int_ptr_func_result,
                         "bf_incr_func_tally",
                         &module);

      if (TrackCallStack) {
        // Inject external declarations for bf_push_function() and
        // bf_pop_function().
        func_arg.clear();
        func_arg.push_back(ptr_to_char_arg);
        func_arg.push_back(keyid_arg);
        func_arg.push_back(ptr_to_syminfo_arg);
        FunctionType* void_str_int_ptr_func_result =
          FunctionType::get(Type::getVoidTy(globctx), func_arg, false);
        push_function =
          declare_extern_c(void_str_int_ptr_func_result,
                           "bf_push_function",
                           &module);
        pop_function = declare_thunk(&module, "bf_pop_function");
      }
    }

    // Declare bf_tally_vector_operation() only if we were asked
    // to track vector operations.
    if (TallyVectors) {
      vector<Type*> all_function_args;
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint8_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      tally_vector =
        declare_extern_c(void_func_result,
                         "bf_tally_vector_operation",
                         &module);
    }

    // Inject external declarations for bf_assoc_addresses_with_prog()
    // and bf_assoc_addresses_with_func().
    if (TrackUniqueBytes || FindMemFootprint) {
      // Declare bf_assoc_addresses_with_prog() any time we need to
      // track unique bytes.
      vector<Type*> all_function_args;
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      assoc_addrs_with_prog =
        declare_extern_c(void_func_result,
                         FindMemFootprint
                         ? "bf_assoc_addresses_with_prog_tb"
                         : "bf_assoc_addresses_with_prog",
                         &module);

      // Declare bf_assoc_addresses_with_func() only if we were
      // asked to track unique addresses by function.
      if (TallyByFunction) {
        vector<Type*> all_function_args;
        all_function_args.push_back(ptr_to_char_arg);
        all_function_args.push_back(uint64_arg);
        all_function_args.push_back(uint64_arg);
        FunctionType* void_func_result =
          FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
        assoc_addrs_with_func =
          declare_extern_c(void_func_result,
                           FindMemFootprint
                           ? "bf_assoc_addresses_with_func_tb"
                           : "bf_assoc_addresses_with_func",
                           &module);
      }
    }

    // Declare bf_assoc_addresses_with_sstruct(),
    // bf_assoc_addresses_with_dstruct(), bf_assoc_addresses_with_dstruct_pm(),
    // bf_assoc_addresses_with_dstruct_stack(), and bf_access_data_struct()
    // only if we were asked to track data-structure acceses.
    if (TallyByDataStruct) {
      vector<Type*> all_function_args;
      FunctionType* void_func_result;

      // Declare bf_assoc_addresses_with_sstruct().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(uint64_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      assoc_addrs_with_sstruct =
        declare_extern_c(void_func_result,
                         "bf_assoc_addresses_with_sstruct",
                         &module);

      // Declare bf_assoc_addresses_with_dstruct().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(uint64_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      assoc_addrs_with_dstruct =
        declare_extern_c(void_func_result,
                         "bf_assoc_addresses_with_dstruct",
                         &module);

      // Declare bf_assoc_addresses_with_dstruct_pm().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(PointerType::get(ptr_to_char_arg, 0));
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(IntegerType::get(globctx, 32));
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      assoc_addrs_with_dstruct_pm =
        declare_extern_c(void_func_result,
                         "bf_assoc_addresses_with_dstruct_pm",
                         &module);

      // Declare bf_assoc_addresses_with_dstruct_stack().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(uint64_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      assoc_addrs_with_dstruct_stack =
        declare_extern_c(void_func_result,
                         "bf_assoc_addresses_with_dstruct_stack",
                         &module);

      // Declare bf_disassoc_addresses_with_dstruct().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_char_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      disassoc_addrs_with_dstruct =
        declare_extern_c(void_func_result,
                         "bf_disassoc_addresses_with_dstruct",
                         &module);

      // Declare bf_access_data_struct().
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint8_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      access_data_struct =
        declare_extern_c(void_func_result,
                         "bf_access_data_struct",
                         &module);
    }

    // Declare bf_touch_cache() only if we are asked to use it.
    if (CacheModel) {
      vector<Type*> all_function_args;
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      access_cache =
        declare_extern_c(void_func_result,
                         "_ZN10bytesflops14bf_touch_cacheEmm",
                         &module);
    }

    // Declare bf_track_stride() only if we were asked to track access strides.
    if (TrackStrides) {
      vector<Type*> all_function_args;
      FunctionType* void_func_result;
      all_function_args.clear();
      all_function_args.push_back(ptr_to_syminfo_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint8_arg);
      all_function_args.push_back(uint8_arg);
      void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      track_stride = declare_extern_c(void_func_result, "bf_track_stride", &module);
    }

    // Inject an external declaration for llvm.memset.p0i8.i64().
    memset_intrinsic = module.getFunction("llvm.memset.p0i8.i64");
    if (memset_intrinsic == NULL) {
      vector<Type*> all_function_args;
      all_function_args.push_back(ptr_to_char_arg);
      all_function_args.push_back(uint8_arg);
      all_function_args.push_back(uint64_arg);
#if LLVM_VERSION_MAJOR <= 6
      all_function_args.push_back(IntegerType::get(globctx, 32));
#endif
      all_function_args.push_back(IntegerType::get(globctx, 1));
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      memset_intrinsic =
        declare_extern_c(void_func_result,
                         "llvm.memset.p0i8.i64",
                         &module);
    }

    // Simplify ReuseDist.getBits() into rd_bits.
    rd_bits = ReuseDist.getBits();
    if ((rd_bits&(1<<RD_BOTH)) != 0)
      rd_bits = (1<<RD_LOADS) | (1<<RD_STORES);

    // Inject external declarations for bf_reuse_dist_addrs_prog().
    if (rd_bits > 0) {
      vector<Type*> all_function_args;
      all_function_args.push_back(uint64_arg);
      all_function_args.push_back(uint64_arg);
      FunctionType* void_func_result =
        FunctionType::get(Type::getVoidTy(globctx), all_function_args, false);
      reuse_dist_prog =
        declare_extern_c(void_func_result,
                         "bf_reuse_dist_addrs_prog",
                         &module);
    }

    // Inject external declarations for bf_acquire_mega_lock() and
    // bf_release_mega_lock().
    if (ThreadSafety) {
      take_mega_lock = declare_thunk(&module, "bf_acquire_mega_lock");
      release_mega_lock = declare_thunk(&module, "bf_release_mega_lock");
    }

    // Initialize the function key generator.
    FunctionKeyGen::Seed_t seed;
    std::hash<std::string> hash_key;
    seed = (hash_key(module.getModuleIdentifier())*UINT64_C(281474976710677) +
            getpid()*UINT64_C(4294967311) +
            time(NULL)*UINT64_C(65537));
    m_keygen = std::unique_ptr<FunctionKeyGen>(new FunctionKeyGen(seed));

    // Track all of our global variables.
    if (TallyByDataStruct)
      track_global_variables(&module);

    return true;
  }

  char BytesFlops::ID = 0;

  // Insert code for incrementing our byte, flop, etc. counters.
  bool BytesFlops::runOnFunction(Function& function) {
    // Do nothing if we're supposed to ignore this function.
    StringRef function_name = function.getName();
    string function_name_orig = demangle_func_name(function_name.str());
    remove_all_instances(function_name_orig, ' ');  // Normalize the name by removing spaces.
    if (instrument_only != nullptr &&
        instrument_only->find(function_name) == instrument_only->end() &&
        instrument_only->find(function_name_orig) == instrument_only->end())
      return false;
    if (dont_instrument != nullptr &&
        (dont_instrument->find(function_name) != dont_instrument->end() ||
         dont_instrument->find(function_name_orig) != dont_instrument->end()))
      return false;
    if (function_name == "bf_categorize_counters")
      // Avoid the endless recursion that would be caused if we were
      // to instrument bf_categorize_counters() using
      // bf_categorize_counters().
      return false;
    if (function_name == "bf_func_key_map_ctor" || function_name == "bf_track_global_vars_ctor")
      // Ignore other Byfl-defined functions, too.
      return false;
    if (function_name == "_Znwm" || function_name == "_ZdlPv" || function_name == "_ZdaPv")
      // Avoid the endless recursion that would be caused if we were
      // to instrument operator new, operator delete, or operator
      // delete[], all of which we invoke from the run-time library.
      return false;
    if (function.empty())
      return false;

    // Reset all of our static counters.
    static_loads = 0;
    static_stores = 0;
    static_flops = 0;
    static_ops = 0;
    static_cond_brs = 0;
    static_bblocks = 0;

    // Construct the instruction_to_string map, which maps an Instruction* to a
    // string.
    map_instructions_to_strings(function);

    // Instrument "interesting" instructions in every basic block.
    Module* module = function.getParent();
    instrument_entire_function(module, function, function_name);

    // Return, indicating that we modified this function.
    return true;
  }

  // For each function in the module, run a function pass on it.
  bool BytesFlops::runOnModule(Module& module) {
    doInitialization(module);
    for (auto fiter = module.begin(); fiter != module.end(); fiter++)
      runOnFunction(*fiter);
    doFinalization(module);
    return true;
  }

  // Output what we instrumented.
  void BytesFlops::print(raw_ostream &outfile, const Module *module) const {
    outfile << module->getModuleIdentifier() << ": "
            << static_loads << " loads, "
            << static_stores << " stores, "
            << static_flops << " flops, "
            << static_cond_brs << " cond_brs, "
            << static_ops << " total_ops, "
            << static_bblocks << " bblocks\n";
  }

  // Make the BytesFlops pass run automatically when loaded via Clang.
  // This technique comes from "Run an LLVM Pass Automatically with
  // Clang" (http://homes.cs.washington.edu/~asampson/blog/clangpass.html).
  //
  // We want our pass to run as late as possible.  Unfortunately,
  // EP_OptimizerLast is leading to segmentation faults so we use
  // EP_ScalarOptimizerLate instead.  However, that doesn't run when
  // optimizations are disabled so we additionally have to register
  // our pass to run with EP_EnabledOnOptLevel0.  Run-time checks of
  // the optimization level ensure that the pass executes exactly
  // once.
  static void registerByflPass_O0(const PassManagerBuilder& pass_mgr_builder,
                                  PassManagerBase& pass_mgr) {
    if (pass_mgr_builder.OptLevel == 0)
      pass_mgr.add(new BytesFlops());
  }
  static RegisterStandardPasses
  RegisterByflPass_O0(PassManagerBuilder::EP_EnabledOnOptLevel0, registerByflPass_O0);
  static void registerByflPass_opt(const PassManagerBuilder& pass_mgr_builder,
                                  PassManagerBase& pass_mgr) {
    if (pass_mgr_builder.OptLevel > 0)
      pass_mgr.add(new BytesFlops());
  }
  static RegisterStandardPasses
  RegisterByflPass_opt(PassManagerBuilder::EP_ScalarOptimizerLate, registerByflPass_opt);

}  // namespace bytesflops_pass

/*
 * Instrument code to keep track of run-time behavior:
 * helper methods
 *
 * By Scott Pakin <pakin@lanl.gov>
 *    Pat McCormick <pat@lanl.gov>
 *    Rob Aulwes <rta@lanl.gov>
 */

#include "bytesflops.h"
#include <iostream>

namespace bytesflops_pass {

// Tally the number of instances of a given character in a string.
static size_t tally_all_instances(string& some_string, char some_char)
{
  size_t tally = 0;
  for (string::const_iterator iter = some_string.cbegin(); iter != some_string.cend(); iter++)
    if (*iter == some_char)
      tally++;
  return tally;
}

// Read a list of function names, one per line, from a file into a
// set.  C++ function names can be either mangled or unmangled.
static void functions_from_file(string filename, set<string>* funcset)
{
  ifstream infile(filename.c_str(), ifstream::in);
  if (!infile.good())
    report_fatal_error(StringRef("Failed to open file ") + filename);
  string oneline;
  while (infile.good() && getline(infile, oneline))
    {
      // Normalize unmangled names by removing spaces then insert the
      // result into the set.
      remove_all_instances(oneline, ' ');
      funcset->insert(oneline);
    }
  infile.close();
}

// Report whether an unconditional branch is mandatory or if it can
// feasibly be removed at code-generation time.
static bool uncond_branch_is_mandatory (BranchInst* br_inst)
{
  // An unconditional branch is defined as removable if and only if
  // it is the only unconditional branch to its successor.  To
  // determine this, we iterate over all basic blocks that branch to
  // the same basic block that br_inst (in basic block bb) branches
  // to.  If there exists a basic block other than bb that
  // unconditionally branches to bb's target we return true,
  // otherwise false.
  const BasicBlock* bb = br_inst->getParent();
  BasicBlock* succ_bb = br_inst->getSuccessor(0);  // Assume there is exactly one successor.
  for (pred_iterator iter = pred_begin(succ_bb), last_iter = pred_end(succ_bb);
       iter != last_iter;
       iter++) {
    const BasicBlock* pred_bb = *iter;
    if (pred_bb == bb)
      // Branches from bb to succ_bb don't affect our decision.
      continue;
    const TerminatorInst* pred_bb_term = pred_bb->getTerminator();
    const BranchInst* pred_bb_br = dyn_cast<BranchInst>(pred_bb_term);
    if (pred_bb_br == NULL)
      // Instructions other than branches don't affect our decision.
      continue;
    if (pred_bb_br->isConditional())
      // Conditional branch instructions don't affect our decision.
      continue;
    return true;   // A basic block other than bb also unconditionally branches to succ_bb --> branch is mandatory.
  }
  return false;   // Branch is not mandatory.
}

// Destructively remove all instances of a given character from a string.
void remove_all_instances(string& some_string, char some_char)
{
  some_string.erase(remove(some_string.begin(), some_string.end(), some_char),
                    some_string.end());
}

// Parse a list of function names into a set.  The trick is that (1)
// demangled C++ function names are split (at commas) across list
// elements and need to be recombined, and (2) the form "@filename"
// causes function names to be read from a file.
set<string>* parse_function_names(vector<string>& funclist)
{
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

// Mark an instruction as having been produced by Byfl.
void BytesFlops::mark_as_byfl(Instruction* inst)
{
  LLVMContext& ctx = inst->getContext();
  MDNode* meta = MDNode::get(ctx, MDString::get(ctx, "inserted by Byfl"));
  inst->setMetadata("byfl", meta);
}

// Insert after a given instruction some code to increment a global
// variable.
void BytesFlops::increment_global_variable(BasicBlock::iterator& insert_before,
                                           Constant* global_var,
                                           Value* increment)
{
  // %0 = load i64* @<global_var>, align 8
  LoadInst* load_var = new LoadInst(global_var, "gvar", false, insert_before);
  mark_as_byfl(load_var);

  // %1 = add i64 %0, <increment>
  BinaryOperator* inc_var =
    BinaryOperator::Create(Instruction::Add, load_var, increment,
                           "new_gvar", insert_before);
  mark_as_byfl(inc_var);

  // store i64 %1, i64* @<global_var>, align 8
  mark_as_byfl(new StoreInst(inc_var, global_var, false, insert_before));
}

// Insert before a given instruction some code to increment an element of a
// global array (really, a pointer to a vector).
void BytesFlops::increment_global_array(BasicBlock::iterator& insert_before,
                                        Constant* global_var,
                                        Value* idx,
                                        Value* increment)
{
  // %1 = load i64** @<global_var>, align 8
  LoadInst* load_array = new LoadInst(global_var, "garray", false, 8, insert_before);
  mark_as_byfl(load_array);

  // %2 = getelementptr inbounds i64* %1, i64 %idx
  GetElementPtrInst* idx_ptr = GetElementPtrInst::Create(nullptr, load_array, idx, "idx_ptr", insert_before);
  mark_as_byfl(idx_ptr);

  // %3 = load i64* %2, align 8
  LoadInst* idx_val = new LoadInst(idx_ptr, "idx_val", false, 8, insert_before);
  mark_as_byfl(idx_val);

  // %4 = add i64 %3, <increment>
  BinaryOperator* inc_elt =
    BinaryOperator::Create(Instruction::Add, idx_val, increment, "new_val", insert_before);
  mark_as_byfl(inc_elt);

  // store i64 %4, i64* %2, align 8
  StoreInst* store_inst = new StoreInst(inc_elt, idx_ptr, false, 8, insert_before);
  mark_as_byfl(store_inst);
}

// Insert before a given instruction some code to increment an element of a
// global 4-D array.
void BytesFlops::increment_global_4D_array(BasicBlock::iterator& insert_before,
                                           GlobalVariable* array4d_var,
                                           Value* idx1,
                                           Value* idx2,
                                           Value* idx3,
                                           Value* idx4,
                                           Value* increment)
{
  // %1 = getelementptr inbounds [<D1> x [<D1> x [<D3> x [<D4> x i64]]]]* @<global_var>, i64 0, i64 <idx1>, i64 <idx2>, i64 <idx3>, i64 <idx4>
  std::vector<Value*> gep_indices;
  gep_indices.push_back(zero);
  gep_indices.push_back(idx1);
  gep_indices.push_back(idx2);
  gep_indices.push_back(idx3);
  gep_indices.push_back(idx4);
  GetElementPtrInst* gep_inst =
    GetElementPtrInst::Create(nullptr, array4d_var, gep_indices, "idx4_ptr", insert_before);
  mark_as_byfl(gep_inst);

  // %2 = load i64* %1, align 8
  LoadInst* load_inst = new LoadInst(gep_inst, "idx4_val", false, 8, insert_before);
  mark_as_byfl(load_inst);

  // %3 = add i64 %2, <increment>
  BinaryOperator* add_inst =
    BinaryOperator::Create(Instruction::Add, load_inst, increment, "new_val", insert_before);
  mark_as_byfl(add_inst);

  // store i64 %3, i64* %1, align 8
  StoreInst* store_inst = new StoreInst(add_inst, gep_inst, false, 8, insert_before);
  mark_as_byfl(store_inst);
}

// Mark a variable as "used" (not eligible for dead-code elimination).
void mark_as_used(Module& module, Constant* protected_var)
{
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

void BytesFlops::mark_as_used(Module& module, Constant* protected_var)
{
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

// Create and initialize a global variable in the instrumented code.
GlobalVariable* BytesFlops::create_global_variable(Module& module,
                                                   Type* var_type,
                                                   Constant * init_value,
                                                   const char* name)
{
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
                                                   uint64_t value,
                                                   bool reuse_old)
{
  // Return the existing constant if any.
  if (reuse_old) {
    GlobalVariable* old_var = module.getGlobalVariable(name);
    if (old_var != nullptr)
      return old_var;
  }

  // Create a new constant.
  LLVMContext& globctx = module.getContext();
  IntegerType* i64type = Type::getInt64Ty(globctx);
  ConstantInt* const_value = ConstantInt::get(globctx, APInt(64, value));
  GlobalVariable* new_constant =
    new GlobalVariable(module, i64type, true, GlobalValue::LinkOnceODRLinkage,
                       const_value, name);
  mark_as_used(module, new_constant);
  return new_constant;
}

// Create and initialize a global bool constant in the instrumented code.
GlobalVariable* BytesFlops::create_global_constant(Module& module,
                                                   const char* name,
                                                   bool value,
                                                   bool reuse_old)
{
  // Return the existing constant if any.
  if (reuse_old) {
    GlobalVariable* old_var = module.getGlobalVariable(name);
    if (old_var != nullptr)
      return old_var;
  }

  // Create a new constant.
  LLVMContext& globctx = module.getContext();
  IntegerType* booltype = Type::getInt1Ty(globctx);
  ConstantInt* const_value = ConstantInt::get(globctx, APInt(1, value));
  GlobalVariable* new_constant =
    new GlobalVariable(module, booltype, true, GlobalValue::LinkOnceODRLinkage,
                       const_value, name);
  mark_as_used(module, new_constant);
  return new_constant;
}

// Create and initialize a global char* constant in the instrumented code.
Constant* BytesFlops::create_global_constant(Module& module,
                                             const char* name,
                                             const char* value,
                                             bool reuse_old)
{
  // Return the existing constant if any.
  if (reuse_old) {
    GlobalVariable* old_var = module.getGlobalVariable(name);
    if (old_var != nullptr && old_var->hasInitializer())
      return old_var->getInitializer();
  }

  // Create a new constant.
  // First, create a _local_ array of characters.
  if (reuse_old) {
    Constant* old_value = module.getNamedValue(name);
    if (old_value != nullptr)
      return old_value;
  }
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
    ConstantExpr::getGetElementPtr(nullptr, string_contents, getelementptr_indexes);
  PointerType* pointer_type = PointerType::get(IntegerType::get(globctx, 8), 0);
  GlobalVariable* new_constant =
    new GlobalVariable(module, pointer_type, true,
                       GlobalValue::LinkOnceODRLinkage, array_pointer, name);
  mark_as_used(module, new_constant);
  return new_constant;
}

// Return the number of elements in a given vector.
ConstantInt* BytesFlops::get_vector_length(LLVMContext& bbctx, const Type* dataType, ConstantInt* scalarValue)
{
  if (dataType->isVectorTy()) {
    unsigned int num_elts = dyn_cast<VectorType>(dataType)->getNumElements();
    return ConstantInt::get(bbctx, APInt(64, num_elts));
  }
  else
    return scalarValue;
}

// Return true if and only if the given instruction should be treated as a
// do-nothing operation.
bool BytesFlops::is_no_op(const Instruction& inst)
{
  // Reject certain instructions that we expect to turn into no-ops during
  // code generation.
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
                                 const Type* instType)
{
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

// Return the total number of bits consumed and produced by a given
// instruction.  The result is a bit unintuitive for certain types of
// instructions so use with caution.
uint64_t BytesFlops::instruction_operand_bits(const Instruction& inst)
{
  uint64_t total_bits = inst.getType()->getPrimitiveSizeInBits();
  for (User::const_op_iterator iter = inst.op_begin(); iter != inst.op_end(); iter++) {
    if (dyn_cast<Constant>(*iter) != nullptr)
      continue;   // We want constants to count as zero operand bits.
    Value* val = *iter;
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
Function* BytesFlops::declare_thunk(Module* module, const char* thunk_name)
{
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
Constant* BytesFlops::map_func_name_to_arg (Module* module, StringRef funcname)
{
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
    ConstantExpr::getGetElementPtr(nullptr, const_char_ptr, getelementptr_indices);
  func_name_to_arg[funcname] = string_argument;
  return string_argument;
}

// Declare an external variable.
GlobalVariable* BytesFlops::declare_global_var(Module& module,
                                               Type* var_type,
                                               StringRef var_name,
                                               bool is_const)
{
  // Don't declare the same variable twice in a single module.
  GlobalVariable* oldvar = module.getGlobalVariable(var_name);
  if (oldvar)
    return oldvar;
  else
    return new GlobalVariable(module, var_type, is_const,
                              GlobalVariable::ExternalLinkage, 0,
                              var_name, 0, GlobalVariable::NotThreadLocal);
}

// Insert code to set every element of a given array to zero.
void BytesFlops::insert_zero_array_code(Module* module,
                                        GlobalVariable* array_to_zero,
                                        uint64_t num_elts,
                                        BasicBlock::iterator& insert_before)
{
  LoadInst* array_addr = new LoadInst(array_to_zero, "ar", false, 8, insert_before);
  mark_as_byfl(array_addr);
  LLVMContext& globctx = module->getContext();
  CastInst* array_addr_cast =
    new BitCastInst(array_addr,
                    PointerType::get(IntegerType::get(globctx, 8), 0),
                    "arv", insert_before);
  mark_as_byfl(array_addr_cast);
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
                                     uint64_t num_insts, int& must_clear,
                                     BasicBlock::iterator& insert_before)
{
  // Keep track of how the basic block terminated.
  Instruction& inst = *insert_before;
  unsigned int opcode = inst.getOpcode();   // Terminator instruction's opcode
  LLVMContext& globctx = module->getContext();
  int bb_end_type;
  switch (opcode) {
    case Instruction::IndirectBr:
      bb_end_type = BF_END_BB_INDIRECT;
      increment_global_array(insert_before, terminator_var,
                             ConstantInt::get(globctx, APInt(64, bb_end_type)),
                             one);
      break;

    case Instruction::Switch:
      bb_end_type = BF_END_BB_SWITCH;
      increment_global_array(insert_before, terminator_var,
                             ConstantInt::get(globctx, APInt(64, bb_end_type)),
                             one);
      break;

    case Instruction::Ret:
      bb_end_type = BF_END_BB_RETURN;
      increment_global_array(insert_before, terminator_var,
                             ConstantInt::get(globctx, APInt(64, bb_end_type)),
                             one);
      break;

    case Instruction::Invoke:
      bb_end_type = BF_END_BB_INVOKE;
      increment_global_array(insert_before, terminator_var,
                             ConstantInt::get(globctx, APInt(64, bb_end_type)),
                             one);
      instrument_invoke(module, &inst, insert_before, must_clear);
      break;

    case Instruction::Br:
      {
        BranchInst* br_inst = dyn_cast<BranchInst>(&inst);
        if (br_inst->isConditional()) {
          // Conditional branch -- dynamically choose to increment
          // either "not taken" or "taken".
          static_cond_brs++;
          SelectInst* array_offset =
            SelectInst::Create(br_inst->getCondition(),
                               ConstantInt::get(globctx, APInt(64, BF_END_BB_COND_NT)),
                               ConstantInt::get(globctx, APInt(64, BF_END_BB_COND_T)),
                               "bf_cond",
                               insert_before);
          mark_as_byfl(array_offset);
          increment_global_array(insert_before, terminator_var, array_offset, one);
        }
        else {
          // Unconditional branch -- statically choose to increment
          // either "mandatory" or "removable".
          bb_end_type = uncond_branch_is_mandatory(br_inst) ? BF_END_BB_UNCOND_REAL : BF_END_BB_UNCOND_FAKE;
          increment_global_array(insert_before, terminator_var,
                                 ConstantInt::get(globctx, APInt(64, bb_end_type)),
                                 one);
        }
      }
      break;

    default:
      break;
  }
  increment_global_array(insert_before, terminator_var,
                         ConstantInt::get(globctx, APInt(64, BF_END_BB_ANY)),
                         one);

  // If we're instrumenting every basic block, insert calls to
  // bf_tally_bb_execution(), bf_accumulate_bb_tallies(), and
  // bf_report_bb_tallies().
  if (InstrumentEveryBB) {
    static MersenneTwister bb_rng(module->getModuleIdentifier());
    vector<Value*> arg_list;
    uint64_t randnum = uint64_t(bb_rng.next());
    func_syminfo =
      find_value_provenance(*module, &inst, inst_to_string(&inst), insert_before, func_syminfo);
    arg_list.push_back(func_syminfo);
    arg_list.push_back(ConstantInt::get(globctx, APInt(64, randnum)));
    arg_list.push_back(ConstantInt::get(globctx, APInt(64, num_insts)));
    callinst_create(tally_bb_exec, arg_list, insert_before);
    callinst_create(accum_bb_tallies, insert_before);
    arg_list.resize(1);    // Retain only the bf_symbol_info_t* argument.
    callinst_create(report_bb_tallies, arg_list, insert_before);
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
      mark_as_byfl(new StoreInst(zero, load_var, false, insert_before));
      mark_as_byfl(new StoreInst(zero, load_inst_var, false, insert_before));
    }
    if (must_clear & CLEAR_STORES) {
      mark_as_byfl(new StoreInst(zero, store_var, false, insert_before));
      mark_as_byfl(new StoreInst(zero, store_inst_var, false, insert_before));
    }
    if (must_clear & CLEAR_FLOPS)
      mark_as_byfl(new StoreInst(zero, flop_var, false, insert_before));
    if (must_clear & CLEAR_FP_BITS)
      mark_as_byfl(new StoreInst(zero, fp_bits_var, false, insert_before));
    if (must_clear & CLEAR_OPS)
      mark_as_byfl(new StoreInst(zero, op_var, false, insert_before));
    if (must_clear & CLEAR_OP_BITS)
      mark_as_byfl(new StoreInst(zero, op_bits_var, false, insert_before));
    if (must_clear & CLEAR_CALLS)
      mark_as_byfl(new StoreInst(zero, call_inst_var, false, insert_before));
    if (must_clear & CLEAR_MEM_TYPES) {
      // Zero out the entire array.
      LoadInst* mem_insts_addr = new LoadInst(mem_insts_var, "mi", false, 8, insert_before);
      mark_as_byfl(mem_insts_addr);
      LLVMContext& globctx = module->getContext();
      CastInst* mem_insts_cast =
        new BitCastInst(mem_insts_addr,
                        PointerType::get(IntegerType::get(globctx, 8), 0),
                        "miv", insert_before);
      mark_as_byfl(mem_insts_cast);
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
      LoadInst* tally_insts_addr = new LoadInst(inst_mix_histo_var, "ti", false, 8, insert_before);
      mark_as_byfl(tally_insts_addr);
      LLVMContext& globctx = module->getContext();
      CastInst* tally_insts_cast =
        new BitCastInst(tally_insts_addr,
                        PointerType::get(IntegerType::get(globctx, 8), 0),
                        "miv", insert_before);
      mark_as_byfl(tally_insts_cast);
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

  // If we're instrumenting by call stack, insert a call to bf_pop_function()
  // at every return from the function.
  if (TrackCallStack && insert_before->getOpcode() == Instruction::Ret)
    callinst_create(pop_function, insert_before);
}

// Wrap CallInst::Create() with a more convenient interface.
void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
                                 Instruction* insert_before)
{
  CallInst* cinst = CallInst::Create(function, args, "", insert_before);
  cinst->setCallingConv(CallingConv::C);
  mark_as_byfl(cinst);
}

// Ditto the above but with a BasicBlock insertion point.
void BytesFlops::callinst_create(Value* function, ArrayRef<Value*> args,
                                 BasicBlock* insert_before)
{
  CallInst* cinst = CallInst::Create(function, args, "", insert_before);
  cinst->setCallingConv(CallingConv::C);
  mark_as_byfl(cinst);
}

// Ditto the above but for parameterless functions.
void BytesFlops::callinst_create(Value* function, BasicBlock* insert_before)
{
  CallInst* cinst = CallInst::Create(function, "", insert_before);
  cinst->setCallingConv(CallingConv::C);
  mark_as_byfl(cinst);
}

// Ditto the above but with an Instruction insertion point.
void BytesFlops::callinst_create(Value* function, Instruction* insert_before)
{
  CallInst* cinst = CallInst::Create(function, "", insert_before);
  cinst->setCallingConv(CallingConv::C);
  mark_as_byfl(cinst);
}

// Given a Call instruction, return true if we can safely ignore it.
bool BytesFlops::ignorable_call (const Instruction* inst)
{
  // Ignore debug intrinsics (llvm.dbg.*).
  if (isa<DbgInfoIntrinsic>(*inst))
    return true;

  // Ignore lifetime intrinsics (llvm.lifetime.*) and
  // bf_assoc_addresses_with_dstruct, which would have been inserted into the
  // current basic block when processing a predecessor basic block.
  if (isa<CallInst>(inst)) {
    Function* func = dyn_cast<CallInst>(inst)->getCalledFunction();
    if (func) {
      StringRef callee_name = func->getName();
      if (callee_name.startswith("llvm.lifetime"))
        return true;
      if (callee_name.startswith("bf_assoc_addresses_with_dstruct"))
        return true;
    }
  }

  // Pay attention to everything else.
  return false;
}

// Count the number of "real" instructions in a basic block.
size_t BytesFlops::bb_size(const BasicBlock& bb)
{
  size_t tally = 0;
  BasicBlock::const_iterator inst;
  for (inst = bb.getFirstInsertionPt(); inst != bb.end(); inst++)
    tally++;
  return tally;
}

// Construct a mapping from Instruction* to string for every instruction in the
// function by converting the function to a single string then splitting it up
// into instructions.  This is a horrible, kludgy workaround for the fact that
// Value::print() is abominably slow in the current version of LLVM.
void BytesFlops::map_instructions_to_strings (Function& function)
{
  string func_string;                      // Function as a string
  raw_string_ostream rso(func_string);
  function.print(rso);
  stringstream func_stream(func_string);   // Function string as a stream
  string oneline;                          // One line of the string
  vector<string> inst_strings;             // List of instruction strings
  const string whitespace(" \t");          // Intra-line whitespace characters
  while (getline(func_stream, oneline, '\n')) {
    // Determine if we're looking at an instruction.
    if (oneline.substr(0, 2) != "  ")
      continue;    // Not an instruction

    // Trim leading and trailing whitespace.
    size_t text_begin = oneline.find_first_not_of(whitespace);
    if (text_begin != string::npos) {
      size_t text_end = oneline.find_last_not_of(whitespace);
      oneline = oneline.substr(text_begin, text_end - text_begin + 1);
    }

    // Store the resulting string.
    if (text_begin > 2 || oneline[0] == ']') {
      // Continuation of previous instruction
      if (inst_strings.size() > 0)
        inst_strings.back() += string(" ") + oneline;
      else
        report_fatal_error(Twine("Unexpected instruction continuation: ") + oneline);
    }
    else
      // Ordinary instruction
      inst_strings.push_back(oneline);
  }
  unsigned int inum = 0;
  for (Function::iterator func_iter = function.begin();
       func_iter != function.end();
       func_iter++) {
    BasicBlock& bb = *func_iter;
    for (BasicBlock::iterator bb_iter = bb.begin(); bb_iter != bb.end(); bb_iter++)
      // Associate each instruction with a string.
      instruction_to_string[&*bb_iter] = inst_strings[inum++];
  }
}

// Convert an LLVM Instruction* to an STL string.  Ideally, this
// should be coded like the following:
//
//     string inst_str;
//     raw_string_ostream rso(inst_str);
//     inst->print(rso);
//
// plus whitespace trimming.  Unfortunately, in the current version of LLVM
// (ca. July 2015), the preceding code is unacceptably slow -- I've seen
// upwards of 1 second per call, depending on function length.  Hence, we use
// map_instructions_to_strings() above as a fast but perhaps fragile workaround
// to construct an Instruction* to string mapping, which we use in
// inst_to_string().
string BytesFlops::inst_to_string(Instruction* inst)
{
  unordered_map<Instruction*, string>::iterator iter = instruction_to_string.find(inst);
  if (iter == instruction_to_string.end()) {
    string errstr;
    raw_string_ostream rso(errstr);
    rso << "Failed to stringify instruction: ";
    inst->print(rso);
    report_fatal_error(errstr);
  }
  return iter->second;
}

// Stack-allocate a bf_symbol_info_t in the generated code based on a
// given InternalSymbolInfo.
AllocaInst* BytesFlops::find_value_provenance(Module& module,
                                              InternalSymbolInfo& syminfo,
                                              Instruction* insert_before,
                                              AllocaInst* syminfo_struct)
{
  // Stack-allocate a bf_symbol_info_t in the generated code.
  LLVMContext& globctx = module.getContext();
  if (syminfo_struct == nullptr) {
    syminfo_struct = new AllocaInst(syminfo_type, "syminfo_struct", insert_before);
    syminfo_struct->setAlignment(8);
    mark_as_byfl(syminfo_struct);
  }

  // Prepare to create a number of instructions.
  vector<Value*> syminfo_indices;
  syminfo_indices.push_back(zero);
  syminfo_indices.push_back(zero);
  GetElementPtrInst* syminfo_gep;
  LoadInst* syminfo_load;
  StoreInst* syminfo_store;
  int32_t field = 0;

  // Assign bf_symbol_info_t.ID.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.ID", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_store =
    new StoreInst(ConstantInt::get(globctx, APInt(64, syminfo.ID)),
                  syminfo_gep, false, 8, insert_before);
  mark_as_byfl(syminfo_store);

  // Assign bf_symbol_info_t.origin.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.origin", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_load =
    new LoadInst(create_global_constant(module, "bf_syminfo.origin", syminfo.origin.c_str()),
                 "deref_str", false, 8, insert_before);
  mark_as_byfl(syminfo_load);
  syminfo_store =
    new StoreInst(syminfo_load, syminfo_gep, false, 8, insert_before);
  mark_as_byfl(syminfo_store);

  // Assign bf_symbol_info_t.symbol.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.symbol", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_load =
    new LoadInst(create_global_constant(module, "bf_syminfo.symbol", syminfo.symbol.c_str()),
                 "deref_str", false, 8, insert_before);
  mark_as_byfl(syminfo_load);
  syminfo_store =
    new StoreInst(syminfo_load, syminfo_gep, false, 8, insert_before);
  mark_as_byfl(syminfo_store);

  // Assign bf_symbol_info_t.function.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.function", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_load =
    new LoadInst(create_global_constant(module, "bf_syminfo.function", syminfo.function.c_str()),
                 "deref_str", false, 8, insert_before);
  mark_as_byfl(syminfo_load);
  syminfo_store =
    new StoreInst(syminfo_load, syminfo_gep, false, 8, insert_before);
  mark_as_byfl(syminfo_store);

  // Assign bf_symbol_info_t.file.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.file", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_load =
    new LoadInst(create_global_constant(module, "bf_syminfo.file", syminfo.file.c_str()),
                 "deref_str", false, 8, insert_before);
  mark_as_byfl(syminfo_load);
  syminfo_store =
    new StoreInst(syminfo_load, syminfo_gep, false, 8, insert_before);
  mark_as_byfl(syminfo_store);

  // Assign bf_symbol_info_t.line.
  syminfo_indices[1] = ConstantInt::get(globctx, APInt(32, field++));
  syminfo_gep = GetElementPtrInst::Create(nullptr, syminfo_struct, syminfo_indices, "syminfo.line", insert_before);
  mark_as_byfl(syminfo_gep);
  syminfo_store =
    new StoreInst(ConstantInt::get(globctx, APInt(32, syminfo.line)),
                  syminfo_gep, false, 4, insert_before);
  mark_as_byfl(syminfo_store);

  // Return a pointer to the initialized bf_symbol_info_t Value.
  return syminfo_struct;
}


// Read the metadata associated with a value and generate code to construct a
// bf_symbol_info_t representing where the value came from.
AllocaInst* BytesFlops::find_value_provenance(Module& module,
                                              Value* value,
                                              string defn_loc,
                                              BasicBlock::iterator& insert_before,
                                              AllocaInst* syminfo_struct)
{
  InternalSymbolInfo syminfo(value, defn_loc);
  return find_value_provenance(module, syminfo, &*insert_before, syminfo_struct);
}

// Read the metadata associated with an instruction and generate code to
// construct a bf_symbol_info_t representing where the value came from.  If the
// instruction contains imprecise location information, try previous
// instructions until precise information is found.
AllocaInst* BytesFlops::find_value_provenance(Module& module,
                                              BasicBlock::iterator& inst_iter,
                                              string defn_loc,
                                              BasicBlock::iterator& insert_before,
                                              AllocaInst* syminfo_struct)
{
  Instruction& inst = *inst_iter;
  BasicBlock* bb = inst.getParent();
  BasicBlock::iterator bb_begin = bb->begin();

  // Walk backwards until we find precise location information.
  for (; inst_iter != bb_begin; inst_iter--) {
    InternalSymbolInfo syminfo(&*inst_iter, defn_loc);
    if (syminfo.precise)
      return find_value_provenance(module, syminfo, &*insert_before, syminfo_struct);
  }

  // Failing to find precise information, use the original instruction's
  // imprecise information.
  InternalSymbolInfo syminfo(&*inst_iter, defn_loc);
  return find_value_provenance(module, syminfo, &*insert_before, syminfo_struct);
}

} // namespace bytesflops_pass

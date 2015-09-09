/*
 * Acquire information about a symbol from the debug metadata
 *
 * By Scott Pakin <pakin@lanl.gov>
 */

#include "bytesflops.h"
#include <iostream>

namespace bytesflops_pass {

// Concatenate a directory name and a file name into a full path.
static string full_file_path(StringRef dirname, StringRef filename)
{
  if (dirname == "")
    return filename == "" ? "??" : filename.str();
  else {
    string dname(dirname.str());
    string fname(filename.str());
    string filespec;
    if (dname.back() == '/')
      filespec = dname + fname;
    else
      filespec = dname + string("/") + fname;
    return string(absolute_file_name(filespec.c_str()));
  }
}

// A null func2loc implies a need for initialization.
unordered_map<const Function*, InternalSymbolInfo::string_uint_pair>* InternalSymbolInfo::func2loc = nullptr;

// A null prng implies a need for initialization.
MersenneTwister* InternalSymbolInfo::prng = nullptr;

// Populate func2loc with every function in the module.
void InternalSymbolInfo::initialize_func2loc(const Module* module)
{
  func2loc = new unordered_map<const Function*, string_uint_pair>;
  DebugInfoFinder finder;
  finder.processModule(*module);
  for (const DISubprogram subprog : finder.subprograms()) {
    const Function* func = subprog.getFunction();
    if (func == nullptr)
      continue;
    string file = full_file_path(subprog.getDirectory(), subprog.getFilename());
    unsigned int line = subprog.getLineNumber();
    string_uint_pair file_line(file, line);
    pair<const Function*, string_uint_pair> func_file_line(func, file_line);
    func2loc->insert(func_file_line);
  }
}

// Indicate whether all fields have been assigned a non-default value.
bool InternalSymbolInfo::have_all_fields() {
  return symbol != "*UNNAMED*" && function != "??" && file != "??" && line != 0;
}

// Construct an InternalSymbolInfo from a Value.  The underlying process is
// based on that presented by
// http://stackoverflow.com/questions/21410675/getting-the-original-variable-name-for-an-llvm-value
// and, to a lesser extent, http://llvm.org/docs/SourceLevelDebugging.html.
InternalSymbolInfo::InternalSymbolInfo(Value* value, string defn_loc)
{
  // Initialize our fields with placeholder values.
  if (prng == nullptr)
    prng = new MersenneTwister("InternalSymbolInfo Value");  // Arbitrary salt
  ID = prng->next();
  origin = defn_loc;
  symbol = "*UNNAMED*";
  function = "??";
  file = "??";
  line = 0;
  precise = false;

  // Find the function containing the value.  Return empty-handed if we can't
  // find it (hopefully an extremely rare situation).
  const Function* parent_func = nullptr;
  if (const Argument* arg = dyn_cast<Argument>(value))
    parent_func = arg->getParent();
  else
    if (const Instruction* inst = dyn_cast<Instruction>(value))
      parent_func = inst->getParent()->getParent();
  if (parent_func == nullptr)
    return;
  if (func2loc == nullptr)
    initialize_func2loc(parent_func->getParent());

  // Find where the value came from.
  MDNode* var_node = nullptr;
  for (Function::const_iterator func_iter = parent_func->begin();
       func_iter != parent_func->end();
       func_iter++) {
    const BasicBlock& bb = *func_iter;
    for (BasicBlock::const_iterator iter = bb.begin();
         iter != bb.end();
         iter++) {
      const Instruction* inst = &*iter;
      if (const DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(inst)) {
        if (DbgDeclare->getAddress() == value) {
          var_node = DbgDeclare->getVariable();
          break;
        }
      }
      else if (const DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(inst)) {
        if (DbgValue->getValue() == value) {
          var_node = DbgValue->getVariable();
          break;
        }
      }
    }
  }

  // Populate our fields.
  // Attempt 1: Use the value's debug information if that exists.
  if (parent_func->hasName())
    function = parent_func->getName();
  if (var_node != nullptr) {
    // We found the value's debug information. Fill in all the information we
    // have.
    DIVariable var_var(var_node);
    DIFile var_file = var_var.getFile();
    symbol = var_var.getName();
    file = full_file_path(var_file.getDirectory(), var_file.getFilename());
    line = var_var.getLineNumber();
    precise = have_all_fields();
    if (precise)
      return;
  }

  // Attempt 2: Use the symbol's internally generated name instead of the
  // user-assigned name.
  if (symbol == "*UNNAMED*" && value->hasName())
    symbol = value->getName();

  // Attempt 3: If we were given an instruction, use the opcode name, in
  // brackets, as the symbol name; try to get the file name and line number
  // from the instruction's debug location.
  Instruction* inst = dyn_cast<Instruction>(value);
  if (inst != nullptr) {
    // Use the opcode name as the symbol name.
    if (symbol == "*UNNAMED*")
      symbol = string("[") + string(inst->getOpcodeName()) + string("]");

    // Get the file and line number from the instruction's DebugLoc.
    if (file == "??") {
      const DebugLoc& dbloc = inst->getDebugLoc();
      MDNode* scope_node = dbloc.getScope(inst->getContext());
      if (scope_node != nullptr) {
        DIScope scope(scope_node);
        file = full_file_path(scope.getDirectory(), scope.getFilename());
        line = dbloc.getLine();
        precise = have_all_fields();
        if (precise)
          return;
      }
    }
  }

  // Attempt 4: Try to get the file name and line number from the surrounding
  // function.
  if (file == "??") {
    auto fiter = func2loc->find(parent_func);
    if (fiter != func2loc->end()) {
      string_uint_pair& file_line = fiter->second;
      file = file_line.first;
      line = file_line.second;
    }
  }
}

// Construct an InternalSymbolInfo from a DIGlobalVariable..
InternalSymbolInfo::InternalSymbolInfo(DIGlobalVariable& var, string defn_loc) {
  if (prng == nullptr)
    prng = new MersenneTwister("InternalSymbolInfo Global");  // Arbitrary salt
  ID = prng->next();
  origin = defn_loc;
  symbol = var.getName().str();
  function = "*GLOBAL*";
  file = full_file_path(var.getDirectory(), var.getFilename());
  line = var.getLineNumber();
}

// Construct an InternalSymbolInfo from a Function.
InternalSymbolInfo::InternalSymbolInfo(Function* funcptr)
{
  // Initialize our fields with placeholder values.
  if (prng == nullptr)
    prng = new MersenneTwister("InternalSymbolInfo Function");  // Arbitrary salt
  ID = prng->next();
  origin = "text";
  symbol = "*UNNAMED*";
  function = "??";
  file = "??";
  line = 0;
  precise = false;

  // Find the function.  Return empty-handed if we can't find it (hopefully an
  // extremely rare situation).
  if (func2loc == nullptr)
    initialize_func2loc(funcptr->getParent());
  auto fiter = func2loc->find(funcptr);
  if (fiter == func2loc->end())
    return;
  if (funcptr->hasName())
    symbol = function = funcptr->getName();
  string_uint_pair& file_line = fiter->second;
  file = file_line.first;
  line = file_line.second;
}

}

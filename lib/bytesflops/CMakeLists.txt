###################################
# Build the LLVM bytesflops pass  #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# Build and install the Byfl plugin for LLVM/Clang.
byfl_add_llvm_loadable_module(bytesflops
  bytesflops.cpp
  instrument.cpp
  helpers.cpp
  init.cpp
  bytesflops.h
  mersennetwister.cpp
  mersennetwister.h
  functionkeygen.cpp
  functionkeygen.h
  symbolinfo.cpp
  )
llvm_update_compile_flags(bytesflops)
add_link_opts(bytesflops)

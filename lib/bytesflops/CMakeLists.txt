###################################
# Build the LLVM bytesflops pass  #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# Let the user choose where to install the Byfl plugin.  It defaults to
# libexec/byfl as opposed to LLVM's default of lib (or lib64) because a plugin
# is not a library to which an application explicitly links.
set(BYFL_PLUGIN_DIRECTORY ${CMAKE_INSTALL_FULL_LIBEXECDIR}/byfl CACHE PATH
  "Directory in which to install the Byfl LLVM plugin")

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

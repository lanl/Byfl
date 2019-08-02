###################################
# Build the LLVM bytesflops pass  #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# Let the user choose where to install the Byfl plugin.  It defaults to
# libexec/byfl as opposed to LLVM's default of lib (or lib64) because a plugin
# is not a library to which an application explicitly links.
set(CMAKE_INSTALL_BYFL_PLUGIN_DIR ${CMAKE_INSTALL_LIBEXECDIR}/byfl CACHE PATH
  "Directory in which to install the Byfl LLVM plugin")
mark_as_advanced(CMAKE_INSTALL_BYFL_PLUGIN_DIR)

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

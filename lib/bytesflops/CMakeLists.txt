###################################
# Build the LLVM bytesflops pass  #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

add_llvm_loadable_module(bytesflops
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

# Let the user choose where to install the Byfl plugin.  Unfortunately, because
# add_llvm_loadable_module hard-wires the plugin location, we have to resort to
# some trickery to move it elsewhere.  Specifically, we additionally install it
# in BYFL_PLUGIN_DIRECTORY then uninstall the version that
# add_llvm_loadable_module installed.
set(BYFL_PLUGIN_DIRECTORY ${CMAKE_INSTALL_FULL_LIBEXECDIR}/byfl CACHE PATH
  "Directory in which to install the Byfl LLVM plugin")
if(WIN32 OR CYGWIN)
  set(_dlldir "${CMAKE_INSTALL_PREFIX}/bin")
else()
  set(_dlldir "${CMAKE_INSTALL_PREFIX}/lib${LLVM_LIBDIR_SUFFIX}")
endif()
if (NOT (_dlldir STREQUAL BYFL_PLUGIN_DIRECTORY))
  install(
    TARGETS bytesflops
    LIBRARY DESTINATION ${BYFL_PLUGIN_DIRECTORY}
    )
  install(CODE "FILE(REMOVE ${_dlldir}/bytesflops.${CMAKE_SHARED_LIBRARY_SUFFIX})")
endif ()

############################################
# Install wrappers for various compilers   #
# to facilitate compiling them with Byfl   #
#                                          #
# By Scott Pakin <pakin@lanl.gov>          #
############################################

# Install a Perl module for parsing compiler flags.  We replace the Perl site
# prefix with the CMake install prefix.  Doing so installs all Byfl files under
# a single top level but at the cost of the Perl module not winding up in a
# place Perl looks for modules.
perl_get_info(PERL_SITELIBEXP sitelibexp IS_PATH)
perl_get_info(PERL_SITEPREFIX siteprefix IS_PATH)
string(LENGTH ${PERL_SITEPREFIX} _prefix_len)
math(EXPR _prefix_len "${_prefix_len} + 1")
string(SUBSTRING ${PERL_SITELIBEXP} ${_prefix_len} -1 _rel_sitelib)
set(
  PERL_MODULE_DIRECTORY
  ${CMAKE_INSTALL_PREFIX}/${_rel_sitelib}
  CACHE PATH
  "Directory in which to install the Byfl Perl module"
  )
mark_as_advanced(PERL_MODULE_DIRECTORY)
install(
  FILES ParseCompilerOpts.pm
  DESTINATION ${PERL_MODULE_DIRECTORY}
  )

# Generate a Byfl pkg-config file.
configure_file(byfl-clang.pc.in byfl-clang.pc @ONLY)
set(
  PKG_CONFIG_DIRECTORY
  ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig
  CACHE PATH
  "Directory in which to install a Byfl pkg-config file"
  )
mark_as_advanced(PKG_CONFIG_DIRECTORY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/byfl-clang.pc
  DESTINATION ${PKG_CONFIG_DIRECTORY}
  )

# Find a set of C++ libraries on which the Byfl run-time library depends.  The
# wrapper scripts will explicitly link to those even when not invoking a C++
# compiler.
set(link_libs "${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES}")
list(REMOVE_DUPLICATES link_libs)
foreach(lib ${link_libs})
  find_library(_one_cxx_lib NAME ${lib} PATHS ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES} "One library needed by C++")
  list(APPEND _byfl_lib_depends ${_one_cxx_lib})
  unset(_one_cxx_lib CACHE)
endforeach()
set(BYFL_LIB_DEPENDS "${_byfl_lib_depends}" CACHE STRING "List of libraries on which the Byfl run-time library depends")

# Build and install a man page for the Perl module that parses compiler
# options.
add_man_from_pod(ParseCompilerOpts.3pm ParseCompilerOpts.pm)

# Generate the bf-clang wrapper script and associated man page.
configure_file(bf-clang.in bf-clang @ONLY)
add_man_from_pod(bf-clang.1 bf-clang GENERATED)

# Generate and install man pages that are derived from bf-clang.1.
patch_bf_clang_man(clang++ CLANGXX C++ cpp)
patch_bf_clang_man(flang FLANG Fortran f90)

# Generate and install the bf-inst wrapper script and associated man page.
configure_file(bf-inst.in bf-inst @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/bf-inst DESTINATION ${CMAKE_INSTALL_BINDIR})
add_man_from_pod(bf-inst.1 bf-inst GENERATED)

# Create symbolic links from bf-clang++ and bf-flang to bf-clang.
add_custom_target(
  bf-clang++ ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink bf-clang bf-clang++
  )
add_custom_target(
  bf-flang ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink bf-clang bf-flang
  )

# Install all of the non-MPI wrapper scripts.
install(
  PROGRAMS
  ${CMAKE_CURRENT_BINARY_DIR}/bf-clang
  ${CMAKE_CURRENT_BINARY_DIR}/bf-clang++
  ${CMAKE_CURRENT_BINARY_DIR}/bf-flang
  ${CMAKE_CURRENT_BINARY_DIR}/bf-inst
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

# Generate and install of the MPI wrapper scripts.
add_mpi_wrapper_script(mpicc bf-clang OMPI_CC MPICH_CC)
add_mpi_wrapper_script(mpicxx bf-clang++ OMPI_CXX MPICH_CXX)
add_mpi_wrapper_script(mpifort bf-flang OMPI_FC MPICH_F90)
add_mpi_wrapper_script(mpif90 bf-flang OMPI_FC MPICH_F90)
add_mpi_wrapper_script(mpif77 bf-flang OMPI_F77 MPICH_F77)

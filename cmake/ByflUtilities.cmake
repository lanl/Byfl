############################################################################
# This file defines some helper functions needed by the Byfl build process #
############################################################################

# =============================================================================
# Wrap execute_process with actions appropriate to the Byfl build process.
#
# Required keyword arguments: OUTPUT_VARIABLE MESSAGE COMMAND
# =============================================================================
function(set_var_to_process_output)
  # Parse the function options.
  set(oneValueArgs OUTPUT_VARIABLE MESSAGE)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(PARSE_ARGV 0 EXEC "" "${oneValueArgs}" "${multiValueArgs}")

  # Execute the command.
  message(STATUS ${EXEC_MESSAGE})
  execute_process(COMMAND ${EXEC_COMMAND}
    RESULT_VARIABLE _exec_result
    OUTPUT_VARIABLE _exec_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  # Set the output variable only if the command succeeded.
  if (_exec_result EQUAL 0)
    message(STATUS "${EXEC_MESSAGE} - ${_exec_output}")
    set(${EXEC_OUTPUT_VARIABLE} ${_exec_output} PARENT_SCOPE)
  else (_exec_result EQUAL 0)
    message(STATUS "${EXEC_MESSAGE} - failed")
  endif (_exec_result EQUAL 0)
endfunction(set_var_to_process_output)

# =============================================================================
# Determine if the system can handle weak function aliases.  (At the
# time of this writing, OS X cannot.)  If so, define HAVE_WEAK_ALIASES.
# =============================================================================
function(check_weak_aliases)
  # Construct a test file.
  set(_msg "Detecting if weak function aliases are supported")
  message(STATUS ${_msg})
  file(WRITE
    ${CMAKE_BINARY_DIR}/CMakeTmp/testWeakAliases.c
    [=[
int my_function_impl (void)
{
  return 0;
}

int my_function (void) __attribute__((weak, alias("my_function_impl")));

int main (void)
{
  return my_function();
}
]=])

  # See if the test file compiles.
  try_compile(
    _weak_okay
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/CMakeTmp/testWeakAliases.c
    )
  if (_weak_okay)
    message(STATUS "${_msg} - yes")
    set(HAVE_WEAK_ALIASES 1 PARENT_SCOPE)
  else (_weak_okay)
    message(STATUS "${_msg} - no")
  endif (_weak_okay)
endfunction(check_weak_aliases)

# =============================================================================
# Tally the number of LLVM opcodes.  Define NUM_LLVM_OPCODES and
# NUM_LLVM_OPCODES_POW2.  For now, we hard-wire the C++ compiler with the "-E"
# option as the way to preprocess a C++ file.
# =============================================================================
function(tally_llvm_opcodes)
  # Tally the number of LLVM opcodes.
  set_var_to_process_output(
    OUTPUT_VARIABLE _llvm_opcodes
    MESSAGE "Tallying the number of LLVM opcodes"
    COMMAND ${PERL_EXECUTABLE} ${PROJECT_SOURCE_DIR}/lib/byfl/gen_opcode2name "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -E" NUM
    )
  if (NOT _llvm_opcodes)
    message(FATAL_ERROR "Cannot continue without knowing the number of LLVM opcodes.")
  endif (NOT _llvm_opcodes)
  set(NUM_LLVM_OPCODES ${_llvm_opcodes} PARENT_SCOPE)

  # Round the number of LLVM opcodes up to a power of two.
  set_var_to_process_output(
    OUTPUT_VARIABLE _llvm_opcodes_2
    MESSAGE "Rounding up the number of LLVM opcodes to a power of two"
    COMMAND ${PERL_EXECUTABLE} ${PROJECT_SOURCE_DIR}/lib/byfl/gen_opcode2name "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -E" NUM_POW2
    )
  if (NOT _llvm_opcodes_2)
    message(FATAL_ERROR "Cannot continue without knowing the number of LLVM opcodes.")
  endif (NOT _llvm_opcodes_2)
  set(NUM_LLVM_OPCODES_POW2 ${_llvm_opcodes_2} PARENT_SCOPE)
endfunction(tally_llvm_opcodes)

# =============================================================================
# Build a man page from a Perl POD file.  If GENERATED is specified, read the
# POD file from the current binary directory instead of the current source
# directory.
# =============================================================================
function(add_man_from_pod MAN POD)
  # Parse our arguments.
  cmake_parse_arguments(PARSE_ARGV 2 _pod2man GENERATED "" "")
  if (_pod2man_GENERATED)
    set(_pod_dir ${CMAKE_CURRENT_BINARY_DIR})
  else (_pod2man_GENERATED)
    set(_pod_dir ${CMAKE_CURRENT_SOURCE_DIR})
  endif (_pod2man_GENERATED)

  # Run pod2man to convert the POD file to a man page.
  get_filename_component(_ext ${MAN} EXT)
  string(SUBSTRING ${_ext} 1 -1 _section)
  file(RELATIVE_PATH _relfile ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/${MAN})
  get_filename_component(_man_base ${MAN} NAME_WE)
  add_custom_target(
    ${MAN} ALL
    DEPENDS ${_pod_dir}/${POD}
    COMMAND ${POD2MAN_EXECUTABLE} --name="${_man_base}" --section=${_section} --release=${MAN_RELEASE} --center=${MAN_CATEGORY} ${_pod_dir}/${POD} ${MAN}
    VERBATIM
    )

  # Install the man page in the appropriate man-page subdirectory.
  get_filename_component(_man_file ${MAN} NAME)
  string(SUBSTRING ${_section} 0 1 _sec_num)
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_man_file}
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man${_sec_num}
    )
endfunction(add_man_from_pod)

# =============================================================================
# This macro was adapted from add_llvm_loadable_module in AddLLVM.cmake but
# modified to install into BYFL_PLUGIN_DIRECTORY instead of lib or lib64.
# Also, various cases not relevant to Byfl were removed.
# =============================================================================
macro(byfl_add_llvm_loadable_module name)
  llvm_add_library(${name} MODULE ${ARGN})
  install(TARGETS ${name}
    LIBRARY DESTINATION ${BYFL_PLUGIN_DIRECTORY}
    )
  set_target_properties(${name} PROPERTIES FOLDER "Loadable modules")
endmacro(byfl_add_llvm_loadable_module name)

# =============================================================================
# Install a symbolic link pointing to bf-clang.
#
# Stupid hack: At least in CMake 3.3, declaring symlink_bf_clang as an
# ordinary function produces an "Unknown CMake command" error when
# it's called from install(CODE ...).  Surprisingly, declaring it as a
# function from within install(CODE ...) works.
# =============================================================================
install(CODE "
function(symlink_bf_clang NEW)
  message(STATUS \"Installing: ${CMAKE_INSTALL_FULL_BINDIR}/\${NEW}\")
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink bf-clang ${CMAKE_INSTALL_FULL_BINDIR}/\${NEW})
endfunction(symlink_bf_clang NEW)
")

# =============================================================================
# Generate and install a variant of the bf-clang.1 man page that represents a
# different programming language and base compiler.
#
# patch_bf_clang_man takes as arguments the base compiler name (e.g., flang), a
# corresponding environment variable (e.g., FLANG), a language name (e.g.,
# Fortran), and a file extension (e.g., f90).
# =============================================================================
function(patch_bf_clang_man COMP ENV LANG EXT)
  add_custom_target(
    bf-${COMP}.1 ALL
    DEPENDS bf-clang.1
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_BINARY_DIR}/CMakeTmp/patch-bf-clang-man.pl ${COMP} ${ENV} ${LANG} ${EXT} ${CMAKE_CURRENT_BINARY_DIR}/bf-${COMP}.1 ${CMAKE_CURRENT_BINARY_DIR}/bf-clang.1
    VERBATIM
    )
  install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/bf-${COMP}.1
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
    )
endfunction(patch_bf_clang_man COMP ENV LANG EXT)

# Create a helper Perl script for the above that patches bf-clang.1 into a man
# page for another language/compiler.
file(
  WRITE ${CMAKE_BINARY_DIR}/CMakeTmp/patch-bf-clang-man.pl
  [=[
use autodie;
my ($comp, $env, $lang, $ext, $out) = (shift, shift, shift, shift, shift);
open(OUT, ">", $out);
while (<>) {
    s/clang/${comp}/g; s/\bC (compiler|code)/${lang} $1/g; s/myprog\.c/myprog.${ext}/g; s/BF_CLANG/BF_${env}/g;
    print OUT;
}
close OUT;
  ]=]
  )

# =============================================================================
# Generate and install an MPI wrapper script for a Byfl compiler.
# =============================================================================
function(add_mpi_wrapper_script MPI_COMP BYFL_COMP OMPI_ENV MPICH_ENV)
  add_custom_target(
    bf-${MPI_COMP} ALL
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make-bf-mpi ${MPI_COMP} ${BYFL_COMP} ${OMPI_ENV} ${MPICH_ENV} "${CMAKE_INSTALL_FULL_BINDIR}" ${CMAKE_CURRENT_BINARY_DIR}/bf-${MPI_COMP}
    VERBATIM
    )
  install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/bf-${MPI_COMP}
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction(add_mpi_wrapper_script MPI_COMP BYFL_COMP OMPI_ENV MPICH_ENV)

# =============================================================================
# Compile and install a postprocessing tool for Byfl-generated output.
#
# Extra compiler dependencies can be declared with CDEPS, and extra library
# dependencies can be declared with LDEPS.
# =============================================================================
function(add_postprocessing_tool EXE)
  cmake_parse_arguments(PARSE_ARGV 1 _pproc "" "" "CDEPS;LDEPS")
  add_executable(${EXE} ${EXE}.cpp bfbin.h ${_pproc_CDEPS})
  target_link_libraries(${EXE} bfbin ${_pproc_LDEPS})
  add_man_from_pod(${EXE}.1 ${EXE}.pod)
  install(TARGETS ${EXE} DESTINATION ${CMAKE_INSTALL_BINDIR})
endfunction(add_postprocessing_tool EXE)

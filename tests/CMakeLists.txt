###################################
# Test that Byfl actually works   #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# For debugging, have "make check" run CTest in verbose mode.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

# We need Bash and AWK for some of our tests.
include(FindUnixCommands)
find_program(
  AWK_EXECUTABLE
  NAMES awk gawk mawk nawk
  DOC "AWK language interpreter"
  )
mark_as_advanced(AWK_EXECUTABLE)

# Generate a helper script that checks if the Byfl output file looks remotely
# reasonable.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh.in"
  [=[
#!@BASH@

int_ops=`"@CMAKE_BINARY_DIR@/tools/postproc/bfbin2csv" --include=Program --flat-output "$1" | "@AWK_EXECUTABLE@" -F, '$3 ~ /Integer operations/ {print $4}'`
if [ ! -z "$int_ops" ] && [ "$int_ops" -lt 100000 ] ; then
    exit 1
fi
exit 0
]=]
)
configure_file(
  "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
  @ONLY
  )

###########################################################################

# Do the Clang C compiler and linker work at all?
add_test(
  NAME ClangSmokeTest
  COMMAND
  clang -g -o simple-clang-no-opts "${CMAKE_CURRENT_SOURCE_DIR}/simple.c"
  )

# Define some commonly needed paths.
set(bf_clang ${CMAKE_BINARY_DIR}/tools/wrappers/bf-clang)
set(bytesflops_so ${CMAKE_BINARY_DIR}/lib/bytesflops/bytesflops.so)
set(byfl_lib_dir ${CMAKE_BINARY_DIR}/lib/byfl)

# Do the C compiler and linker work when invoked from the Byfl wrapper script?
add_test(
  NAME BfClangNoByfl
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-clang-no-byfl
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  -bf-disable=byfl
  )

# Can the Byfl wrapper script compile, instrument, and link a program?
add_test(
  NAME BfClangNoOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-clang-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  )

# Does the Byfl-instrumented program run without error?
add_test(
  NAME BfClangNoOptsCodeRuns
  COMMAND
  ${CMAKE_COMMAND} -E env
  LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
  ./simple-clang-no-opts
  )
set_property(TEST BfClangNoOptsCodeRuns PROPERTY DEPENDS BfClangNoOptsCompiles)

# Can we postprocess the binary output?  Are the results correct within an
# order of magnitude?
add_test(
  NAME BfClangNoOptsOutputGood
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
  simple-clang-no-opts.byfl
  )
set_property(TEST BfClangNoOptsOutputGood PROPERTY DEPENDS BfClangNoOptsCodeRuns)

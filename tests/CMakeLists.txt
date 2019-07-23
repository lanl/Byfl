###################################
# Test that Byfl actually works   #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

################################ PRELIMINARIES ################################

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

# Define some commonly needed values.
set(bf_clang ${CMAKE_BINARY_DIR}/tools/wrappers/bf-clang)
set(bytesflops_so ${CMAKE_BINARY_DIR}/lib/bytesflops/bytesflops.so)
set(byfl_lib_dir ${CMAKE_BINARY_DIR}/lib/byfl)
set(extra_byfl_options "-bf-unique-bytes;-bf-by-func;-bf-call-stack;-bf-vectors;-bf-every-bb;-bf-reuse-dist;-bf-mem-footprint;-bf-types;-bf-inst-mix;-bf-data-structs;-bf-inst-deps;-bf-strides")

########################### COMPILER TESTS, NO BYFL ###########################

# Do the Clang C compiler and linker work at all?
add_test(
  NAME ClangSmokeTest
  COMMAND
  clang -g -o simple-clang-no-opts "${CMAKE_CURRENT_SOURCE_DIR}/simple.c"
  )

############################ BF-CLANG, NO OPTIONS #############################

# Do the C compiler and linker work when invoked from the Byfl wrapper script?
add_test(
  NAME BfClangNoByfl
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-no-byfl
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  -bf-disable=byfl
  )

# Can the Byfl wrapper script compile, instrument, and link a program?
add_test(
  NAME BfClangNoOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  )

# Does the Byfl-instrumented program run without error?
add_test(
  NAME BfClangNoOptsCodeRuns
  COMMAND
  ${CMAKE_COMMAND} -E env
  LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
  ./simple-bf-clang-no-opts
  )
set_property(TEST BfClangNoOptsCodeRuns PROPERTY DEPENDS BfClangNoOptsCompiles)

# Can we postprocess the binary output of a Byfl program?  Are the results
# correct within an order of magnitude?
add_test(
  NAME BfClangNoOptsOutputGood
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
  simple-bf-clang-no-opts.byfl
  )
set_property(TEST BfClangNoOptsOutputGood PROPERTY DEPENDS BfClangNoOptsCodeRuns)

########################### BF-CLANG, MANY OPTIONS ############################

# Do the C compiler and linker work when invoked from the Byfl wrapper script,
# even when extra Byfl options are specified (and not used)?
add_test(
  NAME BfClangOptsNoByfl
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-opts-no-byfl
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  ${extra_byfl_options}
  -bf-disable=byfl
  )

# Can the Byfl wrapper script compile, instrument, and link a program, even
# when extra Byfl options are specified?
add_test(
  NAME BfClangOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  ${extra_byfl_options}
  )

# Does the Byfl-instrumented program run without error, even when extra Byfl
# options were specified?
add_test(
  NAME BfClangOptsCodeRuns
  COMMAND
  ${CMAKE_COMMAND} -E env
  LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
  ./simple-bf-clang-opts
  )
set_property(TEST BfClangOptsCodeRuns PROPERTY DEPENDS BfClangOptsCompiles)

# Can we postprocess the binary output of a Byfl program?  Are the results
# correct within an order of magnitude?
add_test(
  NAME BfClangOptsOutputGood
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
  simple-bf-clang-opts.byfl
  )
set_property(TEST BfClangOptsOutputGood PROPERTY DEPENDS BfClangOptsCodeRuns)

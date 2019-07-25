###################################
# Test that Byfl actually works   #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

################################ PRELIMINARIES ################################

# For debugging, have "make check" run CTest in verbose mode.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --extra-verbose)

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

set -x
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

# Generate a helper script that runs a command and checks that it generated a
# non-empty output file.
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh.in"
  [=[
#!@BASH@

outfile="$1"
shift
command="$1"
shift
set -x
"@CMAKE_BINARY_DIR@/tools/postproc/$command" "$@"
if [ ! -s "$outfile" ] ; then
    exit 1
fi
exit 0
]=]
)
configure_file(
  "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh.in"
  "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
  @ONLY
  )

# Define some commonly needed values.
set(bf_clang ${CMAKE_BINARY_DIR}/tools/wrappers/bf-clang)
set(bf_clangxx ${CMAKE_BINARY_DIR}/tools/wrappers/bf-clang++)
set(bf_flang ${CMAKE_BINARY_DIR}/tools/wrappers/bf-flang)
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

# Do the Clang C++ compiler and linker work at all?
add_test(
  NAME ClangXXSmokeTest
  COMMAND
  clang++ -g -o simple-clang++-no-opts "${CMAKE_CURRENT_SOURCE_DIR}/simple.cpp"
  )

# Do the Flang Fortan compiler and linker work at all?
if (${FLANG_EXECUTABLE})
  add_test(
    NAME FlangSmokeTest
    COMMAND
    flang -g -o simple-flang-no-opts "${CMAKE_CURRENT_SOURCE_DIR}/simple.f90"
    )
endif (${FLANG_EXECUTABLE})

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

# Can the Byfl wrapper script compile, instrument, and link a C program?
add_test(
  NAME BfClangNoOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  )

# Does the Byfl-instrumented C program run without error?
add_test(
  NAME BfClangNoOptsCodeRuns
  COMMAND
  ${CMAKE_COMMAND} -E env
  LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
  ./simple-bf-clang-no-opts
  )
set_property(TEST BfClangNoOptsCodeRuns PROPERTY DEPENDS BfClangNoOptsCompiles)

# Can we postprocess the binary output of a Byfl C program?  Are the results
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

# Can the Byfl wrapper script compile, instrument, and link a C++ program, even
# when extra Byfl options are specified?
add_test(
  NAME BfClangOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  ${extra_byfl_options}
  )

# Does the Byfl-instrumented C program run without error, even when extra Byfl
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

########################### BF-CLANG++, NO OPTIONS ############################

# Do the C++ compiler and linker work when invoked from the Byfl wrapper script?
add_test(
  NAME BfClangXXNoByfl
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clangxx}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clang++-no-byfl
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.cpp -L${byfl_lib_dir}
  -bf-disable=byfl
  )

# Can the Byfl wrapper script compile, instrument, and link a C++ program?
add_test(
  NAME BfClangXXNoOptsCompiles
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clangxx}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-clangxx-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.cpp -L${byfl_lib_dir}
  )

# Does the Byfl-instrumented C++ program run without error?
add_test(
  NAME BfClangXXNoOptsCodeRuns
  COMMAND
  ${CMAKE_COMMAND} -E env
  LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
  ./simple-bf-clangxx-no-opts
  )
set_property(TEST BfClangXXNoOptsCodeRuns PROPERTY DEPENDS BfClangXXNoOptsCompiles)

# Can we postprocess the binary output of a Byfl C++ program?  Are the results
# correct within an order of magnitude?
add_test(
  NAME BfClangXXNoOptsOutputGood
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
  simple-bf-clangxx-no-opts.byfl
  )
set_property(TEST BfClangXXNoOptsOutputGood PROPERTY DEPENDS BfClangXXNoOptsCodeRuns)

############################ BF-FLANG, NO OPTIONS #############################

# All of the Flang tests are run only if the Flang compiler is present.
if (${FLANG_EXECUTABLE})

  # Do the Fortran compiler and linker work when invoked from the Byfl wrapper
  # script?
  add_test(
    NAME BfFlangNoByfl
    COMMAND
    ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_flang}
    -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-flang-no-byfl
    ${CMAKE_CURRENT_SOURCE_DIR}/simple.f90 -L${byfl_lib_dir}
    -bf-disable=byfl
    )

  # Can the Byfl wrapper script compile, instrument, and link a C program?
  add_test(
    NAME BfFlangNoOptsCompiles
    COMMAND
    ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_flang}
    -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-bf-flang-no-opts
    ${CMAKE_CURRENT_SOURCE_DIR}/simple.f90 -L${byfl_lib_dir}
    )

  # Does the Byfl-instrumented C program run without error?
  add_test(
    NAME BfFlangNoOptsCodeRuns
    COMMAND
    ${CMAKE_COMMAND} -E env
    LD_LIBRARY_PATH="${byfl_lib_dir}:$ENV{LD_LIBRARY_PATH}"
    ./simple-bf-flang-no-opts
    )
  set_property(TEST BfFlangNoOptsCodeRuns PROPERTY DEPENDS BfFlangNoOptsCompiles)

  # Can we postprocess the binary output of a Byfl C program?  Are the results
  # correct within an order of magnitude?
  add_test(
    NAME BfFlangNoOptsOutputGood
    COMMAND
    "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/validate-byfl-output.sh"
    simple-bf-flang-no-opts.byfl
    )
  set_property(TEST BfFlangNoOptsOutputGood PROPERTY DEPENDS BfFlangNoOptsCodeRuns)

endif (${FLANG_EXECUTABLE})

########################## ALL POSTPROCESSING TOOLS ###########################

# Can we postprocess the binary output of a Byfl program using bfbin2cgrind?
add_test(
  NAME Bfbin2cgrindRuns
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
  simple-bf-clang-opts.cgrind
  bfbin2cgrind simple-bf-clang-opts.byfl simple-bf-clang-opts.cgrind
  )
set_property(TEST Bfbin2cgrindRuns PROPERTY DEPENDS BfClangOptsCodeRuns)

# Can we postprocess the binary output of a Byfl program using bfbin2csv?
add_test(
  NAME Bfbin2csvRuns
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
  simple-bf-clang-opts.csv
  bfbin2csv simple-bf-clang-opts.byfl -o simple-bf-clang-opts.csv
  )
set_property(TEST Bfbin2csvRuns PROPERTY DEPENDS BfClangOptsCodeRuns)

# Can we postprocess the binary output of a Byfl program using bfbin2hdf5?
# This test runs conditionally on HDF5 being available.
if (HDF5_FOUND)
  add_test(
    NAME Bfbin2hdf5Runs
    COMMAND
    "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
    simple-bf-clang-opts.h5
    bfbin2hdf5 simple-bf-clang-opts.byfl simple-bf-clang-opts.h5
    )
  set_property(TEST Bfbin2hdf5Runs PROPERTY DEPENDS BfClangOptsCodeRuns)
endif (HDF5_FOUND)

# Can we postprocess the binary output of a Byfl program using bfbin2hpctk?
add_test(
  NAME Bfbin2hpctkRuns
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
  hpctoolkit-simple-bf-clang-opts-database/experiment.xml
  bfbin2hpctk simple-bf-clang-opts.byfl
  )
set_property(TEST Bfbin2hpctkRuns PROPERTY DEPENDS BfClangOptsCodeRuns)

# Can we postprocess the binary output of a Byfl program using bfbin2sqlite3?
# This test runs conditionally on SQLite3 being available.
if (SQLITE3_FOUND)
  add_test(
    NAME Bfbin2sqlite3Runs
    COMMAND
    "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
    simple-bf-clang-opts.db
    bfbin2sqlite3 simple-bf-clang-opts.byfl -o simple-bf-clang-opts.db
    )
  set_property(TEST Bfbin2sqlite3Runs PROPERTY DEPENDS BfClangOptsCodeRuns)
endif (SQLITE3_FOUND)

# Can we postprocess the binary output of a Byfl program using bfbin2xmlss?
add_test(
  NAME Bfbin2xmlssRuns
  COMMAND
  "${BASH}" "${CMAKE_CURRENT_BINARY_DIR}/run-byfl-postproc-tool.sh"
  simple-bf-clang-opts.xml
  bfbin2xmlss simple-bf-clang-opts.byfl simple-bf-clang-opts.xml
  )
set_property(TEST Bfbin2xmlssRuns PROPERTY DEPENDS BfClangOptsCodeRuns)

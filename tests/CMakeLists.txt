###################################
# Test that Byfl actually works   #
#                                 #
# By Scott Pakin <pakin@lanl.gov> #
###################################

# For debugging, have "make check" run CTest in verbose mode.
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --verbose)

# Do the Clang C compiler and linker work at all?
add_test(
  NAME clang
  COMMAND
  clang -g -o simple-clang-no-opts "${CMAKE_CURRENT_SOURCE_DIR}/simple.c"
  )

# Define some commonly needed paths.
set(bf_clang ${CMAKE_BINARY_DIR}/tools/wrappers/bf-clang)
set(bytesflops_so ${CMAKE_BINARY_DIR}/lib/bytesflops/bytesflops.so)
set(byfl_lib_dir ${CMAKE_BINARY_DIR}/lib/byfl)

# Do the C compiler and linker work when invoked from the Byfl wrapper script?
add_test(
  NAME clang-from-bf-clang
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-clang-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  -bf-disable=byfl
  )

# Can the Byfl wrapper script compile, instrument, and link a program?
add_test(
  NAME bf-clang-builds
  COMMAND
  ${PERL_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/tools/wrappers ${bf_clang}
  -bf-plugin=${bytesflops_so} -bf-verbose -g -o simple-clang-no-opts
  ${CMAKE_CURRENT_SOURCE_DIR}/simple.c -L${byfl_lib_dir}
  )

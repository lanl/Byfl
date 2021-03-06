######################################
# Build the libbyfl run-time library #
#                                    #
# By Scott Pakin <pakin@lanl.gov>    #
######################################

# Generate the Byfl run-time library.
add_library(byfl
  basicblocks.cpp
  binaryoutput.cpp
  binaryoutput.h
  byfl.cpp
  byfl.h
  cache-model.cpp
  cachemap.h
  callstack.cpp
  callstack.h
  datastructs.cpp
  opcode2name.cpp
  pagetable.cpp
  pagetable.h
  reuse-dist.cpp
  strides.cpp
  symtable.cpp
  tallybytes.cpp
  threading.cpp
  ubytes.cpp
  vectors.cpp
  )
llvm_update_compile_flags(byfl)
add_link_opts(byfl)

# Specify how to create opcode2name.cpp.
add_custom_command(
  OUTPUT opcode2name.cpp
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_opcode2name "${CMAKE_C_COMPILER} ${CMAKE_C_FLAGS} -E" CPP > opcode2name.cpp
  VERBATIM
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/gen_opcode2name
  COMMENT "Generaring opcode2name.cpp"
  )

# Install the library.
install(
  TARGETS byfl
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

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

# Specify how to create opcode2name.cpp.
add_custom_command(
  OUTPUT opcode2name.cpp
  COMMAND ${PERL_EXECUTABLE} ${PROJECT_SOURCE_DIR}/gen_opcode2name "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_FLAGS} -E" CPP > opcode2name.cpp
  VERBATIM
  MAIN_DEPENDENCY ${PROJECT_SOURCE_DIR}/gen_opcode2name
  COMMENT "Generaring opcode2name.cpp"
  )

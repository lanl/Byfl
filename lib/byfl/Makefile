##===- projects/bytesflops/lib/byfl/Makefile -----------------*- Makefile -*-===##

######################################
# Build the libbyfl run-time library #
# (part of the Bytesflops tool)	     #
#				     #
# By Scott Pakin <pakin@lanl.gov>    #
######################################

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

include $(LEVEL)/Makefile.db

#
# Build a bitcode library.
#
LIBRARYNAME = byfl
BYTECODE_LIBRARY = 1

ifeq ($(ENABLE_BYFL_DB), 1)
BF_DB_CPP = byfl-db.cpp
BF_DB_CPP_FLAGS = -I$(MYSQLCPPCONN_INCLUDEDIR) -fexceptions
else
BF_DB_CPP = byfl-db-stubs.cpp
BF_DB_CPP_FLAGS = 
endif

SOURCES = byfl.cpp $(BF_DB_CPP) byfl-binary.cpp reuse-dist.cpp symtable.cpp threading.cpp ubytes.cpp vectors.cpp tallybytes.cpp

BUILT_SOURCES = opcode2name.cpp opcode2name.h
EXTRA_DIST = cachemap.h opcode2name
CPPFLAGS += -I$(PROJ_SRC_ROOT)/lib/include $(BF_DB_CPP_FLAGS)

#
# Specify how to create opcode2name.cpp and opcode2name.h
#
opcode2name.cpp opcode2name.h: opcode2name
	$(PROJ_SRC_DIR)/opcode2name '$(CPP) -I$(LLVM_SRC_ROOT)/include' opcode2name.cpp opcode2name.h

clean-local::
	$(RM) $(BUILT_SOURCES)

#
# With LLVM 3.1, .bca files are deprecated yet the Makefile doesn't know
# how to build anything else.  Here, we tell it to construct a linked .bc
# file and install that.
#
LibName.BC := $(LibDir)/lib$(LIBRARYNAME).bc
DestBytecodeBlob = $(BytecodeDestDir)/lib$(LIBRARYNAME).bc

install-local:: $(DestBytecodeBlob)

$(DestBytecodeBlob): $(ObjectsBC)
	$(Echo) Installing $(BuildMode) Bytecode File $(DestBytecodeBlob)
	$(Verb) llvm-link -o $(DestBytecodeBlob) $(ObjectsBC)

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

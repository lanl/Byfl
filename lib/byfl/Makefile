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

#
# Build a bitcode library.
#
LIBRARYNAME = byfl
BYTECODE_LIBRARY = 1
SOURCES = byfl.cpp reuse-dist.cpp symtable.cpp threading.cpp ubytes.cpp vectors.cpp
EXTRA_DIST = cachemap.h

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

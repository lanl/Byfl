##===- projects/bytesflops/lib/bytesflops/Makefile -----------------*- Makefile -*-===##

###################################
# Build the LLVM bytesflops pass  #
# (part of the Bytesflops tool)	  #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Build the bytesflops pass as a loadable module.
#
LIBRARYNAME=bytesflops
LOADABLE_MODULE=1
SOURCES = bytesflops.cpp instrument.cpp helpers.cpp init.cpp
EXTRA_DIST = bytesflops.h

CPPFLAGS += -I$(PROJ_SRC_ROOT)/lib/include

EXTRA_DIST += MersenneTwister.h FunctionKeyGen.h
SOURCES += MersenneTwister.cpp FunctionKeyGen.cpp

#
# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common

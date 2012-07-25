##===- projects/bytesflops/Makefile ----------------------------*- Makefile -*-===##
#
# Build the Bytesflops tool
# By Scott Pakin <pakin@lanl.gov>
#
##===----------------------------------------------------------------------===##

#
# Indicates our relative path to the top of the project's root directory.
#
LEVEL = .
PARALLEL_DIRS = lib tools

#
# Include the Master Makefile that knows how to build all.
#
include $(LEVEL)/Makefile.common

#! /bin/sh

########################################
# See if bfbin2hdf5 runs to completion #
#                                      #
# By Scott Pakin <pakin@lanl.gov>      #
########################################

# Define some helper variables.  The ":-" ones will normally be
# provided by the Makefile.
AWK=${AWK:-awk}
PERL=${PERL:-perl}
srcdir=${srcdir:-../../tests}
top_srcdir=${top_srcdir:-../..}
top_builddir=${top_builddir:-..}
clang=${BF_CLANG:-clang}
bf_clang="$top_builddir/tools/wrappers/bf-clang"

# Log everything we do.  Fail on the first error.
set -e
set -x

# Ensure that bfbin2hdf5 runs, exits successfully, and produces a
# non-zero-byte output file.
"$top_builddir/tools/postproc/bfbin2hdf5" simple-clang-many-opts.byfl simple-clang-many-opts.h5
if [ ! -s simple-clang-many-opts.h5 ] ; then
    exit 1
fi

#! /bin/sh

#########################################
# See if bfbin2hpctk runs to completion #
#                                       #
# By Scott Pakin <pakin@lanl.gov>       #
#########################################

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

# Ensure that bfbin2hpctk runs, exits successfully, and produces a
# non-zero-byte output file.
"$top_builddir/tools/postproc/bfbin2hpctk" simple-clang-many-opts.byfl
if [ ! -s hpctoolkit-simple-clang-many-opts-database/experiment.xml ] ; then
    exit 1
fi

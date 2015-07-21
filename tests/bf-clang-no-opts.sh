#! /bin/sh

############################################
# Try compiling a simple program with      #
# bf-clang and no special Byfl options     #
#                                          #
# By Scott Pakin <pakin@lanl.gov>          #
############################################

# Define some helper variables.  The ":-" ones will normally be
# provided by the Makefile.
PERL=${PERL:-perl}
srcdir=${srcdir:-../../tests}
top_srcdir=${top_srcdir:-../..}
top_builddir=${top_builddir:-..}
bf_clang="$top_builddir/tools/wrappers/bf-clang"

set -x
"$PERL" -I"$top_srcdir/tools/wrappers" \
  "$bf_clang" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
              -bf-verbose -g -o simple "$srcdir/simple.c" \
              -L"$top_builddir/lib/byfl/.libs"

#! /bin/sh

########################################
# Try compiling a simple program with  #
# bf-flang and no special Byfl options #
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
flang=${BF_FLANG:-flang}
bf_flang="./bf-flang"
use_flang=${use_flang:-yes}

# Skip this test if we don't have Flang.
if [ "x$use_flang" != xyes ] ; then
    exit 77
fi

# Log everything we do.  Fail on the first error.
set -e
set -x

# Test 1: Do the C compiler and linker work at all?
"$flang" -g -o simple-flang-no-opts "$srcdir/simple.f90"

# Test 2: Do the C compiler and linker work when invoked from the Byfl
# wrapper script?
"$PERL" -I"$top_srcdir/tools/wrappers" \
  "$bf_flang" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
		-bf-verbose -g -o simple-flang-no-opts "$srcdir/simple.f90" \
		-L"$top_builddir/lib/byfl/.libs" -bf-disable=byfl

# Test 3: Can the Byfl wrapper script compile, instrument, and link a program?
"$PERL" -I"$top_srcdir/tools/wrappers" \
  "$bf_flang" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
                -bf-verbose -g -o simple-flang-no-opts "$srcdir/simple.f90" \
                -L"$top_builddir/lib/byfl/.libs"

# Test 4: Does the Byfl-instrumented program run without error?
env LD_LIBRARY_PATH="$top_builddir/lib/byfl/.libs:$LD_LIBRARY_PATH" \
  ./simple-flang-no-opts

# Test 5: Can we postprocess the binary output?  Are the results correct within
# an order of magnitude?
int_ops=`"$top_builddir/tools/postproc/bfbin2csv" --include=Program --flat-output simple-flang-no-opts.byfl | "$AWK" -F, '$3 ~ /Integer operations/ {print $4}'`
if [ ! -z "$int_ops" ] && [ "$int_ops" -lt 100000 ] ; then
    exit 1
fi

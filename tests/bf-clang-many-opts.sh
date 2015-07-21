#! /bin/sh

#######################################
# Try compiling a simple program with #
# bf-clang and many Byfl options      #
#                                     #
# By Scott Pakin <pakin@lanl.gov>     #
#######################################

# Define some helper variables.  The ":-" ones will normally be
# provided by the Makefile.
AWK=${AWK:-awk}
PERL=${PERL:-perl}
srcdir=${srcdir:-../../tests}
top_srcdir=${top_srcdir:-../..}
top_builddir=${top_builddir:-..}
LLVM_CONFIG=${LLVM_CONFIG:-llvm-config}
clang=${BF_CLANG:-clang}
bf_clang="$top_builddir/tools/wrappers/bf-clang"

# Log everything we do.  Fail on the first error.
set -e
set -x

# Test 1: Do the C compiler and linker work at all?
"$clang" -O2 -g -o simple-clang-many-opts "$srcdir/simple.c" `$LLVM_CONFIG --cflags`

# Test 2: Do the C compiler and linker work when invoked from the Byfl
# wrapper script?
env BF_DISABLE=byfl \
  "$PERL" -I"$top_srcdir/tools/wrappers" \
    "$bf_clang" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
                -bf-verbose -O2 -g -o simple-clang-many-opts "$srcdir/simple.c" \
                `$LLVM_CONFIG --cflags` \
                -L"$top_builddir/lib/byfl/.libs" \
                -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb -bf-reuse-dist -bf-mem-footprint -bf-types -bf-inst-mix -bf-data-structs -bf-inst-deps

# Test 3: Can the Byfl wrapper script compile, instrument, and link a program?
"$PERL" -I"$top_srcdir/tools/wrappers" \
  "$bf_clang" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
              -bf-verbose -O2 -g -o simple-clang-many-opts "$srcdir/simple.c" \
              `$LLVM_CONFIG --cflags` \
              -L"$top_builddir/lib/byfl/.libs" \
              -bf-unique-bytes -bf-by-func -bf-call-stack -bf-vectors -bf-every-bb -bf-reuse-dist -bf-mem-footprint -bf-types -bf-inst-mix -bf-data-structs -bf-inst-deps

# Test 4: Does the Byfl-instrumented program run without error?
env LD_LIBRARY_PATH="$top_builddir/lib/byfl/.libs:$LD_LIBRARY_PATH" \
  ./simple-clang-many-opts

# Test 5: Can we postprocess the binary output?  Are the results correct within
# an order of magnitude?
int_ops=`"$top_builddir/tools/postproc/bfbin2csv" --include=Program --flat-output simple-clang-many-opts.byfl | "$AWK" -F, '$3 ~ /Integer operations/ {print $4}'`
if [ ! -z "$int_ops" ] && [ "$int_ops" -lt 100000 ] ; then
    exit 1
fi

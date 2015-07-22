#! /bin/sh

##########################################
# Try compiling a simple program with    #
# bf-clang++ and no special Byfl options #
#                                        #
# By Scott Pakin <pakin@lanl.gov>        #
##########################################

# Define some helper variables.  The ":-" ones will normally be
# provided by the Makefile.
AWK=${AWK:-awk}
PERL=${PERL:-perl}
srcdir=${srcdir:-../../tests}
top_srcdir=${top_srcdir:-../..}
top_builddir=${top_builddir:-..}
LLVM_CONFIG=${LLVM_CONFIG:-llvm-config}
clangxx=${BF_CLANGXX:-clang++}
bf_clangxx="./bf-clang++"

# Log everything we do.  Fail on the first error.
set -e
set -x

# Test 1: Do the C compiler and linker work at all?
"$clangxx" -g -o simple-clang++-no-opts "$srcdir/simple.cpp" `$LLVM_CONFIG --cxxflags --ldflags --libs`

# Test 2: Do the C compiler and linker work when invoked from the Byfl
# wrapper script?
env BF_DISABLE=byfl \
  "$PERL" -I"$top_srcdir/tools/wrappers" \
    "$bf_clangxx" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
                  -bf-verbose -g -o simple-clang++-no-opts "$srcdir/simple.cpp" \
                  `$LLVM_CONFIG --cxxflags --ldflags --libs` \
                  -L"$top_builddir/lib/byfl/.libs"

# Test 3: Can the Byfl wrapper script compile, instrument, and link a program?
"$PERL" -I"$top_srcdir/tools/wrappers" \
  "$bf_clangxx" -bf-plugin="$top_builddir/lib/bytesflops/.libs/bytesflops.so" \
                -bf-verbose -g -o simple-clang++-no-opts "$srcdir/simple.cpp" \
                `$LLVM_CONFIG --cxxflags --ldflags --libs` \
                -L"$top_builddir/lib/byfl/.libs"

# Test 4: Does the Byfl-instrumented program run without error?
env LD_LIBRARY_PATH="$top_builddir/lib/byfl/.libs:$LD_LIBRARY_PATH" \
  ./simple-clang++-no-opts

# Test 5: Can we postprocess the binary output?  Are the results correct within
# an order of magnitude?
int_ops=`"$top_builddir/tools/postproc/bfbin2csv" --include=Program --flat-output simple-clang++-no-opts.byfl | "$AWK" -F, '$3 ~ /Integer operations/ {print $4}'`
if [ ! -z "$int_ops" ] && [ "$int_ops" -lt 100000 ] ; then
    exit 1
fi

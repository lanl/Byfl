dnl #######################################
dnl ### Configure the Byfl build        ###
dnl ### By Scott Pakin <pakin@lanl.gov> ###
dnl #######################################

dnl Initialize Autoconf and friends.
AC_INIT([byfl], [1.2], [pakin@lanl.gov])
AC_CONFIG_SRCDIR([lib/byfl/byfl.h])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

dnl Specify various fixed strings to use in man pages.
AC_MSG_CHECKING([the precise Byfl version])
MAN_RELEASE="`git rev-parse --short HEAD 2>/dev/null`"
if test "x$MAN_RELEASE" = x ; then
   MAN_RELEASE="v$PACKAGE_VERSION"
fi
AC_MSG_RESULT([$MAN_RELEASE])
AC_SUBST([MAN_RELEASE])
AC_SUBST([MAN_CATEGORY], ["Byfl tools"])

dnl Define in config.h the version of Byfl we're currently configuring.
byfl_git_sha1="`git rev-parse HEAD 2>/dev/null`"
if test "x$byfl_git_sha1" != x ; then
  AC_DEFINE_UNQUOTED([BYFL_GIT_SHA1], ["$byfl_git_sha1"],
    [Define as the SHA-1 hash for the latest Git commit if known.])
fi
byfl_git_branch="`git rev-parse --abbrev-ref HEAD 2>/dev/null`"
if test "x$byfl_git_branch" != x ; then
  AC_DEFINE_UNQUOTED([BYFL_GIT_BRANCH], ["$byfl_git_branch"],
    [Define as the current Git branch if known.])
fi

dnl Define a string containing the canonical name of the target system.
AC_DEFINE_UNQUOTED([BYFL_HOST_TRIPLE], ["$host"],
  [Define as an ARCH-VENDOR-OS string.])

dnl Tell the Makefiles where plugins should be installed.
AC_ARG_WITH([plugindir],
  [AS_HELP_STRING([--with-plugindir=DIR],
    [install the Byfl LLVM plugin into DIR [default=$libexecdir/byfl]])],
  [plugindir=$withval],
  [plugindir=$libexecdir/$PACKAGE_NAME])
AC_SUBST([plugindir], [$plugindir])

dnl Building Byfl's MPI wrappers requires Perl.
AC_CHECK_PROGS([PERL], [perl], [no])
if test "$PERL" = no ; then
   AC_MSG_ERROR([Byfl can't build without Perl])
fi
AC_CHECK_PROGS([POD2MAN], [pod2man], [pod2man])
AC_CACHE_CHECK([where Perl looks for architecture-independent modules],
  [ax_cv_dir_perl_sitelib],
  [
cat <<'EOF' > conftest.pl
use Config;
$op = $Config{siteprefix};
$np = $ARGV[[0]];
$dir = $Config{sitelibexp};
$dir =~ s/^\Q$op\E//;
print "$np$dir\n";
EOF
   ax_cv_dir_perl_sitelib="`$PERL conftest.pl '${prefix}'`"
   rm -f conftest.pl
  ])
AC_SUBST([perlsitelibdir], [$ax_cv_dir_perl_sitelib])

dnl We need grep, sed, and make within this script.
AC_PROG_GREP
AC_PROG_SED
AC_CHECK_PROGS([MAKE], [make gmake gnumake], [make])

dnl We want to log the current timezone name if possible.
AC_STRUCT_TIMEZONE

dnl Use llvm-config to set our default compiler and linker flags.
AC_CHECK_PROGS([LLVM_CONFIG], [llvm-config], [no])
if test "$LLVM_CONFIG" = no ; then
   AC_MSG_ERROR([Byfl can't build without the llvm-config tool])
fi
AC_MSG_CHECKING([the LLVM version using $LLVM_CONFIG])
llvm_version="`$LLVM_CONFIG --version`"
AC_MSG_RESULT([$llvm_version])
AC_DEFINE_UNQUOTED([BYFL_LLVM_VERSION], ["$llvm_version"],
  [Define as the version of LLVM against which Byfl was built.])
if test "${CPPFLAGS:-undefined}" = undefined ; then
   CPPFLAGS=`${LLVM_CONFIG} --cppflags`
   echo "$CPPFLAGS" | $GREP -- -D_GNU_SOURCE >/dev/null 2>&1 || CPPFLAGS="$CPPFLAGS -D_GNU_SOURCE"
fi
if test "${CFLAGS:-undefined}" = undefined ; then
   CFLAGS=`${LLVM_CONFIG} --cflags`
fi
if test "${CXXFLAGS:-undefined}" = undefined ; then
   CXXFLAGS=`${LLVM_CONFIG} --cxxflags`
fi
AC_SUBST([llvm_libdir], [`${LLVM_CONFIG} --libdir`])
AC_ARG_VAR([LLVM_CONFIG], [LLVM configuration tool])

dnl Byfl requires C++11.  Abort if we don't have C++11 support.
AC_PROG_CXX
AX_CHECK_CXX11
if test "x$ax_cv_cxx11_CXX" = xno ; then
  AC_MSG_FAILURE([Byfl requires C++11 support; try recompiling with -std=c++11])
fi

dnl Byfl can give more informative warning messages if the system supports
dnl weak function aliases, which, as the time of this writing, OS X does not.
AX_CHECK_WEAK_ALIASES

dnl OS X lacks a /proc/self/cmdline file, but it does provide a _NSGetArgv()
dnl function to read the caller's command line.
AC_CHECK_FUNCS([_NSGetArgv],
  [AC_CHECK_HEADERS([crt_externs.h])])

dnl The Byfl build process parses Instruction.def.  Ensure it exists.
AC_CHECK_HEADER([llvm/IR/Instruction.def])
if test "$ac_cv_header_llvm_IR_Instruction_def" != yes ; then
  AC_MSG_ERROR([Byfl requires the llvm/IR/Instruction.def C++ header file])
fi

dnl Tally the number of LLVM opcodes.
AC_CACHE_CHECK([the number of opcodes defined by llvm/IR/Instruction.def],
  [ax_cv_llvm_opcodes],
  [ax_cv_llvm_opcodes=`"$PERL" "$srcdir/gen_opcode2name" "$CPP $CPPFLAGS" NUM`])
if test -z "$ax_cv_llvm_opcodes" ; then
   AC_MSG_ERROR([Failed to run $PERL and/or $CPP $CPPFLAGS])
fi
AC_DEFINE_UNQUOTED([NUM_LLVM_OPCODES], [$ax_cv_llvm_opcodes],
  [Define as one more than the maximum-valued opcode in the LLVM IR.])
AC_CACHE_CHECK([how to round up $ax_cv_llvm_opcodes+2 opcodes to a power of two],
  [ax_cv_llvm_opcodes_pow2],
  [ax_cv_llvm_opcodes_pow2=`"$PERL" "$srcdir/gen_opcode2name" "$CPP $CPPFLAGS" NUM_POW2`])
if test -z "$ax_cv_llvm_opcodes_pow2" ; then
   AC_MSG_ERROR([Failed to run $PERL and/or $CPP $CPPFLAGS])
fi
AC_DEFINE_UNQUOTED([NUM_LLVM_OPCODES_POW2], [$ax_cv_llvm_opcodes_pow2],
  [Define as NUM_LLVM_OPCODES+2 rounded up to a power of two.])

dnl The Byfl binary-output library uses asprintf for error reporting but can
dnl work around its absence with an unsafe alternative.
AC_CHECK_FUNCS([asprintf])

dnl If HDF5 is installed and seems to work we can try building bfbin2hdf5.
AC_ARG_VAR([H5CXX], [Compiler for C++ code using HDF5])
AC_CHECK_PROGS([H5CXX], [h5c++], [no])
if test "x$H5CXX" != xno ; then
  AX_CHECK_CXX11([H5CXX])
  if test "x$ax_cv_cxx11_H5CXX" = xno ; then
    H5CXX=no
  fi
fi
if test "x$H5CXX" != xno ; then
  AC_CACHE_CHECK([if $H5CXX can compile and link an HDF5 program],
    [ax_cv_link_h5cxx],
    [orig_CXX="$CXX"
     CXX="$H5CXX"
     AC_LANG_PUSH([C++])
     AC_LINK_IFELSE(
       [AC_LANG_PROGRAM([[#include <H5Cpp.h>]], [
  H5::H5File hdf5file("myfile.h5", H5F_ACC_TRUNC);
  return 0;
       ])],
       [ax_cv_link_h5cxx=yes],
       [ax_cv_link_h5cxx=no])
     AC_LANG_POP([C++])
     CXX="$orig_CXX"
    ])
fi
if test "x$ax_cv_link_h5cxx" = xno ; then
  H5CXX=no
fi
if test "x$H5CXX" = xno ; then
  H5CXX="$CXX"
  BUILD_BFBIN2HDF5=no
else
  BUILD_BFBIN2HDF5=yes
fi
AM_CONDITIONAL([HDF5_AVAILABLE], [test $BUILD_BFBIN2HDF5 != no])
AC_ARG_VAR([H5CXXFLAGS], [HDF5 C++ compiler flags])
if test "x${H5CXXFLAGS-undefined}" = xundefined ; then
  H5CXXFLAGS="$CXXFLAGS"
fi

dnl If getopt_long is available we can build bfbin2text.
AC_CHECK_FUNCS([getopt_long])
AM_CONDITIONAL([GETOPT_LONG_AVAILABLE], [test "x$ac_cv_func_getopt_long" != xno])

dnl If SQLite3 is available we can build bfbin2sqlite3.
BUILD_BFBIN2SQLITE3=no
AC_CHECK_LIB([sqlite3], [sqlite3_errstr], [true]) dnl Don't modify LIBS.
if test "x$ac_cv_lib_sqlite3_sqlite3_errstr" != xno ; then
  AC_CHECK_HEADER([sqlite3.h], [BUILD_BFBIN2SQLITE3=yes])
fi
AM_CONDITIONAL([SQLITE3_AVAILABLE], [test $BUILD_BFBIN2SQLITE3 = yes])

dnl The GCC DragonEgg plugin, while not strictly required by Byfl,
dnl precludes the use of most of the Byfl wrapper scripts.
AC_ARG_VAR([DRAGONEGG], [absolute filename of the GCC DragonEgg plugin])
if test "${DRAGONEGG:-no}" = no ; then
  # The location of the DragonEgg plugin was not specified -- try to find it.
  AC_CACHE_CHECK([for dragonegg.so],
    [ax_cv_file_dragonegg_so],
    [dnl
  # Search for dragonegg.so in various likely places.
  ax_cv_file_dragonegg_so=no
  for dname in `$LLVM_CONFIG --libdir` `echo $LD_LIBRARY_PATH | $SED 's/:/\n/g'` /usr/local/lib* /usr/lib* ; do
    if test -f "$dname/dragonegg.so" ; then
      ax_cv_file_dragonegg_so="$dname/dragonegg.so"
      break
    fi
  done
  ])
  if test "$ax_cv_file_dragonegg_so" = no ; then
    dnl Define where dragonegg.so *ought* to be but isn't.
    DRAGONEGG="`$LLVM_CONFIG --libdir`/dragonegg.so"
  else
    DRAGONEGG="$ax_cv_file_dragonegg_so"
  fi
else
  ax_cv_file_dragonegg_so="$DRAGONEGG"
fi
AM_CONDITIONAL([HAVE_DRAGONEGG], [test x$ax_cv_file_dragonegg_so != xno])

dnl Generate all of the Byfl Makefiles and other files.
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([lib/Makefile])
AC_CONFIG_FILES([lib/byfl/Makefile])
AC_CONFIG_FILES([lib/bytesflops/Makefile])
AC_CONFIG_FILES([tools/Makefile])
AC_CONFIG_FILES([tools/wrappers/byfl-clang.pc])
AC_CONFIG_FILES([tools/postproc/Makefile])
AC_CONFIG_FILES([tools/postproc/bfbin2py], [
    cat tools/postproc/bfbin2py | `$MAKE -s sed-command | $GREP sed` > tools/postproc/bfbin2py.tmp
    mv tools/postproc/bfbin2py.tmp tools/postproc/bfbin2py
  ],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_CONFIG_FILES([tools/wrappers/Makefile])
AC_CONFIG_FILES([tools/wrappers/bf-gcc], [
    cat tools/wrappers/bf-gcc | `$MAKE -s sed-command | $GREP sed` > tools/wrappers/bf-gcc.tmp
    mv tools/wrappers/bf-gcc.tmp tools/wrappers/bf-gcc
  ],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_CONFIG_FILES([tools/wrappers/bf-clang], [
    cat tools/wrappers/bf-clang | `$MAKE -s sed-command | $GREP sed` > tools/wrappers/bf-clang.tmp
    mv tools/wrappers/bf-clang.tmp tools/wrappers/bf-clang
  ],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_CONFIG_FILES([tools/wrappers/bf-inst], [
    cat tools/wrappers/bf-inst | `$MAKE -s sed-command | $GREP sed` > tools/wrappers/bf-inst.tmp
    mv tools/wrappers/bf-inst.tmp tools/wrappers/bf-inst],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_OUTPUT

dnl Issue various warning messages here, where we hope the user will
dnl notice them.
if test "x$enable_shared" = xno ; then
  AC_MSG_WARN([Byfl's LLVM plugin must be built as a shared object.  The build will override --disable-shared in the lib/bytesflops directory.])
fi
if test "x$ac_cv_func_asprintf" != xyes; then
  AC_MSG_WARN([Using an unsafe alternative to asprintf, which wasn't found])
fi
if test $BUILD_BFBIN2HDF5 = no ; then
  AC_MSG_WARN([Not building the bfbin2hdf5 postprocessor because it requires a working HDF5 with C++11 support])
fi
if test $BUILD_BFBIN2SQLITE3 = no ; then
  AC_MSG_WARN([Not building the bfbin2sqlite3 postprocessor because it requires SQLite3 (v3.7.15+)])
fi
if test x$ax_cv_file_dragonegg_so != xno -a ! -e "$DRAGONEGG" ; then
  # The location of the DragonEgg plugin was specified but does not exist.
  AC_MSG_WARN([$DRAGONEGG does not exist; did you specify an incorrect file for the DRAGONEGG variable?])
fi

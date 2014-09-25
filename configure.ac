dnl #######################################
dnl ### Configure the Byfl build        ###
dnl ### By Scott Pakin <pakin@lanl.gov> ###
dnl #######################################

dnl Initialize Autoconf and friends.
AC_INIT([byfl], [1.0], [pakin@lanl.gov])
AC_CONFIG_SRCDIR([lib/byfl/byfl.h])
AM_INIT_AUTOMAKE([foreign])
LT_INIT

dnl Building Byfl's MPI wrappers requires Perl.
AC_CHECK_PROGS([PERL], [perl], [no])
if test "$PERL" = no ; then
   AC_MSG_ERROR([Byfl can't build without Perl])
fi

dnl We need grep, sed, and make within this script.
AC_PROG_GREP
AC_PROG_SED
AC_CHECK_PROGS([MAKE], [make gmake gnumake], [make])

dnl Use llvm-config to set our default compiler and linker flags.
AC_CHECK_PROGS([LLVM_CONFIG], [llvm-config], [no])
if test "$LLVM_CONFIG" = no ; then
   AC_MSG_ERROR([Byfl can't build without the llvm-config tool])
fi
if test "${CPPFLAGS:-undefined}" = undefined ; then
   CPPFLAGS=`${LLVM_CONFIG} --cppflags`
fi
if test "${CFLAGS:-undefined}" = undefined ; then
   CFLAGS=`${LLVM_CONFIG} --cflags`
fi
if test "${CXXFLAGS:-undefined}" = undefined ; then
   CXXFLAGS=`${LLVM_CONFIG} --cxxflags`
fi
AC_SUBST([llvm_libdir], [`${LLVM_CONFIG} --libdir`])

dnl Byfl requires C++11.  Abort if we don't have C++11 support.
AC_PROG_CXX
AC_LANG_PUSH([C++])
AC_MSG_CHECKING([if $CXX provides C++11 support])
AC_COMPILE_IFELSE(
 [AC_LANG_SOURCE([
#include <string>

using namespace std;

string::const_iterator check_cxx11 (string mystring)
{
  return mystring.cbegin();
}
 ])],
 [AC_MSG_RESULT([yes])],
 [AC_MSG_RESULT([no])
  AC_MSG_FAILURE([Byfl requires C++11 support; try recompiling with -std=c++11 (or similar)])])
AC_LANG_POP([C++])

dnl The Byfl build process parses Instruction.def.  Ensure it exists.
AC_CHECK_HEADER([llvm/IR/Instruction.def])
if test "$ac_cv_header_llvm_IR_Instruction_def" != yes ; then
  AC_MSG_ERROR([Byfl requires the llvm/IR/Instruction.def C++ header file])
fi

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
    AC_MSG_WARN([Byfl's GCC wrapper scripts require DragonEgg and will fail without it])
    DRAGONEGG="`$LLVM_CONFIG --libdir`/dragonegg.so"
  else
    DRAGONEGG="$ax_cv_file_dragonegg_so"
  fi
elif test ! -e "$DRAGONEGG" ; then
  # The location of the DragonEgg plugin was specified -- see if it exists.
  AC_MSG_WARN([Did you specify an incorrect file for the DRAGONEGG variable ($DRAGONEGG)?])
fi

dnl Warn the user that we'll override --disable-shared in the plugin directory.
if test "$enable_shared" = no ; then
  AC_MSG_WARN([Byfl's LLVM plugin must be built as a shared object.  The build will attempt to override --disable-shared in the lib/bytesflops directory.])
fi

dnl Generate all of the Byfl Makefiles and other files.
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([lib/Makefile])
AC_CONFIG_FILES([lib/byfl/Makefile])
AC_CONFIG_FILES([lib/bytesflops/Makefile])
AC_CONFIG_FILES([tools/Makefile])
AC_CONFIG_FILES([tools/postproc/Makefile])
AC_CONFIG_FILES([tools/wrappers/Makefile])
AC_CONFIG_FILES([tools/wrappers/bf-gcc], [
    cat tools/wrappers/bf-gcc | `$MAKE -s sed-command | $GREP sed` > tools/wrappers/bf-gcc.tmp
    mv tools/wrappers/bf-gcc.tmp tools/wrappers/bf-gcc
  ],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_CONFIG_FILES([tools/wrappers/bf-inst], [
    cat tools/wrappers/bf-inst | `$MAKE -s sed-command | $GREP sed` > tools/wrappers/bf-inst.tmp
    mv tools/wrappers/bf-inst.tmp tools/wrappers/bf-inst],
  [MAKE="$MAKE"
   GREP="$GREP"])
AC_OUTPUT

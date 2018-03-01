dnl ----------------------------------------------------------------------
dnl Helper macros for Byfl's configure.ac script
dnl By Scott Pakin <pakin@lanl.gov>
dnl ----------------------------------------------------------------------

dnl @synopsis AX_CHECK_CXX11 ([COMPILER-VAR])
dnl 
dnl Determine if the C++ compiler, named by COMPILER-VAR (default: CXX),
dnl supports C++11 language extensions.  If so, set
dnl ax_cv_cxx11_[COMPILER-VAR] to "yes", otherwise to "no".
AC_DEFUN([AX_CHECK_CXX11],
[AC_REQUIRE([AC_PROG_CXX])
m4_define([ax_cxx_name], [m4_ifval([$1], [ax_cv_cxx11_$1], [ax_cv_cxx11_CXX])])
m4_ifval([$1], [ax_CXX_orig="$CXX" ; CXX="[$]$1"])
AC_CACHE_CHECK([if $CXX provides C++11 support],
  [ax_cxx_name],
  [AC_LANG_PUSH([C++])
   AC_COMPILE_IFELSE(
     [AC_LANG_SOURCE([
#include <string>

using namespace std;

string::const_iterator check_cxx11 (string mystring)
{
  return mystring.cbegin();
}
     ])],
     [eval ax_cxx_name=yes],
     [eval ax_cxx_name=no])
   AC_LANG_POP([C++])])
m4_ifval([$1], [CXX="$ax_CXX_orig"])
])

dnl @synopsis AX_CHECK_WEAK_ALIASES
dnl
dnl Determine if the system can handle weak function aliases.  (At the
dnl time of this writing, OS X cannot.)  If so, define HAVE_WEAK_ALIASES.
AC_DEFUN([AX_CHECK_WEAK_ALIASES],
[AC_REQUIRE([AC_PROG_CC])
AC_CACHE_CHECK([if weak function aliases are supported],
  [ax_cv_have_weak_aliases],
  [AC_LANG_PUSH([C])
   AC_LINK_IFELSE(
     [AC_LANG_SOURCE([
int my_function_impl (void)
{
  return 0;
}

int my_function (void) __attribute__((weak, alias("my_function_impl")));

int main (void)
{
  return my_function();
}
     ])],
     [ax_cv_have_weak_aliases=yes],
     [ax_cv_have_weak_aliases=no])])
if test "x$ax_cv_have_weak_aliases" = xyes ; then
  AC_DEFINE([HAVE_WEAK_ALIASES], [1],
    [Define if the compiler and linker support weak function aliases.])
fi
])

dnl @synopsis AX_CHECK_PROGS_EXIST (VARIABLE, PROGS-TO-CHECK-FOR,
dnl                                 [VALUE-IF-NOT-FOUND], [PATH])
dnl
dnl Perform the same operation as AC_CHECK_PROGS but actually
dnl verify that the program exists, even if the user specified
dnl VARIABLE explicitly.
AC_DEFUN([AX_CHECK_PROGS_EXIST],
[AC_CHECK_PROGS([$1], [$2], [$3], [$4])
AS_VAR_IF([$1], ["$3"], , [
  AC_CACHE_CHECK([if [$]$1 really exists],
    [ax_cv_prog_$1_exists],
    [ax_which=`which which`
     if "$ax_which" "[$]$1" >/dev/null 2>&1 ; then
       ax_cv_prog_$1_exists=yes
     else
       ax_cv_prog_$1_exists=no
       $1="$3"
     fi
    ])
  ])
])

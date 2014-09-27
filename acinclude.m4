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

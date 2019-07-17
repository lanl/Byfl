############################################
# Install wrappers for various compilers   #
# to facilitate compiling them with Byfl   #
#                                          #
# By Scott Pakin <pakin@lanl.gov>          #
############################################

# Install a Perl module for parsing compiler flags.  We replace the Perl site
# prefix with the CMake install prefix.  Doing so installs all Byfl files under
# a single top level but at the cost of the Perl module not winding up in a
# place Perl looks for modules.
perl_get_info(PERL_SITELIBEXP sitelibexp IS_PATH)
perl_get_info(PERL_SITEPREFIX siteprefix IS_PATH)
string(LENGTH ${PERL_SITEPREFIX} _prefix_len)
math(EXPR _prefix_len "${_prefix_len} + 1")
string(SUBSTRING ${PERL_SITELIBEXP} ${_prefix_len} -1 _rel_sitelib)
set(
  PERL_MODULE_DIRECTORY
  ${CMAKE_INSTALL_PREFIX}/${_rel_sitelib}
  CACHE PATH
  "Directory in which to install the Byfl Perl module"
  )
mark_as_advanced(PERL_MODULE_DIRECTORY)
install(
  FILES ParseCompilerOpts.pm
  DESTINATION ${PERL_MODULE_DIRECTORY}
  )

# Generate a Byfl pkg-config file.
configure_file(byfl-clang.pc.in byfl-clang.pc @ONLY)
set(
  PKG_CONFIG_DIRECTORY
  ${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig
  CACHE PATH
  "Directory in which to install a Byfl pkg-config file"
  )
mark_as_advanced(PKG_CONFIG_DIRECTORY)
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/byfl-clang.pc
  DESTINATION ${PKG_CONFIG_DIRECTORY}
  )

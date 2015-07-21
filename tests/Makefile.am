###################################
# Test that Byfl actually works   #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

TESTS = bf-clang-no-opts.sh bf-clang-many-opts.sh

EXTRA_DIST = bf-clang-no-opts.sh bf-clang-many-opts.sh simple.c

AM_TESTS_ENVIRONMENT = \
	AWK='$(AWK)'; export AWK; \
	PERL='$(PERL)'; export PERL; \
	srcdir='$(srcdir)'; export srcdir; \
	top_srcdir='$(top_srcdir)'; export top_srcdir; \
	top_builddir='$(top_builddir)'; export top_builddir;

CLEANFILES = \
	simple-no-opts \
	simple-no-opts.byfl \
	simple-many-opts \
	simple-many-opts.byfl

# On OS X we may wind up with a simple.dSYM directory that needs to be deleted.
clean-local:
	$(RM) -r simple-no-opts.dSYM
	$(RM) -r simple-many-opts.dSYM

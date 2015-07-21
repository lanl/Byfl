###################################
# Test that Byfl actually works   #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

TESTS = \
	bf-clang-no-opts.sh \
	bf-clang-many-opts.sh \
	bf-clang++-no-opts.sh \
	bf-gcc-no-opts.sh

EXTRA_DIST = $(TESTS) simple.c simple.cpp

AM_TESTS_ENVIRONMENT = \
	AWK='$(AWK)'; export AWK; \
	PERL='$(PERL)'; export PERL; \
	srcdir='$(srcdir)'; export srcdir; \
	top_srcdir='$(top_srcdir)'; export top_srcdir; \
	top_builddir='$(top_builddir)'; export top_builddir;

CLEANFILES = \
	simple-clang-no-opts \
	simple-clang-no-opts.byfl \
	simple-clang-many-opts \
	simple-clang-many-opts.byfl \
	simple-clang++-no-opts \
	simple-clang++-no-opts.byfl

if HAVE_DRAGONEGG
CLEANFILES += \
	simple-gcc-no-opts \
	simple-gcc-no-opts.byfl
endif

# On OS X we may wind up with a simple.dSYM directory that needs to be deleted.
clean-local:
	$(RM) -r simple-clang-no-opts.dSYM
	$(RM) -r simple-clang-many-opts.dSYM
	$(RM) -r simple-clang++-no-opts.dSYM
	$(RM) -r simple-gcc-no-opts.dSYM

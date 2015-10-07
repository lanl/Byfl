###################################
# Test that Byfl actually works   #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

TESTS = \
	bf-clang-no-opts.sh \
	bf-clang++-no-opts.sh \
	bf-gcc-no-opts.sh \
	bf-clang-many-opts.sh \
	bfbin2cgrind.sh \
	bfbin2csv.sh \
	bfbin2hpctk.sh \
	bfbin2xmlss.sh

if HDF5_AVAILABLE
  TESTS += bfbin2hdf5.sh
endif
if SQLITE3_AVAILABLE
  TESTS += bfbin2sqlite3.sh
endif

TEST_EXTENSIONS = .sh

# All bfbin2* tests depend on simple-clang-many-opts.byfl, which is
# created as a side effect of running bf-clang-many-opts.sh.
simple-clang-many-opts.byfl: bf-clang-many-opts.log
bfbin2csv.log: simple-clang-many-opts.byfl
bfbin2cgrind.log: simple-clang-many-opts.byfl
bfbin2hpctk.log: simple-clang-many-opts.byfl
bfbin2hdf5.log: simple-clang-many-opts.byfl
bfbin2sqlite3.log: simple-clang-many-opts.byfl
bfbin2xmlss.log: simple-clang-many-opts.byfl

EXTRA_DIST = \
	$(TESTS) \
	simple.c \
	simple.cpp

AM_TESTS_ENVIRONMENT = \
	AWK='$(AWK)'; export AWK; \
	PERL='$(PERL)'; export PERL; \
	srcdir='$(srcdir)'; export srcdir; \
	top_srcdir='$(top_srcdir)'; export top_srcdir; \
	top_builddir='$(top_builddir)'; export top_builddir; \
	use_dragonegg='$(use_dragonegg)'; export use_dragonegg; \
	echo "*** AM_TESTS_ENVIRONMENT seems to be working; use_dragonegg=$$use_dragonegg ***";

# bf-clang++ isn't normally symlinked until install time.  Symlink it here.
bf-clang++-no-opts.sh: bf-clang++
bf-clang++: $(top_builddir)/tools/wrappers/bf-clang
	$(LN_S) $(top_builddir)/tools/wrappers/bf-clang bf-clang++

CLEANFILES = \
	simple-clang-no-opts \
	simple-clang-no-opts.byfl \
	simple-clang-many-opts \
	simple-clang-many-opts.byfl \
	simple-clang-many-opts.csv \
	simple-clang-many-opts.cgrind \
	simple-clang-many-opts.h5 \
	simple-clang-many-opts.db \
	simple-clang-many-opts.xml \
	simple-clang++-no-opts \
	simple-clang++-no-opts.byfl \
	simple-gcc-no-opts \
	simple-gcc-no-opts.byfl \
	simple.o \
	bf-clang++

# On OS X we may wind up with a simple.dSYM directory that needs to be deleted.
clean-local:
	$(RM) -r simple-clang-no-opts.dSYM
	$(RM) -r simple-clang-many-opts.dSYM
	$(RM) -r simple-clang++-no-opts.dSYM
	$(RM) -r simple-gcc-no-opts.dSYM
	$(RM) -r hpctoolkit-simple-clang-many-opts-database

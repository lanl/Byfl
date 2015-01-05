############################################
# Install wrappers for various GCC progams #
# to facilitate compiling them with Byfl   #
#					   #
# By Scott Pakin <pakin@lanl.gov>	   #
############################################

nodist_bin_SCRIPTS = \
	bf-inst \
	bf-gcc

EXTRA_DIST = \
	bf-gcc.in \
	bf-inst.in \
	ParseGccOpts.pm \
	make-bf-mpi

CLEANFILES = $(nodist_bin_SCRIPTS)

PERL_SITELIBEXP = $(shell $(PERL) -e 'use Config; $$op=$$Config{siteprefix}; $$np="$(prefix)"; $$dir=$$Config{sitelibexp}; $$dir=~s/^\Q$$op\E/$$np/; print "$$dir\n"')

AM_DEFAULT_VERBOSITY ?= 1
SILENT_MAKE = $(SILENT_MAKE_$(V))
SILENT_MAKE_ = $(SILENT_MAKE_$(AM_DEFAULT_VERBOSITY))
SILENT_MAKE_0 = -s
SILENT_MAKE_1 = NOECHO=""

install-parser: $(srcdir)/ParseGccOpts.pm
	$(INSTALL_DATA) -D "$(srcdir)/ParseGccOpts.pm" "$(DESTDIR)$(PERL_SITELIBEXP)/ParseGccOpts.pm"

install-compiler-scripts: bf-gcc
	$(LN_S) -f bf-gcc $(DESTDIR)$(bindir)/bf-g++
	$(LN_S) -f bf-gcc $(DESTDIR)$(bindir)/bf-gfortran
	$(LN_S) -f bf-gcc $(DESTDIR)$(bindir)/bf-gccgo

install-mpi-scripts: $(srcdir)/make-bf-mpi
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpicc bf-gcc OMPI_CC MPICH_CC $(bindir) > $(DESTDIR)$(bindir)/bf-mpicc
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpicxx bf-g++ OMPI_CXX MPICH_CXX $(bindir) > $(DESTDIR)$(bindir)/bf-mpicxx
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpif90 bf-gfortran OMPI_FC MPICH_F90 $(bindir) > $(DESTDIR)$(bindir)/bf-mpif90
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpif77 bf-gfortran OMPI_F77 MPICH_F77 $(bindir) > $(DESTDIR)$(bindir)/bf-mpif77

install-man1-pages: bf-gcc
	$(INSTALL) -d "$(DESTDIR)$(mandir)/man1"
	$(RM) "$(DESTDIR)$(mandir)/man1/bf-gcc.1" \
	      "$(DESTDIR)$(mandir)/man1/bf-g++.1" \
	      "$(DESTDIR)$(mandir)/man1/bf-gfortran.1" \
	      "$(DESTDIR)$(mandir)/man1/bf-gccgo.1"
	$(AM_V_GEN) $(POD2MAN) --center="$(MAN_CATEGORY)" --release="$(MAN_RELEASE)" bf-gcc > \
	  "$(DESTDIR)$(mandir)/man1/bf-gcc.1"
	$(AM_V_GEN) cat "$(DESTDIR)$(mandir)/man1/bf-gcc.1" | \
	  $(PERL) -ne 's/gcc/g++/g; s/\bC (compiler|code)/C++ $$1/g; s/myprog\.c/myprog.cpp/g; s/BF_GCC/BF_GXX/g; print' > \
	    "$(DESTDIR)$(mandir)/man1/bf-g++.1"
	$(AM_V_GEN) cat "$(DESTDIR)$(mandir)/man1/bf-gcc.1" | \
	  $(PERL) -ne 's/gcc/gfortran/g; s/\bC (compiler|code)/Fortran $$1/g; s/myprog\.c/myprog.f90/g; s/BF_GCC/BF_GFORTRAN/g; print' > \
	    "$(DESTDIR)$(mandir)/man1/bf-gfortran.1"
	$(AM_V_GEN) cat "$(DESTDIR)$(mandir)/man1/bf-gcc.1" | \
	    $(PERL) -ne 's/gcc/gccgo/g; s/\bC (compiler|code)/Go $$1/g; s/myprog\.c/myprog.go/g; s/BF_GCC/BF_GCCGO/g; print' > \
	    "$(DESTDIR)$(mandir)/man1/bf-gccgo.1"

install-man3-pages: $(srcdir)/ParseGccOpts.pm
	$(INSTALL) -d "$(DESTDIR)$(mandir)/man3"
	$(RM) "$(DESTDIR)$(mandir)/man3/ParseGccOpts.3pm"
	$(AM_V_GEN) $(POD2MAN) --name=ParseGccOpts --section=3pm --center="$(MAN_CATEGORY)" --release="$(MAN_RELEASE)" "$(srcdir)/ParseGccOpts.pm" > "$(DESTDIR)$(mandir)/man3/ParseGccOpts.3pm"

install-exec-local: install-parser install-compiler-scripts install-mpi-scripts install-man1-pages install-man3-pages

uninstall-local:
	for script in bf-gcc bf-g++ bf-gfortran bf-gccgo bf-mpicc bf-mpicxx bf-mpif90 bf-mpif77 ; do \
	  $(RM) "$(DESTDIR)$(bindir)/$${script}" ; \
	  $(RM) "$(DESTDIR)$(mandir)/man1/$${script}.1" ; \
	done
	$(RM) "$(DESTDIR)$(mandir)/man3/ParseGccOpts.3pm"
	$(RM) "$(DESTDIR)$(PERL_SITELIBEXP)/ParseGccOpts.pm"

.PHONY: install-parser install-compiler-scripts install-mpi-scripts install-man1-pages install-man3-pages

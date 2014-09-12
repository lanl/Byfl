############################################
# Install wrappers for various GCC progams #
# to facilitate compiling them with Byfl   #
#					   #
# By Scott Pakin <pakin@lanl.gov>	   #
############################################

nodist_bin_SCRIPTS = \
	bf-inst \
	bf-gcc \
	bf-g++ \
	bf-gfortran \
	bf-gccgo \
	bf-mpicc \
	bf-mpicxx \
	bf-mpif90 \
	bf-mpif77

EXTRA_DIST = bf-gcc.in bf-inst.in Makefile.PL lib/ParseGccOpts.pm make-bf-mpi
CLEANFILES = $(nodist_bin_SCRIPTS) wrappers.mak

bf-g++: bf-gcc
	$(AM_V_GEN) cp bf-gcc bf-g++

bf-gfortran: bf-gcc
	$(AM_V_GEN) cp bf-gcc bf-gfortran

bf-gccgo: bf-gcc
	$(AM_V_GEN) cp bf-gcc bf-gccgo

bf-mpicc: $(srcdir)/make-bf-mpi
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpicc bf-gcc OMPI_CC MPICH_CC $(bindir) > $@

bf-mpicxx: $(srcdir)/make-bf-mpi
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpicxx bf-g++ OMPI_CXX MPICH_CXX $(bindir) > $@

bf-mpif90: $(srcdir)/make-bf-mpi
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpif90 bf-gfortran OMPI_FC MPICH_F90 $(bindir) > $@

bf-mpif77: $(srcdir)/make-bf-mpi
	$(AM_V_GEN) $(PERL) $(srcdir)/make-bf-mpi mpif77 bf-gfortran OMPI_F77 MPICH_F77 $(bindir) > $@

all-local: wrappers.mak

wrappers.mak: $(srcdir)/Makefile.PL bf-gcc $(srcdir)/lib/ParseGccOpts.pm bf-inst
	$(AM_V_GEN) $(PERL) $(srcdir)/Makefile.PL $(if $(AM_V_at),,verbose) \
	  FIRST_MAKEFILE=wrappers.mak PREFIX=$(prefix) \
	  INSTALLMAN1DIR=$(mandir)/man1 INSTALLMAN3DIR=$(mandir)/man3 \
	  INSTALLSITEMAN1DIR=$(mandir)/man1 INSTALLSITEMAN3DIR=$(mandir)/man3
	$(AM_V_at) /bin/echo -e 'echo-%:\n\t@echo "$$($$(subst echo-,,$$@))"' >> wrappers.mak

clean-local:
	-if [ -e wrappers.mak ] ; then $(MAKE) -f wrappers.mak clean ; fi

install-exec-local: wrappers.mak
	$(MAKE) -f wrappers.mak \
	  POD2MAN='$$(POD2MAN_EXE) --center=" " --release=v1.0' pure_install

uninstall-local: wrappers.mak
	SITELIB=`$(MAKE) -f wrappers.mak echo-INSTALLSITELIB | $(GREP) "$(prefix)"` ; \
	 $(RM) "$$SITELIB/ParseGccOpts.pm"
	SITEARCH=`$(MAKE) -f wrappers.mak echo-INSTALLSITEARCH | $(GREP) "$(prefix)"` ; \
	 $(RM) -r "$$SITEARCH/auto/ParseGccOpts"
	SITEMAN1DIR=`$(MAKE) -f wrappers.mak echo-INSTALLSITEMAN1DIR | $(GREP) "$(prefix)"` ; \
	 $(RM) "$$SITEMAN1DIR/bf-inst.1"
	SITEMAN3DIR=`$(MAKE) -f wrappers.mak echo-INSTALLSITEMAN3DIR | $(GREP) "$(prefix)"` ; \
	 $(RM) "$$SITEMAN3DIR/ParseGccOpts.3pm"

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

all-local: wrappers.mak blib

wrappers.mak: $(srcdir)/Makefile.PL bf-gcc $(srcdir)/lib/ParseGccOpts.pm bf-inst
	$(AM_V_at) test -d lib || mkdir lib
	$(AM_V_at) test -f lib/ParseGccOpts.pm || cp $(srcdir)/lib/ParseGccOpts.pm lib/
	$(AM_V_GEN) $(PERL) $(srcdir)/Makefile.PL \
	  FIRST_MAKEFILE=wrappers.mak PREFIX=$(prefix) \
	  INSTALLMAN1DIR=$(mandir)/man1 INSTALLMAN3DIR=$(mandir)/man3 \
	  INSTALLSITEMAN1DIR=$(mandir)/man1 INSTALLSITEMAN3DIR=$(mandir)/man3
	$(AM_V_at) /bin/echo -e 'echo-%:\n\t@echo "$$($$(subst echo-,,$$@))"' >> wrappers.mak

SILENT_MAKE = $(SILENT_MAKE_$(V))
SILENT_MAKE_ = $(SILENT_MAKE_$(AM_DEFAULT_VERBOSITY))
SILENT_MAKE_0 = -s
SILENT_MAKE_1 =

blib: wrappers.mak
	$(AM_V_at) test -f Makefile.PL || cp $(srcdir)/Makefile.PL .
	$(AM_V_at) touch wrappers.mak
	$(AM_V_GEN) $(MAKE) $(SILENT_MAKE) -f wrappers.mak

clean-local: wrappers.mak
	-$(MAKE) -f wrappers.mak clean
	if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
	  $(RM) lib/ParseGccOpts.pm ; \
	  $(RM) Makefile.PL ; \
	fi
	$(RM) MYMETA.json
	$(RM) MYMETA.yml
	$(RM) wrappers.mak.old

install-exec-local: wrappers.mak
	$(MAKE) -f wrappers.mak \
	  POD2MAN='$$(POD2MAN_EXE) --center=" " --release=v1.0' pure_install

uninstall-local: wrappers.mak
	SITELIB=`$(MAKE) -f wrappers.mak echo-INSTALLSITELIB | $(GREP) "$(prefix)"` ; \
	  export SITELIB ; \
	  $(RM) "$$DESTDIR$$SITEMAN3DIR$$SITELIB/ParseGccOpts.pm"
	SITEARCH=`$(MAKE) -f wrappers.mak echo-INSTALLSITEARCH | $(GREP) "$(prefix)"` ; \
	  export SITEARCH ; \
	  $(RM) -r "$$DESTDIR$$SITEMAN3DIR$$SITEARCH/auto/ParseGccOpts"
	SITEMAN1DIR=`$(MAKE) -f wrappers.mak echo-INSTALLSITEMAN1DIR | $(GREP) "$(prefix)"` ; \
	  export SITEMAN1DIR ; \
	  $(RM) "$$DESTDIR$$SITEMAN3DIR$$SITEMAN1DIR/bf-inst.1"
	SITEMAN3DIR=`$(MAKE) -f wrappers.mak echo-INSTALLSITEMAN3DIR | $(GREP) '$(prefix)'` ; \
	  export SITEMAN3DIR ; \
	  $(RM) "$$DESTDIR$$SITEMAN3DIR/ParseGccOpts.3pm"

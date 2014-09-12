############################################
# Install wrappers for various GCC progams #
# to facilitate compiling them with Byfl   #
#					   #
# By Scott Pakin <pakin@lanl.gov>	   #
############################################

dist_bin_SCRIPTS = bf-parse-cache-dump
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
	cp bf-gcc bf-g++

bf-gfortran: bf-gcc
	cp bf-gcc bf-gfortran

bf-gccgo: bf-gcc
	cp bf-gcc bf-gccgo

bf-mpicc: $(srcdir)/make-bf-mpi
	$(PERL) $(srcdir)/make-bf-mpi mpicc bf-gcc OMPI_CC MPICH_CC $(bindir) > $@

bf-mpicxx: $(srcdir)/make-bf-mpi
	$(PERL) $(srcdir)/make-bf-mpi mpicxx bf-g++ OMPI_CXX MPICH_CXX $(bindir) > $@

bf-mpif90: $(srcdir)/make-bf-mpi
	$(PERL) $(srcdir)/make-bf-mpi mpif90 bf-gfortran OMPI_FC MPICH_F90 $(bindir) > $@

bf-mpif77: $(srcdir)/make-bf-mpi
	$(PERL) $(srcdir)/make-bf-mpi mpif77 bf-gfortran OMPI_F77 MPICH_F77 $(bindir) > $@

all-local: wrappers.mak

wrappers.mak: $(srcdir)/Makefile.PL bf-gcc $(srcdir)/lib/ParseGccOpts.pm bf-inst
	$(PERL) $(srcdir)/Makefile.PL $(if $(AM_V_at),,verbose) \
	  PREFIX=$(prefix) FIRST_MAKEFILE=wrappers.mak \
	  INSTALLMAN1DIR=$(mandir)/man1 INSTALLMAN3DIR=$(mandir)/man3 \
	  INSTALLSITEMAN1DIR=$(mandir)/man1 INSTALLSITEMAN3DIR=$(mandir)/man3
	$(Verb) /bin/echo -e '\npm-loc:\n\t@echo $$(INSTALLSITELIB)' >> wrappers.mak

clean-local:
	-if [ -e wrappers.mak ] ; then $(MAKE) -f wrappers.mak clean ; fi

install-exec-local: wrappers.mak
	$(MAKE) -f wrappers.mak \
	  POD2MAN='$$(POD2MAN_EXE) --center=" " --release=v1.0' install

uninstall-local: wrappers.mak
	$(MAKE) -f wrappers.mak uninstall

##===- projects/bytesflops/tools/wrappers/Makefile ------------------------*- Makefile -*-===##

################################################
# Install wrappers for various GCC progams     #
# to facilitate compiling them with Bytesflops #
#					       #
# By Scott Pakin <pakin@lanl.gov>	       #
################################################

#
# Relative path to the top of the source tree.
#
LEVEL=../..

#
# Name all of the scripts we want to install.
#
SCRIPTS = bf-gcc make-bf-mpi bf-inst
EXTRA_DIST = $(SCRIPTS) Makefile.PL lib/ParseGccOpts.pm
GENERATED_SCRIPTS = bf-mpicc bf-mpicxx bf-mpif90 bf-mpif77

include $(LEVEL)/Makefile.common

install:: wrappers.mak $(addprefix $(PROJ_OBJ_DIR)/, $(GENERATED_SCRIPTS))
	$(Echo) Installing $(BuildMode) wrapper scripts
	-$(Verb) $(MAKE) -f wrappers.mak \
	  POD2MAN='$$(POD2MAN_EXE) --center=" " --release=v1.0' install
	$(Verb) $(ScriptInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ScriptInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_OBJ_DIR)/, $(GENERATED_SCRIPTS))

clean-local::
	-$(Verb) if [ -e wrappers.mak ] ; then $(MAKE) -f wrappers.mak clean ; fi
	-$(Verb) $(RM) -f bf-gccgo bf-gfortran bf-g++ bf-gcc
	-$(Verb) $(RM) -f $(GENERATED_SCRIPTS)
	-$(Verb) $(RM) -f Makefile.PL wrappers.mak wrappers.mak.old
	-$(Verb) $(RM) -rf lib

# ----------------------------------------------------------------------

wrappers.mak: $(PROJ_OBJ_DIR)/Makefile.PL $(PROJ_OBJ_DIR)/bf-gcc $(PROJ_OBJ_DIR)/lib/ParseGccOpts.pm $(PROJ_OBJ_DIR)/bf-inst
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/Makefile.PL $(if $(VERBOSE),verbose) PREFIX=$(PROJ_prefix) FIRST_MAKEFILE=wrappers.mak

$(PROJ_OBJ_DIR)/Makefile.PL: $(PROJ_SRC_DIR)/Makefile.PL
	$(Verb) cp $< $@

$(PROJ_OBJ_DIR)/bf-gcc: $(PROJ_SRC_DIR)/bf-gcc
	$(Verb) cat $< | \
	  $(SED) -e 's,@DRAGONEGG@,$(DRAGONEGG),' \
		 -e 's,@PROJ_libdir@,$(PROJ_libdir),' \
		 -e 's,@LLVM_INST_LIBDIR@,$(LLVM_INST_LIBDIR),' > $@

$(PROJ_OBJ_DIR)/bf-inst: $(PROJ_SRC_DIR)/bf-inst
	$(Verb) cat $< | \
	  $(SED) -e 's,@DRAGONEGG@,$(DRAGONEGG),' \
		 -e 's,@PROJ_libdir@,$(PROJ_libdir),' \
		 -e 's,@LLVM_INST_LIBDIR@,$(LLVM_INST_LIBDIR),' > $@

$(PROJ_OBJ_DIR)/lib/ParseGccOpts.pm: $(PROJ_SRC_DIR)/lib/ParseGccOpts.pm
	-$(Verb) mkdir lib
	$(Verb) cp $< $@

# ----------------------------------------------------------------------

$(PROJ_OBJ_DIR)/bf-mpicc: $(PROJ_SRC_DIR)/make-bf-mpi
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/make-bf-mpi mpicc bf-gcc OMPI_CC MPICH_CC $(PROJ_bindir) > $@

$(PROJ_OBJ_DIR)/bf-mpicxx: $(PROJ_SRC_DIR)/make-bf-mpi
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/make-bf-mpi mpicxx bf-g++ OMPI_CXX MPICH_CXX $(PROJ_bindir) > $@

$(PROJ_OBJ_DIR)/bf-mpif90: $(PROJ_SRC_DIR)/make-bf-mpi
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/make-bf-mpi mpif90 bf-gfortran OMPI_FC MPICH_F90 $(PROJ_bindir) > $@

$(PROJ_OBJ_DIR)/bf-mpif77: $(PROJ_SRC_DIR)/make-bf-mpi
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/make-bf-mpi mpif77 bf-gfortran OMPI_F77 MPICH_F77 $(PROJ_bindir) > $@

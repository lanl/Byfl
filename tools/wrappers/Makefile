##===- projects/bytesflops/tools/wrappers/Makefile ------------------------*- Makefile -*-===##

################################################
# Install wrappers for various GCC progams     #
# to facilitate compiling them with Bytesflops #
#				               #
# By Scott Pakin <pakin@lanl.gov>              #
################################################

#
# Relative path to the top of the source tree.
#
LEVEL=../..

#
# Name all of the scripts we want to install.
#
SCRIPTS = bf-gcc
EXTRA_DIST = $(SCRIPTS) Makefile.PL lib/ParseGccOpts.pm

include $(LEVEL)/Makefile.common

install:: wrappers.mak
	$(Echo) Installing $(BuildMode) $(SCRIPTS)
	-$(VERB) $(MAKE) -f wrappers.mak install

wrappers.mak: $(PROJ_OBJ_DIR)/Makefile.PL $(PROJ_OBJ_DIR)/bf-gcc $(PROJ_OBJ_DIR)/lib/ParseGccOpts.pm
	$(Verb) $(PERL) $(PROJ_SRC_DIR)/Makefile.PL $(if $(VERBOSE),verbose) PREFIX=$(PROJ_prefix) FIRST_MAKEFILE=wrappers.mak

$(PROJ_OBJ_DIR)/Makefile.PL: $(PROJ_SRC_DIR)/Makefile.PL
	$(Verb) cp $< $@

$(PROJ_OBJ_DIR)/bf-gcc: $(PROJ_SRC_DIR)/bf-gcc
	$(Verb) cat $< | \
	  $(SED) -e 's,@DRAGONEGG@,$(DRAGONEGG),' \
	  	 -e 's,@PROJ_libdir@,$(PROJ_libdir),' \
		 -e 's,@LLVM_INST_LIBDIR@,$(LLVM_INST_LIBDIR),' > $@

$(PROJ_OBJ_DIR)/lib/ParseGccOpts.pm: $(PROJ_SRC_DIR)/lib/ParseGccOpts.pm
	-$(Verb) mkdir lib
	$(Verb) cp $< $@

clean-local::
	-$(Verb) if [ -e wrappers.mak ] ; then $(MAKE) -f wrappers.mak clean ; fi
	-$(Verb) $(RM) -f bf-gfortran bf-g++ bf-gcc
	-$(Verb) $(RM) -f Makefile.PL wrappers.mak wrappers.mak.old
	-$(Verb) $(RM) -rf lib

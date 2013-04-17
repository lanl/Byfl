##===- projects/bytesflops/tools/postproc/Makefile ------------------------*- Makefile -*-===##

################################################
# Install various postprocessing scripts that  #
# convert Bytesflops output to various formats #
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
SCRIPTS = bf2cgrind bf2hpctk bfmerge
EXTRA_DIST = $(SCRIPTS)

include $(LEVEL)/Makefile.common

install::
	$(Echo) Installing $(BuildMode) $(SCRIPTS)
	$(Verb) $(ScriptInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ScriptInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_SRC_DIR)/,$(SCRIPTS))

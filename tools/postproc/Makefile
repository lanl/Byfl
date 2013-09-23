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

bf2mysql: $(addprefix $(PROJ_SRC_DIR)/,bf2mysql.cpp)
	$(CXX) -L/usr/lib64 -lmysqlcppconn -I$(PROJ_SRC_ROOT)/lib/include $(addprefix $(PROJ_SRC_DIR)/,bf2mysql.cpp)	-o bf2mysql
	
install:: bf2mysql
	$(Echo) Installing $(BuildMode) $(SCRIPTS)
	$(Verb) $(ScriptInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ScriptInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_SRC_DIR)/,$(SCRIPTS))
	$(Echo) Installing $(BuildMode) bf2mysql
	$(Verb) $(ProgInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ProgInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_OBJ_DIR)/,bf2mysql)

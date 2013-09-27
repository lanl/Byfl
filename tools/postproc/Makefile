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
include $(LEVEL)/Makefile.db

ifeq ($(ENABLE_BYFL_DB), 1)
bf2mysql: $(addprefix $(PROJ_SRC_DIR)/,bf2mysql.cpp)
	$(CXX) -L$(MYSQLCPPCONN_LIBDIR) -lmysqlcppconn -I$(PROJ_SRC_ROOT)/lib/include $(addprefix $(PROJ_SRC_DIR)/,bf2mysql.cpp)	-o bf2mysql
BF_DB_TOOLS = bf2mysql
else
BF_DB_TOOLS =
endif
	
install:: $(BF_DB_TOOLS)
	$(Echo) Installing $(BuildMode) $(SCRIPTS)
	$(Verb) $(ScriptInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ScriptInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_SRC_DIR)/,$(SCRIPTS))
ifeq ($(ENABLE_BYFL_DB), 1)
	$(Echo) Installing $(BuildMode) $(BF_DB_TOOLS)
	$(Verb) $(ProgInstall) -d $(DESTDIR)$(PROJ_bindir)
	$(Verb) $(ProgInstall) -t $(DESTDIR)$(PROJ_bindir) $(addprefix $(PROJ_OBJ_DIR)/,bf2mysql)
endif

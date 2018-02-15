######################################
# Build the libbyfl run-time library #
#				     #
# By Scott Pakin <pakin@lanl.gov>    #
######################################

lib_LTLIBRARIES = libbyfl.la
libbyfl_la_SOURCES = \
	basicblocks.cpp \
	binaryoutput.cpp \
	binaryoutput.h \
	byfl.cpp \
	byfl.h \
	cache-model.cpp \
	cachemap.h \
	callstack.cpp \
	callstack.h \
	datastructs.cpp \
	pagetable.cpp \
	pagetable.h \
	reuse-dist.cpp \
	strides.cpp \
	symtable.cpp \
	tallybytes.cpp \
	threading.cpp \
	ubytes.cpp \
	vectors.cpp \
	papi_sde_interface.c \
	papi_sde_interface.h \
	papi_sde.c \
	papi_sde.h
nodist_libbyfl_la_SOURCES = opcode2name.cpp
BUILT_SOURCES = opcode2name.cpp

libbyfl_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbyfl_la_LDFLAGS = -version-info 0:0:0

CLEANFILES = $(BUILT_SOURCES)

# Specify how to create opcode2name.cpp.
opcode2name.cpp: $(top_srcdir)/gen_opcode2name
	$(AM_V_GEN) $(PERL) $(top_srcdir)/gen_opcode2name '$(CPP) $(CPPFLAGS)' CPP > opcode2name.cpp

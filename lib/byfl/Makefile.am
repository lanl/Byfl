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
	opcode2name.cpp \
	reuse-dist.cpp \
	strides.cpp \
	symtable.cpp \
	tallybytes.cpp \
	threading.cpp \
	ubytes.cpp \
	vectors.cpp

libbyfl_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbyfl_la_LDFLAGS = -version-info 0:0:0

BUILT_SOURCES = opcode2name.cpp
CLEANFILES = $(BUILT_SOURCES)

# Specify how to create opcode2name.cpp.
opcode2name.cpp: $(top_srcdir)/gen_opcode2name
	$(AM_V_GEN) $(PERL) $(top_srcdir)/gen_opcode2name '$(CPP) $(CPPFLAGS)' CPP > $(srcdir)/opcode2name.cpp

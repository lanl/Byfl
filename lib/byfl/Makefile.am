######################################
# Build the libbyfl run-time library #
#				     #
# By Scott Pakin <pakin@lanl.gov>    #
######################################

lib_LTLIBRARIES = libbyfl.la
libbyfl_la_SOURCES = \
	byfl.cpp \
	reuse-dist.cpp \
	symtable.cpp \
	threading.cpp \
	ubytes.cpp \
	vectors.cpp \
	tallybytes.cpp \
	cache-model.cpp \
	opcode2name.cpp \
	opcode2name.h \
	byfl.h \
	cachemap.h
libbyfl_la_CPPFLAGS = -I$(srcdir)/../include
libbyfl_la_LDFLAGS = -version-info 0:0:0

BUILT_SOURCES = opcode2name.cpp opcode2name.h
EXTRA_DIST = gen_opcode2name
CLEANFILES = $(BUILT_SOURCES)

# Specify how to create opcode2name.cpp and opcode2name.h
opcode2name.cpp opcode2name.h: $(srcdir)/gen_opcode2name
	$(AM_V_GEN) $(PERL) $(srcdir)/gen_opcode2name '$(CPP) $(CPPFLAGS)' $(srcdir)/opcode2name.cpp $(srcdir)/opcode2name.h

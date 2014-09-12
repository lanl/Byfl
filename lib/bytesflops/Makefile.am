###################################
# Build the LLVM bytesflops pass  #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

lib_LTLIBRARIES = libbytesflops.la
libbytesflops_la_SOURCES = \
	bytesflops.cpp \
	instrument.cpp \
	helpers.cpp \
	init.cpp \
	bytesflops.h
libbytesflops_la_CPPFLAGS = -I$(srcdir)/../include
libbytesflops_la_LDFLAGS = -version-info 0:0:0

install-exec-hook:
	$(INSTALL) $(DESTDIR)$(libdir)/libbytesflops.so $(DESTDIR)$(libdir)/bytesflops.so
	$(RM) $(DESTDIR)$(libdir)/libbytesflops.so*
	$(RM) $(DESTDIR)$(libdir)/libbytesflops.{a,la}

uninstall-local:
	$(RM) $(DESTDIR)$(libdir)/bytesflops.so


# Force a shared-library build even if the user specified
# --disable-shared to the configure script.

libtool: $(top_builddir)/libtool
	$(SED) -e 's/build_libtool_libs=no/build_libtool_libs=yes/' < $(top_builddir)/libtool > $(builddir)/libtool

BUILT_SOURCES = libtool
LIBTOOL = $(SHELL) $(builddir)/libtool
CLEANFILES = $(BUILT_SOURCES)

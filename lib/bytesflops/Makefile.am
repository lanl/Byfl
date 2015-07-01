###################################
# Build the LLVM bytesflops pass  #
#				  #
# By Scott Pakin <pakin@lanl.gov> #
###################################

plugin_LTLIBRARIES = bytesflops.la
bytesflops_la_SOURCES = \
	bytesflops.cpp \
	instrument.cpp \
	helpers.cpp \
	init.cpp \
	bytesflops.h \
	mersennetwister.cpp \
	mersennetwister.h \
	functionkeygen.cpp \
	functionkeygen.h \
	symbolinfo.cpp
bytesflops_la_CPPFLAGS = -I$(top_srcdir)/include
bytesflops_la_LDFLAGS = -module -avoid-version

# Because bytesflops.so is a module, not an ordinary shared library one
# explicitly links against, the Libtool .la file is, as far as I can
# tell, useless.  Remove it.

install-data-hook:
	$(RM) "$(DESTDIR)$(plugindir)/bytesflops.la"
	$(RM) "$(DESTDIR)$(plugindir)/bytesflops.a"

uninstall-hook:
	$(RM) "$(DESTDIR)$(plugindir)/bytesflops.so"

# Force a shared-library build even if the user specified
# --disable-shared to the configure script.

libtool: $(top_builddir)/libtool
	$(SED) -e 's/build_libtool_libs=no/build_libtool_libs=yes/' < $(top_builddir)/libtool > $(builddir)/libtool

BUILT_SOURCES = libtool
LIBTOOL = $(SHELL) $(builddir)/libtool
CLEANFILES = $(BUILT_SOURCES)

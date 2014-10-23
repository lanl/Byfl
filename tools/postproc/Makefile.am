###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

CXX = $(H5CXX)

dist_bin_SCRIPTS = bf2cgrind bf2hpctk bf-parse-cache-dump
bin_PROGRAMS =
dist_man1_MANS =
EXTRA_DIST = bfbin2sqlite3.rst

lib_LTLIBRARIES = libbfbin.la
libbfbin_la_SOURCES = parsebfbin.c bfbin.h
libbfbin_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbfbin_la_LDFLAGS = -version-info 0:0:0
pkginclude_HEADERS = bfbin.h

if HDF5_AVAILABLE
bin_PROGRAMS += bfbin2hdf5
endif
bfbin2hdf5_SOURCES = bfbin2hdf5.cpp
bfbin2hdf5_CPPFLAGS = -I$(top_srcdir)/include
bfbin2hdf5_CXXFLAGS = $(H5CXXFLAGS)
bfbin2hdf5_LDADD = libbfbin.la

if GETOPT_LONG_AVAILABLE
bin_PROGRAMS += bfbin2text
endif
bfbin2text_SOURCES = bfbin2text.cpp
bfbin2text_CPPFLAGS = -I$(top_srcdir)/include
bfbin2text_LDADD = libbfbin.la

if SQLITE3_AVAILABLE
bin_PROGRAMS += bfbin2sqlite3
dist_man1_MANS += bfbin2sqlite3.1
endif
bfbin2sqlite3_SOURCES = bfbin2sqlite3.cpp
bfbin2sqlite3_CPPFLAGS = -I$(top_srcdir)/include
bfbin2sqlite3_LDADD = libbfbin.la

if RST2MAN_AVAILABLE
%.1: %.rst
	$(RST2MAN) $< $@
endif

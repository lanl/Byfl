###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

CXX = $(H5CXX)

dist_bin_SCRIPTS = bf2cgrind bf2hpctk bf-parse-cache-dump
bin_PROGRAMS =
man1_MANS =
CLEANFILES =
EXTRA_DIST = bfbin2hdf5.pod bfbin2csv.pod bfbin2sqlite3.pod

lib_LTLIBRARIES = libbfbin.la
libbfbin_la_SOURCES = parsebfbin.c bfbin.h
libbfbin_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbfbin_la_LDFLAGS = -version-info 0:0:0
pkginclude_HEADERS = bfbin.h

if HDF5_AVAILABLE
bin_PROGRAMS += bfbin2hdf5
man1_MANS += bfbin2hdf5.1
CLEANFILES += bfbin2hdf5.1
endif
bfbin2hdf5_SOURCES = bfbin2hdf5.cpp
bfbin2hdf5_CPPFLAGS = -I$(top_srcdir)/include
bfbin2hdf5_CXXFLAGS = $(H5CXXFLAGS)
bfbin2hdf5_LDADD = libbfbin.la

if GETOPT_LONG_AVAILABLE
bin_PROGRAMS += bfbin2csv
man1_MANS += bfbin2csv.1
CLEANFILES += bfbin2csv.1
endif
bfbin2csv_SOURCES = bfbin2csv.cpp
bfbin2csv_CPPFLAGS = -I$(top_srcdir)/include
bfbin2csv_LDADD = libbfbin.la

if SQLITE3_AVAILABLE
bin_PROGRAMS += bfbin2sqlite3
man1_MANS += bfbin2sqlite3.1
CLEANFILES += bfbin2sqlite3.1
endif
bfbin2sqlite3_SOURCES = bfbin2sqlite3.cpp
bfbin2sqlite3_CPPFLAGS = -I$(top_srcdir)/include
bfbin2sqlite3_LDADD = libbfbin.la

%.1: %.pod
	$(POD2MAN) --section=1 --release="$(MAN_RELEASE)" --center="$(MAN_CATEGORY)" $< $@

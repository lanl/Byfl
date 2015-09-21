###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

CXX = $(H5CXX)

dist_bin_SCRIPTS = bf-parse-cache-dump
bin_PROGRAMS =
man1_MANS =
man3_MANS = bf_process_byfl_file.3
CLEANFILES = bf_process_byfl_file.3
EXTRA_DIST = \
	bf_process_byfl_file.pod \
	bfbin2cgrind.pod \
	bfbin2csv.pod \
	bfbin2hdf5.pod \
	bfbin2hpctk.pod \
	bfbin2sqlite3.pod \
	bfbin2xmlss.pod \
	bfbin2py.in
lib_LTLIBRARIES =
pkginclude_HEADERS =

lib_LTLIBRARIES += libbfbin.la
libbfbin_la_SOURCES = parsebfbin.c bfbin.h
libbfbin_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbfbin_la_LDFLAGS = -version-info 1:0:0
pkginclude_HEADERS += bfbin.h

lib_LTLIBRARIES += libbfbin-swig.la
libbfbin_swig_la_SOURCES = parsebfbin-swig.c bfbin-swig.h bfbin.h
libbfbin_swig_la_CPPFLAGS = -I$(top_srcdir)/include -I$(srcdir)/../include
libbfbin_swig_la_LDFLAGS = -version-info 1:0:0
libbfbin_swig_la_LIBADD = libbfbin.la -lpthread
pkginclude_HEADERS += bfbin-swig.h

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

bin_PROGRAMS += bfbin2xmlss
man1_MANS += bfbin2xmlss.1
CLEANFILES += bfbin2xmlss.1
bfbin2xmlss_SOURCES = bfbin2xmlss.cpp
bfbin2xmlss_CPPFLAGS = -I$(top_srcdir)/include
bfbin2xmlss_LDADD = libbfbin.la

bin_PROGRAMS += bfbin2hpctk
man1_MANS += bfbin2hpctk.1
CLEANFILES += bfbin2hpctk.1
bfbin2hpctk_SOURCES = bfbin2hpctk.cpp bfbin2hpctk.h
bfbin2hpctk_CPPFLAGS = -I$(top_srcdir)/include
bfbin2hpctk_LDADD = libbfbin.la

bin_PROGRAMS += bfbin2cgrind
man1_MANS += bfbin2cgrind.1
CLEANFILES += bfbin2cgrind.1
bfbin2cgrind_SOURCES = bfbin2cgrind.cpp
bfbin2cgrind_CPPFLAGS = -I$(top_srcdir)/include
bfbin2cgrind_LDADD = libbfbin.la

if SQLITE3_AVAILABLE
bin_PROGRAMS += bfbin2sqlite3
man1_MANS += bfbin2sqlite3.1
CLEANFILES += bfbin2sqlite3.1
endif
bfbin2sqlite3_SOURCES = bfbin2sqlite3.cpp
bfbin2sqlite3_CPPFLAGS = -I$(top_srcdir)/include
bfbin2sqlite3_LDADD = libbfbin.la -lsqlite3

exampledir = $(docdir)/examples
example_SCRIPTS = bfbin2py

%.1: %.pod
	$(POD2MAN) \
	  --section=1 \
	  --release="$(MAN_RELEASE)" \
	  --center="$(MAN_CATEGORY)" \
	  $< $@

%.3: %.pod
	$(POD2MAN) \
	  --section=3 \
	  --release="$(MAN_RELEASE)" \
	  --center="$(MAN_CATEGORY)" \
	  --name="`basename $< .pod | tr a-z A-Z`" \
	  $< $@

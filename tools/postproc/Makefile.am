###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

CXX = $(H5CXX)

dist_bin_SCRIPTS = bf2cgrind bf2hpctk bfmerge bf-parse-cache-dump
bin_PROGRAMS =

if HDF5_AVAILABLE
bin_PROGRAMS += bfbin2hdf5
endif
bfbin2hdf5_SOURCES = bfbin2hdf5.cpp
bfbin2hdf5_CPPFLAGS = -I$(top_srcdir)/include
bfbin2hdf5_CXXFLAGS = $(H5CXXFLAGS)

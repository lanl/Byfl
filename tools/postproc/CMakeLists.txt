###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#                                             #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

# Build the parser for Byfl binary output files.
add_library(bfbin parsebfbin.c bfbin.h)
add_man_from_pod(bf_process_byfl_file.3 bf_process_byfl_file.pod)

# Build the SWIG version of the parser for Byfl binary output files.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(bfbin-swig parsebfbin-swig.c bfbin-swig.h bfbin.h)
target_link_libraries(bfbin-swig bfbin pthread)

# Always build the converters to XML Spreadsheet, HPC Toolkit, and Cachegrind
# formats.
add_postprocessing_tool(bfbin2xmlss)
add_postprocessing_tool(bfbin2hpctk CDEPS bfbin2hpctk.h)
add_postprocessing_tool(bfbin2cgrind)

# If possible, build the converters to CSV and SQLite3 formats.
if (HAVE_GETOPT_LONG)
  add_postprocessing_tool(bfbin2csv)
endif (HAVE_GETOPT_LONG)
if (SQLITE3_FOUND AND HAVE_GETOPT_LONG)
  add_postprocessing_tool(bfbin2sqlite3 LDEPS sqlite3)
endif (SQLITE3_FOUND AND HAVE_GETOPT_LONG)

# If possible, build the converter to HDF5 format.
#
# We need to use the h5c++ compiler wrapper to build bfbin2hdf5, but I don't
# know how to build *only* bfbin2hdf5 using h5c++.  Setting CMAKE_CXX_COMPILER
# below unfortunately affects all files in this directory.
if (HDF5_FOUND)
  set(CMAKE_CXX_COMPILER ${HDF5_CXX_COMPILER_EXECUTABLE})
  add_postprocessing_tool(bfbin2hdf5)
endif (HDF5_FOUND)

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

# Build the converter to XML Spreadsheet format.
add_executable(bfbin2xmlss bfbin2xmlss.cpp)
target_link_libraries(bfbin2xmlss bfbin)
add_man_from_pod(bfbin2xmlss.1 bfbin2xmlss.pod)

# Build the converter to HPC Toolkit format.
add_executable(bfbin2hpctk bfbin2xmlss.cpp bfbin2hpctk.h)
target_link_libraries(bfbin2hpctk bfbin)
add_man_from_pod(bfbin2hpctk.1 bfbin2hpctk.pod)

# Build the converter to Cachegrind format.
add_executable(bfbin2cgrind bfbin2cgrind.cpp)
target_link_libraries(bfbin2cgrind bfbin)
add_man_from_pod(bfbin2cgrind.1 bfbin2cgrind.pod)

# Build the converter to CSV format.
if (HAVE_GETOPT_LONG)
  add_executable(bfbin2csv bfbin2csv.cpp)
  target_link_libraries(bfbin2csv bfbin)
  add_man_from_pod(bfbin2csv.1 bfbin2csv.pod)
endif (HAVE_GETOPT_LONG)

# Build the converter to SQLite3 format.
if (SQLITE3_FOUND AND HAVE_GETOPT_LONG)
  add_executable(bfbin2sqlite3 bfbin2sqlite3.cpp)
  target_link_libraries(bfbin2sqlite3 bfbin sqlite3)
  add_man_from_pod(bfbin2sqlite3.1 bfbin2sqlite3.pod)
endif (SQLITE3_FOUND AND HAVE_GETOPT_LONG)

# Build the converter to HDF5 format.
#
# We need to use the h5c++ compiler wrapper to build bfbin2hdf5, but I don't
# know how to build *only* bfbin2hdf5 using h5c++.  Setting CMAKE_CXX_COMPILER
# below unfortunately affects all files in this directory.
if (HDF5_FOUND)
  set(CMAKE_CXX_COMPILER ${HDF5_CXX_COMPILER_EXECUTABLE})
  add_executable(bfbin2hdf5 bfbin2hdf5.cpp)
  target_link_libraries(bfbin2hdf5 bfbin)
  add_man_from_pod(bfbin2hdf5.1 bfbin2hdf5.pod)
endif()

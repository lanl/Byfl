###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#				              #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

# Build the parser for Byfl binary output files.
add_library(bfbin parsebfbin.c bfbin.h)

# Build the SWIG version of the parser for Byfl binary output files.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(bfbin-swig parsebfbin-swig.c bfbin-swig.h bfbin.h)
target_link_libraries(bfbin-swig bfbin pthread)

# Build the converter to XML Spreadsheet format.
add_executable(bfbin2xmlss bfbin2xmlss.cpp)
target_link_libraries(bfbin2xmlss bfbin)

# Build the converter to HPC Toolkit format.
add_executable(bfbin2hpctk bfbin2xmlss.cpp bfbin2hpctk.h)
target_link_libraries(bfbin2hpctk bfbin)

# Build the converter to Cachegrind format.
add_executable(bfbin2cgrind bfbin2cgrind.cpp)
target_link_libraries(bfbin2cgrind bfbin)

# Build the converter to SQLite3 format.
if (SQLITE3_FOUND)
  add_executable(bfbin2sqlite3 bfbin2sqlite3.cpp)
  target_link_libraries(bfbin2sqlite3 bfbin sqlite3)
endif (SQLITE3_FOUND)

# Build the converter to HDF5 format.
#
# We need to use the h5c++ compiler wrapper to build bfbin2hdf5, but I don't
# know how to build *only* bfbin2hdf5 using h5c++.  Setting CMAKE_CXX_COMPILER
# below unfortunately affects all files in this directory.
if (HDF5_FOUND)
  set(CMAKE_CXX_COMPILER ${HDF5_CXX_COMPILER_EXECUTABLE})
  add_executable(bfbin2hdf5 bfbin2hdf5.cpp)
  target_link_libraries(bfbin2hdf5 bfbin)
endif()

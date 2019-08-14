###############################################
# Install various postprocessing scripts that #
# convert Byfl output to various formats      #
#                                             #
# By Scott Pakin <pakin@lanl.gov>             #
###############################################

# Build and install the parser for Byfl binary output files.
add_library(bfbin parsebfbin.c bfbin.h)
add_man_from_pod(bf_process_byfl_file.3 bf_process_byfl_file.pod)
install(TARGETS bfbin DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES bfbin.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/byfl)

# Build and install the SWIG version of the parser for Byfl binary output
# files.
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_library(bfbin-swig parsebfbin-swig.c bfbin-swig.h bfbin.h)
target_link_libraries(bfbin-swig bfbin pthread)
install(TARGETS bfbin-swig DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES bfbin-swig.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/byfl)

# Always build and install the converters to XML Spreadsheet, HPC Toolkit, and
# Cachegrind formats.
add_postprocessing_tool(bfbin2xmlss)
add_postprocessing_tool(bfbin2hpctk CDEPS bfbin2hpctk.h)
add_postprocessing_tool(bfbin2cgrind)

# If possible, build and install the converters to CSV and SQLite3 formats.
if (HAVE_GETOPT_LONG)
  add_postprocessing_tool(bfbin2csv)
endif (HAVE_GETOPT_LONG)
if (SQLITE3_FOUND AND HAVE_GETOPT_LONG)
  add_postprocessing_tool(bfbin2sqlite3 LDEPS sqlite3)
endif (SQLITE3_FOUND AND HAVE_GETOPT_LONG)

# If possible, build and install the converter to HDF5 format.
if (HDF5_FOUND)
  add_postprocessing_tool(bfbin2hdf5)
  target_compile_definitions(bfbin2hdf5 PRIVATE ${HDF5_CXX_DEFINITIONS})
  target_include_directories(bfbin2hdf5 PRIVATE ${HDF5_CXX_INCLUDE_DIRS})
  target_link_libraries(bfbin2hdf5 ${HDF5_CXX_LIBRARIES})
endif (HDF5_FOUND)

# Generate and install an example of parsing Byfl output from Python using
# SWIG.
configure_file(bfbin2py.in bfbin2py @ONLY)
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/bfbin2py DESTINATION ${CMAKE_INSTALL_DOCDIR}/examples)

# Install the helper tool for (the experimental) Byfl cache simulations.
install(PROGRAMS bf-parse-cache-dump DESTINATION ${CMAKE_INSTALL_BINDIR})
